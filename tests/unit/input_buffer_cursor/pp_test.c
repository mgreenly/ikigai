#include <check.h>
#include <talloc.h>
#include "../../../src/input_buffer_cursor.h"
#include "../../../src/input_buffer.h"
#include "../../../src/format.h"

// Helper: Insert text character by character (ASCII only)
static void insert_text(ik_input_buffer_t *input_buffer, const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++) {
        res_t res = ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)(unsigned char)text[i]);
        ck_assert(is_ok(&res));
    }
}

// Test: ik_pp_cursor with cursor at start
START_TEST(test_pp_cursor_at_start) {
    void *tmp_ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    res_t res = ik_input_buffer_create(tmp_ctx, &input_buffer);
    ck_assert(is_ok(&res));

    insert_text(input_buffer, "Hello World");

    // Move cursor to start
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));

    ik_format_buffer_t *buf = NULL;
    res = ik_format_buffer_create(tmp_ctx, &buf);
    ck_assert(is_ok(&res));

    ik_pp_cursor(input_buffer->cursor, buf, 0);

    const char *output = ik_format_get_string(buf);
    ck_assert(strstr(output, "ik_cursor_t @ ") != NULL);
    ck_assert(strstr(output, "byte_offset: 0\n") != NULL);
    ck_assert(strstr(output, "grapheme_offset: 0\n") != NULL);

    talloc_free(tmp_ctx);
}
END_TEST
// Test: ik_pp_cursor with cursor in middle
START_TEST(test_pp_cursor_in_middle)
{
    void *tmp_ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    res_t res = ik_input_buffer_create(tmp_ctx, &input_buffer);
    ck_assert(is_ok(&res));

    insert_text(input_buffer, "Hello World");

    // Move cursor to start, then right 5 times (after "Hello")
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));
    for (int32_t i = 0; i < 5; i++) {
        res = ik_input_buffer_cursor_right(input_buffer);
        ck_assert(is_ok(&res));
    }

    ik_format_buffer_t *buf = NULL;
    res = ik_format_buffer_create(tmp_ctx, &buf);
    ck_assert(is_ok(&res));

    ik_pp_cursor(input_buffer->cursor, buf, 0);

    const char *output = ik_format_get_string(buf);
    ck_assert(strstr(output, "byte_offset: 5\n") != NULL);
    ck_assert(strstr(output, "grapheme_offset: 5\n") != NULL);

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_cursor with indentation
START_TEST(test_pp_cursor_with_indent)
{
    void *tmp_ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    res_t res = ik_input_buffer_create(tmp_ctx, &input_buffer);
    ck_assert(is_ok(&res));

    insert_text(input_buffer, "Test");

    ik_format_buffer_t *buf = NULL;
    res = ik_format_buffer_create(tmp_ctx, &buf);
    ck_assert(is_ok(&res));

    ik_pp_cursor(input_buffer->cursor, buf, 4);

    const char *output = ik_format_get_string(buf);
    // Check that header is indented with 4 spaces
    ck_assert(strstr(output, "    ik_cursor_t @ ") != NULL);
    // Check that fields are indented with 6 spaces (4 + 2)
    ck_assert(strstr(output, "      byte_offset: ") != NULL);

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_cursor with UTF-8 text
START_TEST(test_pp_cursor_utf8)
{
    void *tmp_ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    res_t res = ik_input_buffer_create(tmp_ctx, &input_buffer);
    ck_assert(is_ok(&res));

    // Insert emoji (4 bytes, 1 grapheme)
    insert_text(input_buffer, "Hello ");
    res = ik_input_buffer_insert_codepoint(input_buffer, 0x1F600); // 😀
    ck_assert(is_ok(&res));
    insert_text(input_buffer, " World");

    // Move to after emoji: byte_offset=10, grapheme_offset=7
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));
    for (int32_t i = 0; i < 7; i++) {
        res = ik_input_buffer_cursor_right(input_buffer);
        ck_assert(is_ok(&res));
    }

    ik_format_buffer_t *buf = NULL;
    res = ik_format_buffer_create(tmp_ctx, &buf);
    ck_assert(is_ok(&res));

    ik_pp_cursor(input_buffer->cursor, buf, 0);

    const char *output = ik_format_get_string(buf);
    ck_assert(strstr(output, "byte_offset: 10\n") != NULL);
    ck_assert(strstr(output, "grapheme_offset: 7\n") != NULL);

    talloc_free(tmp_ctx);
}

END_TEST

// Test suite setup
static Suite *pp_cursor_suite(void)
{
    Suite *s = suite_create("input_buffer_cursor_pp");

    TCase *tc_basic = tcase_create("basic");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_test(tc_basic, test_pp_cursor_at_start);
    tcase_add_test(tc_basic, test_pp_cursor_in_middle);
    tcase_add_test(tc_basic, test_pp_cursor_with_indent);
    tcase_add_test(tc_basic, test_pp_cursor_utf8);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = pp_cursor_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
