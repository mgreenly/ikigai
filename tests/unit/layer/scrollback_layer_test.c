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
// Test: Render starting from middle of wrapped line
START_TEST(test_scrollback_layer_render_partial_start)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create scrollback with width 10
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: "Short" (1 row)
    ik_scrollback_append_line(scrollback, "Short", 5);

    // Line 1: "AAAAAAAAAA" + "BBBBBBBBBB" (20 chars = 2 rows at width 10)
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBB", 20);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render starting at physical row 2 (second row of line 1: "BBBBBBBBBB")
    layer->render(layer, output, 10, 2, 1);

    // Should render "BBBBBBBBBB" + \r\n (because it's end of logical line)
    ck_assert_uint_eq(output->size, 12);  // 10 chars + \r\n
    ck_assert(memcmp(output->data, "BBBBBBBBBB\r\n", 12) == 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render ending in middle of wrapped line (no trailing \r\n)
START_TEST(test_scrollback_layer_render_partial_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: 30 chars = 3 rows at width 10
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBBCCCCCCCCCC", 30);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render only first 2 rows (ending mid-line, before "CCCCCCCCCC")
    layer->render(layer, output, 10, 0, 2);

    // Should render "AAAAAAAAAA" + "BBBBBBBBBB" with NO trailing \r\n
    // (we stopped mid-logical-line)
    ck_assert_uint_eq(output->size, 20);  // 20 chars, no \r\n
    ck_assert(memcmp(output->data, "AAAAAAAAAABBBBBBBBBB", 20) == 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render middle portion of wrapped line (skip start, stop before end)
START_TEST(test_scrollback_layer_render_partial_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: 40 chars = 4 rows at width 10
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDD", 40);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render rows 1-2 (skip AAAA, stop before DDDD)
    layer->render(layer, output, 10, 1, 2);

    // Should render "BBBBBBBBBB" + "CCCCCCCCCC" with NO trailing \r\n
    ck_assert_uint_eq(output->size, 20);
    ck_assert(memcmp(output->data, "BBBBBBBBBBCCCCCCCCCC", 20) == 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render with UTF-8 content (multi-byte chars)
START_TEST(test_scrollback_layer_render_partial_utf8)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 5);

    // "cafe monde" with accented chars
    // "cafe " (6 bytes, 5 cols) + "monde" (5 bytes, 5 cols) = 11 bytes, 10 cols = 2 rows
    ik_scrollback_append_line(scrollback, "caf\xc3\xa9 monde", 11);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render second row only ("monde")
    layer->render(layer, output, 5, 1, 1);

    // Should render "monde\r\n" (end of logical line)
    ck_assert_uint_eq(output->size, 7);  // 5 + \r\n
    ck_assert(memcmp(output->data, "monde\r\n", 7) == 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render with ANSI escape sequences
START_TEST(test_scrollback_layer_render_partial_ansi)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // "\x1b[31m" + "AAAAAAAAAA" + "\x1b[0m" + "BBBBBBBBBB"
    // ANSI: 5 bytes, text: 10, ANSI: 4, text: 10 = 29 bytes, 20 display cols = 2 rows
    ik_scrollback_append_line(scrollback, "\x1b[31mAAAAAAAAAA\x1b[0mBBBBBBBBBB", 29);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render second row ("BBBBBBBBBB")
    layer->render(layer, output, 10, 1, 1);

    // Should render the reset ANSI + "BBBBBBBBBB\r\n"
    // Note: We start at byte offset after first 10 display cols
    // The ANSI reset (\x1b[0m) comes before BBBB, so it should be included
    ck_assert(output->size > 0);
    ck_assert(strstr((char *)output->data, "BBBBBBBBBB") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: Single-row line doesn't break when rendered partially
START_TEST(test_scrollback_layer_render_single_row_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(scrollback, "Short line", 10);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render the single row
    layer->render(layer, output, 80, 0, 1);

    // Should render "Short line\r\n"
    ck_assert_uint_eq(output->size, 12);
    ck_assert(memcmp(output->data, "Short line\r\n", 12) == 0);

    talloc_free(ctx);
}

END_TEST
// Test: Multiple lines with first partial, last partial
START_TEST(test_scrollback_layer_render_multiple_lines_partial)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: 20 chars = 2 rows
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBB", 20);
    // Line 1: 10 chars = 1 row
    ik_scrollback_append_line(scrollback, "CCCCCCCCCC", 10);
    // Line 2: 20 chars = 2 rows
    ik_scrollback_append_line(scrollback, "DDDDDDDDDDEEEEEEEEEE", 20);

    // Physical rows: 0=AAAA, 1=BBBB, 2=CCCC, 3=DDDD, 4=EEEE

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render rows 1-3 (BBBB + CCCC + DDDD)
    layer->render(layer, output, 10, 1, 3);

    // BBBB\r\n (end of line 0) + CCCC\r\n (complete line 1) + DDDD (partial line 2)
    // = 10+2 + 10+2 + 10 = 34 bytes
    const char *expected = "BBBBBBBBBB\r\nCCCCCCCCCC\r\nDDDDDDDDDD";
    ck_assert_uint_eq(output->size, 34);
    ck_assert(memcmp(output->data, expected, 34) == 0);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_layer_suite(void)
{
    Suite *s = suite_create("Scrollback Layer");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_scrollback_layer_create_and_visibility);
    tcase_add_test(tc_core, test_scrollback_layer_height_empty);
    tcase_add_test(tc_core, test_scrollback_layer_height_with_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_empty);
    tcase_add_test(tc_core, test_scrollback_layer_render_with_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_row_count_zero);
    tcase_add_test(tc_core, test_scrollback_layer_render_start_row_beyond_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_newline_conversion);
    tcase_add_test(tc_core, test_scrollback_render_end_row_beyond_content);
    tcase_add_test(tc_core, test_scrollback_layer_render_partial_start);
    tcase_add_test(tc_core, test_scrollback_layer_render_partial_end);
    tcase_add_test(tc_core, test_scrollback_layer_render_partial_middle);
    tcase_add_test(tc_core, test_scrollback_layer_render_partial_utf8);
    tcase_add_test(tc_core, test_scrollback_layer_render_partial_ansi);
    tcase_add_test(tc_core, test_scrollback_layer_render_single_row_line);
    tcase_add_test(tc_core, test_scrollback_layer_render_multiple_lines_partial);
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
