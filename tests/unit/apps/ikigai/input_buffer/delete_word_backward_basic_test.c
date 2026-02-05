/**
 * @file delete_word_backward_basic_test.c
 * @brief Basic unit tests for input_buffer delete_word_backward operation (Ctrl+W)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/input_buffer/core.h"
#include "tests/helpers/test_utils_helper.h"

/* Test: delete_word_backward basic operation */
START_TEST(test_delete_word_backward_basic) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello world test" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 't');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 's');
    ik_input_buffer_insert_codepoint(input_buffer, 't');
    /* Cursor is at end: after "test" */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 16); /* After "hello world test" */
    /* Action: delete word backward (should delete "test") */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello world ", cursor after "world " */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 12);
    ck_assert_mem_eq(result_text, "hello world ", 12);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 12); /* After "hello world " */
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward when cursor is at word boundary */
START_TEST(test_delete_word_backward_at_word_boundary) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello world" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');
    /* Move cursor to after space (before "world") */
    for (int i = 0; i < 5; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 6); /* After "hello " */
    /* Action: delete word backward (should delete space and "hello") */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is "world", cursor at start */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 5);
    ck_assert_mem_eq(result_text, "world", 5);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with multiple spaces */
START_TEST(test_delete_word_backward_multiple_spaces) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello   world" (3 spaces) */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 13); /* After "hello   world" */
    /* Action: delete word backward (should delete "world") */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello   ", cursor after spaces */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 8);
    ck_assert_mem_eq(result_text, "hello   ", 8);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 8);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with punctuation */
START_TEST(test_delete_word_backward_punctuation) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello,world" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, ',');
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 11); /* After "hello,world" */
    /* Action: delete word backward (should delete "world", stop at comma) */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello,", cursor after comma */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 6);
    ck_assert_mem_eq(result_text, "hello,", 6);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with UTF-8 */
START_TEST(test_delete_word_backward_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello 世界" (world in Chinese) */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 0x4E16); /* 世 */
    ik_input_buffer_insert_codepoint(input_buffer, 0x754C); /* 界 */

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 12); /* After "hello 世界" (6 + 1 + 3 + 3) */
    /* Action: delete word backward (should delete "世界") */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello ", cursor after space */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 6);
    ck_assert_mem_eq(result_text, "hello ", 6);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward at start (no-op) */
START_TEST(test_delete_word_backward_at_start) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    /* Move cursor to start */
    for (int i = 0; i < 5; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 0);
    /* Action: delete word backward (should be no-op at start) */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text unchanged */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 5);
    ck_assert_mem_eq(result_text, "hello", 5);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0);
    talloc_free(ctx);
}

END_TEST

static Suite *input_buffer_delete_word_backward_basic_suite(void)
{
    Suite *s = suite_create("Input Buffer Delete Word Backward - Basic");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Basic functionality tests */
    tcase_add_test(tc_core, test_delete_word_backward_basic);
    tcase_add_test(tc_core, test_delete_word_backward_at_word_boundary);
    tcase_add_test(tc_core, test_delete_word_backward_multiple_spaces);
    tcase_add_test(tc_core, test_delete_word_backward_punctuation);
    tcase_add_test(tc_core, test_delete_word_backward_utf8);
    tcase_add_test(tc_core, test_delete_word_backward_at_start);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_delete_word_backward_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input_buffer/delete_word_backward_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
