#include "../../test_constants.h"
// Tests for banner layer functionality
#include "../../../src/layer_wrappers.h"
#include "../../../src/error.h"
#include "../../../src/version.h"
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

static Suite *banner_layer_suite(void) {
    Suite *s = suite_create("banner_layer");

    TCase *tc_basic = tcase_create("basic");
    tcase_add_test(tc_basic, test_banner_layer_create_and_visibility);
    tcase_add_test(tc_basic, test_banner_layer_height);
    tcase_add_test(tc_basic, test_banner_layer_render_content);
    tcase_add_test(tc_basic, test_banner_layer_border_scaling_wide);
    tcase_add_test(tc_basic, test_banner_layer_border_scaling_narrow);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = banner_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/layer/banner_layer_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
