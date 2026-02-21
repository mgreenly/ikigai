#include "tests/test_constants.h"
// Tests for status layer functionality
#include "apps/ikigai/layer_wrappers.h"
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_status_layer_create_and_visibility) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    char *model = talloc_strdup(ctx, "claude-sonnet-4");
    int thinking = IK_THINKING_LOW;

    ik_layer_t *layer = ik_status_layer_create(ctx, "status", &visible, &model, &thinking);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "status");
    ck_assert(layer->is_visible(layer) == true);

    // Change visibility
    visible = false;
    ck_assert(layer->is_visible(layer) == false);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_status_layer_height) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    char *model = talloc_strdup(ctx, "gpt-4");
    int thinking = IK_THINKING_MED;

    ik_layer_t *layer = ik_status_layer_create(ctx, "status", &visible, &model, &thinking);

    // Status layer is always 2 rows (separator + status)
    ck_assert_uint_eq(layer->get_height(layer, 80), 2);
    ck_assert_uint_eq(layer->get_height(layer, 40), 2);
    ck_assert_uint_eq(layer->get_height(layer, 200), 2);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_status_layer_render_with_model) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    char *model = talloc_strdup(ctx, "test-model");
    int thinking = IK_THINKING_LOW;

    ik_layer_t *layer = ik_status_layer_create(ctx, "status", &visible, &model, &thinking);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render status at width 80
    layer->render(layer, output, 80, 0, 2);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain robot emoji
    ck_assert(strstr(output_str, "🤖") != NULL);

    // Should contain model name
    ck_assert(strstr(output_str, "test-model") != NULL);

    // Should contain thinking level "low"
    ck_assert(strstr(output_str, "low") != NULL);

    // Should contain separator characters (box-drawing)
    ck_assert(strstr(output_str, "─") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_status_layer_render_no_model) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    char *model = NULL;
    int thinking = IK_THINKING_MIN;

    ik_layer_t *layer = ik_status_layer_create(ctx, "status", &visible, &model, &thinking);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

    // Render status at width 80
    layer->render(layer, output, 80, 0, 2);

    // Verify output is not empty
    ck_assert(output->size > 0);

    // Convert to string for easier checking
    char *output_str = talloc_strndup(ctx, output->data, output->size);

    // Should contain robot emoji
    ck_assert(strstr(output_str, "🤖") != NULL);

    // Should contain "(no model)" text
    ck_assert(strstr(output_str, "(no model)") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_status_layer_thinking_levels) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    char *model = talloc_strdup(ctx, "model");

    // Test each thinking level
    const char *expected[] = {"min", "low", "medium", "high"};
    int levels[] = {IK_THINKING_MIN, IK_THINKING_LOW, IK_THINKING_MED, IK_THINKING_HIGH};

    for (int i = 0; i < 4; i++) {
        int thinking = levels[i];
        ik_layer_t *layer = ik_status_layer_create(ctx, "status", &visible, &model, &thinking);
        ik_output_buffer_t *output = ik_output_buffer_create(ctx, 500);

        layer->render(layer, output, 80, 0, 2);

        char *output_str = talloc_strndup(ctx, output->data, output->size);
        ck_assert(strstr(output_str, expected[i]) != NULL);

        talloc_free(output);
    }

    talloc_free(ctx);
}
END_TEST

static Suite *status_layer_suite(void) {
    Suite *s = suite_create("status_layer");

    TCase *tc_basic = tcase_create("basic");
    tcase_add_test(tc_basic, test_status_layer_create_and_visibility);
    tcase_add_test(tc_basic, test_status_layer_height);
    tcase_add_test(tc_basic, test_status_layer_render_with_model);
    tcase_add_test(tc_basic, test_status_layer_render_no_model);
    tcase_add_test(tc_basic, test_status_layer_thinking_levels);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = status_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/layer/status_layer_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
