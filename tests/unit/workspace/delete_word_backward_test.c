/**
 * @file delete_word_backward_test.c
 * @brief Unit tests for workspace delete_word_backward operation (Ctrl+W)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: delete_word_backward basic operation */
START_TEST(test_workspace_delete_word_backward_basic) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello world test" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, 't');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 's');
    ik_workspace_insert_codepoint(workspace, 't');
    /* Cursor is at end: after "test" */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 16); /* After "hello world test" */
    /* Action: delete word backward (should delete "test") */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello world ", cursor after "world " */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 12);
    ck_assert_mem_eq(result_text, "hello world ", 12);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 12); /* After "hello world " */
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward when cursor is at word boundary */
START_TEST(test_workspace_delete_word_backward_at_word_boundary)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello world" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');
    /* Move cursor to after space (before "world") */
    for (int i = 0; i < 5; i++) {
        ik_workspace_cursor_left(workspace);
    }
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 6); /* After "hello " */
    /* Action: delete word backward (should delete space and "hello") */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is "world", cursor at start */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 5);
    ck_assert_mem_eq(result_text, "world", 5);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with multiple spaces */
START_TEST(test_workspace_delete_word_backward_multiple_spaces)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello   world" (3 spaces) */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 13); /* After "hello   world" */
    /* Action: delete word backward (should delete "world") */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello   ", cursor after spaces */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 8);
    ck_assert_mem_eq(result_text, "hello   ", 8);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 8);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with punctuation */
START_TEST(test_workspace_delete_word_backward_punctuation)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello,world" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, ',');
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 11); /* After "hello,world" */
    /* Action: delete word backward (should delete "world", stop at comma) */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello,", cursor after comma */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 6);
    ck_assert_mem_eq(result_text, "hello,", 6);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with UTF-8 */
START_TEST(test_workspace_delete_word_backward_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello 世界" (world in Chinese) */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, 0x4E16); /* 世 */
    ik_workspace_insert_codepoint(workspace, 0x754C); /* 界 */

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 12); /* After "hello 世界" (6 + 1 + 3 + 3) */
    /* Action: delete word backward (should delete "世界") */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is "hello ", cursor after space */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 6);
    ck_assert_mem_eq(result_text, "hello ", 6);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward at start (no-op) */
START_TEST(test_workspace_delete_word_backward_at_start)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    /* Move cursor to start */
    for (int i = 0; i < 5; i++) {
        ik_workspace_cursor_left(workspace);
    }
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 0);
    /* Action: delete word backward (should be no-op at start) */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text unchanged */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 5);
    ck_assert_mem_eq(result_text, "hello", 5);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with mixed case and digits */
START_TEST(test_workspace_delete_word_backward_mixed_case_digits)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "Test123 ABC456" */
    ik_workspace_insert_codepoint(workspace, 'T');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 's');
    ik_workspace_insert_codepoint(workspace, 't');
    ik_workspace_insert_codepoint(workspace, '1');
    ik_workspace_insert_codepoint(workspace, '2');
    ik_workspace_insert_codepoint(workspace, '3');
    ik_workspace_insert_codepoint(workspace, ' ');
    ik_workspace_insert_codepoint(workspace, 'A');
    ik_workspace_insert_codepoint(workspace, 'B');
    ik_workspace_insert_codepoint(workspace, 'C');
    ik_workspace_insert_codepoint(workspace, '4');
    ik_workspace_insert_codepoint(workspace, '5');
    ik_workspace_insert_codepoint(workspace, '6');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 14); /* After "Test123 ABC456" */
    /* Action: delete word backward (should delete "ABC456") */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is "Test123 ", cursor after space */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 8);
    ck_assert_mem_eq(result_text, "Test123 ", 8);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 8);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with only non-word characters */
START_TEST(test_workspace_delete_word_backward_only_punctuation)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "..." (only punctuation) */
    ik_workspace_insert_codepoint(workspace, '.');
    ik_workspace_insert_codepoint(workspace, '.');
    ik_workspace_insert_codepoint(workspace, '.');

    /* Cursor at end */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 3);
    /* Action: delete word backward (should delete all punctuation) */
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    /* Assert: text is empty */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 0);
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0);
    talloc_free(ctx);
}

END_TEST
/* Test: delete_word_backward with punctuation boundaries (Bash/readline behavior) */
START_TEST(test_workspace_delete_word_backward_punctuation_boundaries)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_workspace_create(ctx, &workspace);
    /* Insert "hello-world_test.txt" */
    const char *input = "hello-world_test.txt";
    for (size_t i = 0; input[i] != '\0'; i++) {
        ik_workspace_insert_codepoint(workspace, (uint32_t)(unsigned char)input[i]);
    }

    /* Helper to check text after delete */
#define CHECK_DELETE(expected_text, expected_len) \
        do { \
            res_t res = ik_workspace_delete_word_backward(workspace); \
            ck_assert(is_ok(&res)); \
            char *text = NULL; \
            size_t len = 0; \
            res = ik_workspace_get_text(workspace, &text, &len); \
            ck_assert(is_ok(&res)); \
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
START_TEST(test_workspace_delete_word_backward_whitespace_variants)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Test tab whitespace: "hello\tworld" → Ctrl+W → "hello\t" */
    ik_workspace_create(ctx, &workspace);
    const char *tab_input = "hello\tworld";
    for (size_t i = 0; tab_input[i] != '\0'; i++) {
        ik_workspace_insert_codepoint(workspace, (uint32_t)(unsigned char)tab_input[i]);
    }
    res_t res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 6);
    ck_assert_mem_eq(text, "hello\t", 6);
    talloc_free(workspace);

    /* Test CR whitespace: "a\rb" → Ctrl+W → "a\r" */
    ik_workspace_create(ctx, &workspace);
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, '\r');
    ik_workspace_insert_codepoint(workspace, 'b');
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "a\r", 2);
    talloc_free(workspace);

    /* Test newline whitespace: "a\nb" → Ctrl+W → "a\n" */
    ik_workspace_create(ctx, &workspace);
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, '\n');
    ik_workspace_insert_codepoint(workspace, 'b');
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "a\n", 2);
    talloc_free(workspace);

    /* Test whitespace-only: "   " → Ctrl+W → "" */
    ik_workspace_create(ctx, &workspace);
    for (int32_t i = 0; i < 3; i++) {
        ik_workspace_insert_codepoint(workspace, ' ');
    }
    res = ik_workspace_delete_word_backward(workspace);
    ck_assert(is_ok(&res));
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 0);
    talloc_free(ctx);
}

END_TEST
/* Test: NULL workspace should assert */
START_TEST(test_workspace_delete_word_backward_null_workspace_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_delete_word_backward(NULL);
}

END_TEST

static Suite *workspace_delete_word_backward_suite(void)
{
    Suite *s = suite_create("Workspace Delete Word Backward");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_delete_word_backward_basic);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_at_word_boundary);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_multiple_spaces);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_punctuation);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_utf8);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_at_start);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_mixed_case_digits);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_only_punctuation);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_punctuation_boundaries);
    tcase_add_test(tc_core, test_workspace_delete_word_backward_whitespace_variants);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_delete_word_backward_null_workspace_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_delete_word_backward_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
