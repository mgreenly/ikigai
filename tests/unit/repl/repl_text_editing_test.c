/**
 * @file repl_text_editing_test.c
 * @brief Unit tests for REPL text editing actions
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

/* Test: Process CHAR action */
START_TEST(test_repl_process_action_char) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'a'};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);
    ck_assert_int_eq(repl->quit, false);

    talloc_free(ctx);
}
END_TEST
/* Test: Process NEWLINE action */
START_TEST(test_repl_process_action_newline)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'i');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_INSERT_NEWLINE};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'h');
    ck_assert_int_eq(text[1], 'i');
    ck_assert_int_eq(text[2], '\n');

    talloc_free(ctx);
}

END_TEST
/* Test: Process BACKSPACE action */
START_TEST(test_repl_process_action_backspace)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'c');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_BACKSPACE};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'b');

    talloc_free(ctx);
}

END_TEST
/* Test: Process DELETE action */
START_TEST(test_repl_process_action_delete)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'c');
    ck_assert(is_ok(&res));

    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_DELETE};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'c');

    talloc_free(ctx);
}

END_TEST
/* Test: Process BACKSPACE at start (no-op) */
START_TEST(test_repl_process_action_backspace_at_start)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;

    ik_input_action_t action = {.type = IK_INPUT_BACKSPACE};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Process DELETE at end (no-op) */
START_TEST(test_repl_process_action_delete_at_end)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;

    ik_input_action_t action = {.type = IK_INPUT_DELETE};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'b');

    talloc_free(ctx);
}

END_TEST

static Suite *repl_text_editing_suite(void)
{
    Suite *s = suite_create("REPL_TextEditing");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_process_action_char);
    tcase_add_test(tc_core, test_repl_process_action_newline);
    tcase_add_test(tc_core, test_repl_process_action_backspace);
    tcase_add_test(tc_core, test_repl_process_action_delete);
    tcase_add_test(tc_core, test_repl_process_action_backspace_at_start);
    tcase_add_test(tc_core, test_repl_process_action_delete_at_end);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_text_editing_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
