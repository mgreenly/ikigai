// Tests for input layer wrapper
#include "../../../src/layer_wrappers.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_input_layer_create_and_visibility) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "test";
    const char *text_ptr = text;
    size_t text_len = 4;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "input");
    ck_assert(layer->is_visible(layer) == true);

    // Change visibility
    visible = false;
    ck_assert(layer->is_visible(layer) == false);

    talloc_free(ctx);
}
END_TEST START_TEST(test_input_layer_height_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "";
    const char *text_ptr = text;
    size_t text_len = 0;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    // Empty input still occupies 1 row (for cursor)
    ck_assert_uint_eq(layer->get_height(layer, 80), 1);

    talloc_free(ctx);
}

END_TEST START_TEST(test_input_layer_height_single_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "Hello world";
    const char *text_ptr = text;
    size_t text_len = 11;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    // Single line text
    ck_assert_uint_eq(layer->get_height(layer, 80), 1);

    talloc_free(ctx);
}

END_TEST START_TEST(test_input_layer_height_with_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "Line 1\nLine 2";
    const char *text_ptr = text;
    size_t text_len = 13;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    // Two lines (1 newline)
    ck_assert_uint_eq(layer->get_height(layer, 80), 2);

    talloc_free(ctx);
}

END_TEST START_TEST(test_input_layer_height_with_wrapping)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    // Text that wraps at width 10
    const char *text = "12345678901234567890"; // 20 chars
    const char *text_ptr = text;
    size_t text_len = 20;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    // Should wrap to 3 lines at width 10 (10 + 10 chars with wrapping logic)
    ck_assert_uint_eq(layer->get_height(layer, 10), 3);

    talloc_free(ctx);
}

END_TEST START_TEST(test_input_layer_render_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "";
    const char *text_ptr = text;
    size_t text_len = 0;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    layer->render(layer, output, 80, 0, 1);
    // Empty input produces no output
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_input_layer_render_simple_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "Hello";
    const char *text_ptr = text;
    size_t text_len = 5;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    layer->render(layer, output, 80, 0, 1);
    ck_assert_uint_eq(output->size, 5);
    ck_assert_int_eq(memcmp(output->data, "Hello", 5), 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_input_layer_render_with_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    const char *text = "Line1\nLine2";
    const char *text_ptr = text;
    size_t text_len = 11;

    ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    layer->render(layer, output, 80, 0, 2);

    // Newline should be converted to \r\n
    ck_assert_uint_eq(output->size, 12); // "Line1\r\nLine2"
    ck_assert_int_eq(memcmp(output->data, "Line1\r\nLine2", 12), 0);

    talloc_free(ctx);
}

END_TEST

static Suite *input_layer_suite(void)
{
    Suite *s = suite_create("Input Layer");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_input_layer_create_and_visibility);
    tcase_add_test(tc_core, test_input_layer_height_empty);
    tcase_add_test(tc_core, test_input_layer_height_single_line);
    tcase_add_test(tc_core, test_input_layer_height_with_newline);
    tcase_add_test(tc_core, test_input_layer_height_with_wrapping);
    tcase_add_test(tc_core, test_input_layer_render_empty);
    tcase_add_test(tc_core, test_input_layer_render_simple_text);
    tcase_add_test(tc_core, test_input_layer_render_with_newline);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = input_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
