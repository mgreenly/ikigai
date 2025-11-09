/**
 * @file create_test.c
 * @brief Unit tests for workspace creation, clear, and get_text operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Create workspace */
START_TEST(test_workspace_create) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(workspace);

    /* Verify text buffer is empty */
    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at position 0 */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Create workspace - OOM scenarios */
START_TEST(test_workspace_create_oom)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Test OOM during workspace allocation */
    oom_test_fail_next_alloc();
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(workspace);
    oom_test_reset();

    /* Test OOM during byte array allocation (after workspace alloc succeeds) */
    //  Call 1: workspace struct allocation (succeeds)
    //  Call 2: array struct allocation (fails here)
    oom_test_fail_after_n_calls(2);
    res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(workspace);
    oom_test_reset();

    /* Test OOM during cursor allocation (after workspace and byte array succeed) */
    //  Call 1: workspace struct allocation (succeeds)
    //  Call 2: array struct allocation (succeeds)
    //  Call 3: cursor struct allocation (fails here)
    //  Note: array uses lazy allocation, so no data allocation until first append/insert
    oom_test_fail_after_n_calls(3);
    res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(workspace);
    oom_test_reset();

    talloc_free(ctx);
}

END_TEST
/* Test: Get text */
START_TEST(test_workspace_get_text)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Manually add some data */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(workspace->text, test_data[i]);
    }

    /* Get text */
    char *text = NULL;
    size_t len = 0;
    res_t res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 5);
    ck_assert_mem_eq(text, "hello", 5);

    talloc_free(ctx);
}

END_TEST
/* Test: Clear workspace */
START_TEST(test_workspace_clear)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Manually add some data to test clearing */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(workspace->text, test_data[i]);
    }
    workspace->cursor_byte_offset = 3;

    /* Clear the workspace */
    ik_workspace_clear(workspace);

    /* Verify empty */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at 0 */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_workspace_create_null_workspace_out_asserts)
{
    void *ctx = talloc_new(NULL);

    /* workspace_out cannot be NULL - should abort */
    ik_workspace_create(ctx, NULL);

    talloc_free(ctx);
}

END_TEST START_TEST(test_workspace_get_text_null_workspace_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    char *text = NULL;
    size_t len = 0;

    ik_workspace_create(ctx, &workspace);

    /* workspace cannot be NULL */
    ik_workspace_get_text(NULL, &text, &len);

    talloc_free(ctx);
}

END_TEST START_TEST(test_workspace_get_text_null_text_out_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    size_t len = 0;

    ik_workspace_create(ctx, &workspace);

    /* text_out cannot be NULL */
    ik_workspace_get_text(workspace, NULL, &len);

    talloc_free(ctx);
}

END_TEST START_TEST(test_workspace_get_text_null_len_out_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    char *text = NULL;

    ik_workspace_create(ctx, &workspace);

    /* len_out cannot be NULL */
    ik_workspace_get_text(workspace, &text, NULL);

    talloc_free(ctx);
}

END_TEST START_TEST(test_workspace_clear_null_workspace_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_clear(NULL);
}

END_TEST

static Suite *workspace_create_suite(void)
{
    Suite *s = suite_create("Workspace Create");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_create);
    tcase_add_test(tc_core, test_workspace_create_oom);
    tcase_add_test(tc_core, test_workspace_get_text);
    tcase_add_test(tc_core, test_workspace_clear);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_create_null_workspace_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_get_text_null_workspace_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_get_text_null_text_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_get_text_null_len_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_clear_null_workspace_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
