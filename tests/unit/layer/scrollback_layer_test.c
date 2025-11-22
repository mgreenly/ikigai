// Tests for scrollback layer wrapper
#include "../../../src/layer_wrappers.h"
#include "../../../src/scrollback.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_scrollback_layer_create_and_visibility) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "scrollback");
    // Scrollback is always visible
    ck_assert(layer->is_visible(layer) == true);

    talloc_free(ctx);
}
END_TEST START_TEST(test_scrollback_layer_height_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    // Empty scrollback has 0 height
    size_t height = layer->get_height(layer, 80);
    ck_assert_uint_eq(height, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_layer_height_with_content)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(scrollback, "Line 1", 6);
    ik_scrollback_append_line(scrollback, "Line 2", 6);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    size_t height = layer->get_height(layer, 80);
    ck_assert_uint_eq(height, 2); // 2 lines

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_layer_render_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    layer->render(layer, output, 80, 0, 10);
    // Empty scrollback produces no output
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_layer_render_with_content)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(scrollback, "Hello", 5);
    ik_scrollback_append_line(scrollback, "World", 5);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    layer->render(layer, output, 80, 0, 10);

    // Should contain both lines with \r\n conversions
    ck_assert(output->size > 0);
    ck_assert(strstr((char *)output->data, "Hello") != NULL);
    ck_assert(strstr((char *)output->data, "World") != NULL);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_layer_render_row_count_zero)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(scrollback, "Test", 4);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Request 0 rows
    layer->render(layer, output, 80, 0, 0);
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_layer_render_start_row_beyond_content)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(scrollback, "Test", 4);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Request rendering starting from row 100 (beyond content)
    layer->render(layer, output, 80, 100, 10);
    // Should return OK with no output
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_layer_render_newline_conversion)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);
    // Append line with embedded newline
    ik_scrollback_append_line(scrollback, "Line\nWith\nNewlines", 18);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    layer->render(layer, output, 80, 0, 10);

    // Newlines should be converted to \r\n
    ck_assert(output->size > 0);
    // The output should contain \r\n sequences
    const char *data = (const char *)output->data;
    bool has_crlf = false;
    for (size_t i = 0; i < output->size - 1; i++) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            has_crlf = true;
            break;
        }
    }
    ck_assert(has_crlf);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_render_end_row_beyond_content)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback;
    scrollback = ik_scrollback_create(ctx, 80);
    // Add one line
    ik_scrollback_append_line(scrollback, "Line 1", 6);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Request many more rows than exist (end_physical_row will be way beyond content)
    layer->render(layer, output, 80, 0, 100);
    // Should still succeed, just render what's available

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_layer_suite(void)
{
    Suite *s = suite_create("Scrollback Layer");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_scrollback_layer_create_and_visibility);
    tcase_add_test(tc_core, test_scrollback_layer_height_empty);
    tcase_add_test(tc_core, test_scrollback_layer_height_with_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_empty);
    tcase_add_test(tc_core, test_scrollback_layer_render_with_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_row_count_zero);
    tcase_add_test(tc_core, test_scrollback_layer_render_start_row_beyond_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_newline_conversion);
    tcase_add_test(tc_core, test_scrollback_render_end_row_beyond_content);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = scrollback_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
