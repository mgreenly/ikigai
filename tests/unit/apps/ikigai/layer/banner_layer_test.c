#include "tests/test_constants.h"
// Tests for banner layer functionality
#include "apps/ikigai/layer_wrappers.h"
#include "shared/error.h"
#include "apps/ikigai/version.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_banner_layer_create_and_visibility) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "banner");
    ck_assert(layer->is_visible(layer) == true);

    // Change visibility
    visible = false;
    ck_assert(layer->is_visible(layer) == false);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_height) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);

    // Banner layer is always 6 rows
    ck_assert_uint_eq(layer->get_height(layer, 80), 6);
    ck_assert_uint_eq(layer->get_height(layer, 40), 6);
    ck_assert_uint_eq(layer->get_height(layer, 200), 6);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_render_content) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Render banner at width 80
    layer->render(layer, output, 80, 0, 6);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain owl face elements
    ck_assert(strstr(output_str, "╭") != NULL);  // Eye top-left
    ck_assert(strstr(output_str, "╮") != NULL);  // Eye top-right
    ck_assert(strstr(output_str, "│") != NULL);  // Eye sides
    ck_assert(strstr(output_str, "●") != NULL);  // Pupils
    ck_assert(strstr(output_str, "╰") != NULL);  // Eye/smile bottom-left
    ck_assert(strstr(output_str, "╯") != NULL);  // Eye/smile bottom-right

    // Should contain version text
    ck_assert(strstr(output_str, "Ikigai v") != NULL);
    ck_assert(strstr(output_str, IK_VERSION) != NULL);

    // Should contain tagline
    ck_assert(strstr(output_str, "Agentic Orchestration") != NULL);

    // Should contain border characters (double horizontal)
    ck_assert(strstr(output_str, "═") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_border_scaling_wide) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 2000);

    // Render banner at width 100
    layer->render(layer, output, 100, 0, 6);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should still contain all expected elements
    ck_assert(strstr(output_str, "Ikigai v") != NULL);
    ck_assert(strstr(output_str, "═") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_border_scaling_narrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render banner at narrow width (30 columns)
    layer->render(layer, output, 30, 0, 6);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain owl face elements (these appear early in line)
    ck_assert(strstr(output_str, "╭") != NULL);
    ck_assert(strstr(output_str, "●") != NULL);

    // Should contain border characters (double horizontal)
    ck_assert(strstr(output_str, "═") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_partial_render_middle) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render only rows 2-3 (middle of banner)
    layer->render(layer, output, 80, 2, 2);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain row 2 elements (eyes, pupils, version)
    ck_assert(strstr(output_str, "●") != NULL);
    ck_assert(strstr(output_str, "Ikigai v") != NULL);

    // Should NOT contain row 0 or 5 elements (borders)
    // The output should have exactly 2 lines (rows 2-3), not 6
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 2);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_partial_render_top) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Render only rows 0-2 (top of banner)
    layer->render(layer, output, 80, 0, 3);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Should have exactly 3 lines
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 3);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_partial_render_bottom) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Render only rows 4-5 (bottom of banner)
    layer->render(layer, output, 80, 4, 2);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain row 4 (smile bottom) and row 5 (bottom border)
    ck_assert(strstr(output_str, "═") != NULL);

    // Should have exactly 2 lines
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 2);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_partial_render_single_row) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render only row 3
    layer->render(layer, output, 80, 3, 1);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain row 3 elements (eye bottoms and tagline)
    ck_assert(strstr(output_str, "Agentic Orchestration") != NULL);

    // Should have exactly 1 line
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 1);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_skip_row_0) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render rows 1-5, skipping row 0 (top border)
    layer->render(layer, output, 80, 1, 5);

    // Should have 5 lines (rows 1-5)
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 5);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_skip_row_1) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render only row 0, skipping rows 1-5
    layer->render(layer, output, 80, 0, 1);

    // Should have 1 line (row 0)
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 1);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_skip_row_2) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render rows 0-1, skipping rows 2-5
    layer->render(layer, output, 80, 0, 2);

    // Should have 2 lines (rows 0-1)
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 2);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_skip_row_4) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render rows 0-3, skipping rows 4-5
    layer->render(layer, output, 80, 0, 4);

    // Should have 4 lines (rows 0-3)
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 4);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_skip_row_5) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render rows 0-4, skipping row 5 (bottom border)
    layer->render(layer, output, 80, 0, 5);

    // Should have 5 lines (rows 0-4)
    size_t line_count = 0;
    for (size_t i = 0; i < output->size; i++) {
        if (output->data[i] == '\n') line_count++;
    }
    ck_assert_uint_eq(line_count, 5);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_render_zero_rows_at_start) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    // Render 0 rows starting at row 0 (edge case)
    layer->render(layer, output, 80, 0, 0);

    // Output should be empty
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_render_zero_rows_at_middle) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    // Render 0 rows starting at row 4 (edge case)
    layer->render(layer, output, 80, 4, 0);

    // Output should be empty
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_banner_layer_render_zero_rows_at_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;

    ik_layer_t *layer = ik_banner_layer_create(ctx, "banner", &visible);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    // Render 0 rows starting at row 5 (edge case)
    layer->render(layer, output, 80, 5, 0);

    // Output should be empty
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}
END_TEST

static Suite *banner_layer_suite(void) {
    Suite *s = suite_create("banner_layer");

    TCase *tc_basic = tcase_create("basic");
    tcase_add_test(tc_basic, test_banner_layer_create_and_visibility);
    tcase_add_test(tc_basic, test_banner_layer_height);
    tcase_add_test(tc_basic, test_banner_layer_render_content);
    tcase_add_test(tc_basic, test_banner_layer_border_scaling_wide);
    tcase_add_test(tc_basic, test_banner_layer_border_scaling_narrow);
    tcase_add_test(tc_basic, test_banner_layer_partial_render_middle);
    tcase_add_test(tc_basic, test_banner_layer_partial_render_top);
    tcase_add_test(tc_basic, test_banner_layer_partial_render_bottom);
    tcase_add_test(tc_basic, test_banner_layer_partial_render_single_row);
    tcase_add_test(tc_basic, test_banner_layer_skip_row_0);
    tcase_add_test(tc_basic, test_banner_layer_skip_row_1);
    tcase_add_test(tc_basic, test_banner_layer_skip_row_2);
    tcase_add_test(tc_basic, test_banner_layer_skip_row_4);
    tcase_add_test(tc_basic, test_banner_layer_skip_row_5);
    tcase_add_test(tc_basic, test_banner_layer_render_zero_rows_at_start);
    tcase_add_test(tc_basic, test_banner_layer_render_zero_rows_at_middle);
    tcase_add_test(tc_basic, test_banner_layer_render_zero_rows_at_end);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = banner_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/layer/banner_layer_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
