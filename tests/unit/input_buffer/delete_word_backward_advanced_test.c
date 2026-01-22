/**
 * @file delete_word_backward_advanced_test.c
 * @brief Advanced unit tests for delete_word_backward - edge cases and complex scenarios
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/* Test: delete_word_backward with mixed case and digits */
START_TEST(test_delete_word_backward_mixed_case_digits) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "Test123 ABC456" */
    ik_input_buffer_insert_codepoint(input_buffer, 'T');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 's');
    ik_input_buffer_insert_codepoint(input_buffer, 't');
    ik_input_buffer_insert_codepoint(input_buffer, '1');
    ik_input_buffer_insert_codepoint(input_buffer, '2');
    ik_input_buffer_insert_codepoint(input_buffer, '3');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 'A');
    ik_input_buffer_insert_codepoint(input_buffer, 'B');
    ik_input_buffer_insert_codepoint(input_buffer, 'C');
    ik_input_buffer_insert_codepoint(input_buffer, '4');
    ik_input_buffer_insert_codepoint(input_buffer, '5');
    ik_input_buffer_insert_codepoint(input_buffer, '6');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 14); /* After "Test123 ABC456" */
    /* Action: delete word backward (should delete "ABC456") */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is "Test123 ", cursor after space */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 8);
    ck_assert_mem_eq(result_text, "Test123 ", 8);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 8);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with only non-word characters */
START_TEST(test_delete_word_backward_only_punctuation) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "..." (only punctuation) */
    ik_input_buffer_insert_codepoint(input_buffer, '.');
    ik_input_buffer_insert_codepoint(input_buffer, '.');
    ik_input_buffer_insert_codepoint(input_buffer, '.');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 3);
    /* Action: delete word backward (should delete all punctuation) */
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    /* Assert: text is empty */
    size_t result_len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 0);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with punctuation boundaries (Bash/readline behavior) */
START_TEST(test_delete_word_backward_punctuation_boundaries) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    input_buffer = ik_input_buffer_create(ctx);
    /* Insert "hello-world_test.txt" */
    const char *input = "hello-world_test.txt";
    for (size_t i = 0; input[i] != '\0'; i++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)(unsigned char)input[i]);
    }

    /* Helper to check text after delete */
#define CHECK_DELETE(expected_text, expected_len) \
        do { \
            res_t res = ik_input_buffer_delete_word_backward(input_buffer); \
            ck_assert(is_ok(&res)); \
            size_t len = 0; \
            const char *text = ik_input_buffer_get_text(input_buffer, &len); \
            ck_assert_uint_eq(len, expected_len); \
            ck_assert_mem_eq(text, expected_text, expected_len); \
        } while (0)

    /* Verify each Ctrl+W deletes one "unit" (word or punctuation) */
    CHECK_DELETE("hello-world_test.", 17);  /* Delete "txt" */
    CHECK_DELETE("hello-world_test", 16);   /* Delete "." */
    CHECK_DELETE("hello-world_", 12);       /* Delete "test" */
    CHECK_DELETE("hello-world", 11);        /* Delete "_" */

#undef CHECK_DELETE
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with various whitespace (tab, CR, space-only) */
START_TEST(test_delete_word_backward_whitespace_variants) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    /* Test tab whitespace: "hello\tworld" → Ctrl+W → "hello\t" */
    input_buffer = ik_input_buffer_create(ctx);
    const char *tab_input = "hello\tworld";
    for (size_t i = 0; tab_input[i] != '\0'; i++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)(unsigned char)tab_input[i]);
    }
    res_t res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 6);
    ck_assert_mem_eq(text, "hello\t", 6);
    talloc_free(input_buffer);

    /* Test CR whitespace: "a\rb" → Ctrl+W → "a\r" */
    input_buffer = ik_input_buffer_create(ctx);
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, '\r');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "a\r", 2);
    talloc_free(input_buffer);

    /* Test newline whitespace: "a\nb" → Ctrl+W → "a\n" */
    input_buffer = ik_input_buffer_create(ctx);
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, '\n');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "a\n", 2);
    talloc_free(input_buffer);

    /* Test whitespace-only: "   " → Ctrl+W → "" */
    input_buffer = ik_input_buffer_create(ctx);
    for (int32_t i = 0; i < 3; i++) {
        ik_input_buffer_insert_codepoint(input_buffer, ' ');
    }
    res = ik_input_buffer_delete_word_backward(input_buffer);
    ck_assert(is_ok(&res));
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);
    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL input_buffer should assert */
START_TEST(test_delete_word_backward_null_input_buffer_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_delete_word_backward(NULL);
}

END_TEST
#endif

static Suite *input_buffer_delete_word_backward_advanced_suite(void)
{
    Suite *s = suite_create("Input Buffer Delete Word Backward - Advanced");
    TCase *tc_advanced = tcase_create("Advanced");
    tcase_set_timeout(tc_advanced, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_advanced, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_advanced, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_advanced, IK_TEST_TIMEOUT);

    /* Advanced edge case tests */
    tcase_add_test(tc_advanced, test_delete_word_backward_mixed_case_digits);
    tcase_add_test(tc_advanced, test_delete_word_backward_only_punctuation);
    tcase_add_test(tc_advanced, test_delete_word_backward_punctuation_boundaries);
    tcase_add_test(tc_advanced, test_delete_word_backward_whitespace_variants);

    suite_add_tcase(s, tc_advanced);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_delete_word_backward_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_delete_word_backward_advanced_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input_buffer/delete_word_backward_advanced_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
