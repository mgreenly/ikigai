// REPL action tests - Bracketed paste mode
#include "tests/test_constants.h"
#include <check.h>
#include <stdatomic.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/terminal.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper.h"

// Minimal mocks required by the linker for repl_actions_llm.c code paths
res_t ik_db_message_insert(ik_db_ctx_t *db_ctx,
                           int64_t session_id,
                           const char *agent_uuid,
                           const char *kind,
                           const char *content,
                           const char *data_json)
{
    (void)db_ctx; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx; (void)session_id_out;
    return OK(NULL);
}

res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx; (void)session_id_out;
    return OK(NULL);
}

void ik_agent_invalidate_provider(ik_agent_ctx_t *agent)
{
    (void)agent;
}

res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, const void *row)
{
    (void)agent; (void)row;
    return OK(NULL);
}

res_t ik_agent_get_provider(ik_agent_ctx_t *agent, ik_provider_t **provider_out)
{
    *provider_out = agent->provider_instance;
    return OK(NULL);
}

// Test fixtures
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    repl->shared = shared;

    // Leave cfg NULL so newline action returns without LLM call
    shared->cfg = NULL;
    shared->history = NULL;

    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    repl->current = agent;
    agent->shared = shared;

    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    repl->current->input_buffer = ik_input_buffer_create(repl);
    ck_assert_ptr_nonnull(repl->current->input_buffer);

    repl->current->messages = NULL;
    repl->current->message_count = 0;
    repl->current->message_capacity = 0;

    repl->shared->term = talloc_zero_(repl, sizeof(ik_term_ctx_t));
    ck_assert_ptr_nonnull(repl->shared->term);
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;

    repl->current->viewport_offset = 0;
    repl->current->curl_still_running = 0;
    repl->in_paste_mode = false;

    atomic_store(&repl->current->state, IK_AGENT_STATE_IDLE);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Test: PASTE_START sets in_paste_mode
START_TEST(test_paste_start_sets_flag) {
    ck_assert(!repl->in_paste_mode);

    ik_input_action_t action = { .type = IK_INPUT_PASTE_START, .codepoint = 0 };
    res_t r = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&r));

    ck_assert(repl->in_paste_mode);
}
END_TEST

// Test: PASTE_END clears in_paste_mode
START_TEST(test_paste_end_clears_flag) {
    repl->in_paste_mode = true;

    ik_input_action_t action = { .type = IK_INPUT_PASTE_END, .codepoint = 0 };
    res_t r = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&r));

    ck_assert(!repl->in_paste_mode);
}
END_TEST

// Test: CR during paste inserts newline instead of submitting
START_TEST(test_cr_during_paste_inserts_newline) {
    // Pre-condition: in paste mode
    repl->in_paste_mode = true;

    // Insert some text first
    ik_input_action_t char_action = { .type = IK_INPUT_CHAR, .codepoint = 'x' };
    res_t r = ik_repl_process_action(repl, &char_action);
    ck_assert(is_ok(&r));

    // Send NEWLINE (CR)
    ik_input_action_t newline = { .type = IK_INPUT_NEWLINE, .codepoint = 0 };
    r = ik_repl_process_action(repl, &newline);
    ck_assert(is_ok(&r));

    // Agent should still be IDLE (not submitted)
    ck_assert_int_eq(atomic_load(&repl->current->state), IK_AGENT_STATE_IDLE);

    // Buffer should contain 'x' + '\n' (not cleared)
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_int_eq(text[0], 'x');
    ck_assert_int_eq(text[1], '\n');
}
END_TEST

// Test: Characters during paste insert normally
START_TEST(test_chars_during_paste_insert_normally) {
    repl->in_paste_mode = true;

    ik_input_action_t action_h = { .type = IK_INPUT_CHAR, .codepoint = 'h' };
    ik_input_action_t action_i = { .type = IK_INPUT_CHAR, .codepoint = 'i' };
    res_t r = ik_repl_process_action(repl, &action_h);
    ck_assert(is_ok(&r));
    r = ik_repl_process_action(repl, &action_i);
    ck_assert(is_ok(&r));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_int_eq(text[0], 'h');
    ck_assert_int_eq(text[1], 'i');
}
END_TEST

// Test: CR after paste ends triggers submit (buffer cleared)
START_TEST(test_cr_after_paste_submits) {
    // Start paste, add some text, end paste
    repl->in_paste_mode = true;

    ik_input_action_t char_a = { .type = IK_INPUT_CHAR, .codepoint = 'a' };
    res_t r = ik_repl_process_action(repl, &char_a);
    ck_assert(is_ok(&r));

    ik_input_action_t paste_end = { .type = IK_INPUT_PASTE_END, .codepoint = 0 };
    r = ik_repl_process_action(repl, &paste_end);
    ck_assert(is_ok(&r));
    ck_assert(!repl->in_paste_mode);

    // Add more text to ensure buffer is non-empty
    ik_input_action_t char_b = { .type = IK_INPUT_CHAR, .codepoint = 'b' };
    r = ik_repl_process_action(repl, &char_b);
    ck_assert(is_ok(&r));

    // CR should now submit (buffer cleared)
    ik_input_action_t newline = { .type = IK_INPUT_NEWLINE, .codepoint = 0 };
    r = ik_repl_process_action(repl, &newline);
    ck_assert(is_ok(&r));

    // Buffer should be cleared (submit happened, not insert)
    size_t len = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 0);
}
END_TEST

// Test: Full paste simulation - line1\rline2\rline3 -> buffer contains newlines
START_TEST(test_full_paste_simulation) {
    // Simulate: ESC[200~ + "line1\rline2" + ESC[201~
    ik_input_action_t paste_start = { .type = IK_INPUT_PASTE_START, .codepoint = 0 };
    res_t r = ik_repl_process_action(repl, &paste_start);
    ck_assert(is_ok(&r));
    ck_assert(repl->in_paste_mode);

    // Type "line1"
    const char *word1 = "line1";
    for (const char *p = word1; *p; p++) {
        ik_input_action_t ca = { .type = IK_INPUT_CHAR, .codepoint = (uint32_t)*p };
        r = ik_repl_process_action(repl, &ca);
        ck_assert(is_ok(&r));
    }

    // CR inside paste → insert newline
    ik_input_action_t cr = { .type = IK_INPUT_NEWLINE, .codepoint = 0 };
    r = ik_repl_process_action(repl, &cr);
    ck_assert(is_ok(&r));

    // Type "line2"
    const char *word2 = "line2";
    for (const char *p = word2; *p; p++) {
        ik_input_action_t ca = { .type = IK_INPUT_CHAR, .codepoint = (uint32_t)*p };
        r = ik_repl_process_action(repl, &ca);
        ck_assert(is_ok(&r));
    }

    // End paste
    ik_input_action_t paste_end = { .type = IK_INPUT_PASTE_END, .codepoint = 0 };
    r = ik_repl_process_action(repl, &paste_end);
    ck_assert(is_ok(&r));
    ck_assert(!repl->in_paste_mode);

    // Agent still IDLE (no submit happened)
    ck_assert_int_eq(atomic_load(&repl->current->state), IK_AGENT_STATE_IDLE);

    // Buffer should contain "line1\nline2"
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 11);
    ck_assert_int_eq(memcmp(text, "line1\nline2", 11), 0);
}
END_TEST

// Test: PASTE_END without PASTE_START is a no-op (no crash)
START_TEST(test_paste_end_without_start) {
    ck_assert(!repl->in_paste_mode);

    ik_input_action_t action = { .type = IK_INPUT_PASTE_END, .codepoint = 0 };
    res_t r = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&r));

    // Flag stays false
    ck_assert(!repl->in_paste_mode);
}
END_TEST

// Test: Double PASTE_START is safe (stays in paste mode)
START_TEST(test_double_paste_start) {
    ik_input_action_t action = { .type = IK_INPUT_PASTE_START, .codepoint = 0 };
    res_t r = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&r));
    ck_assert(repl->in_paste_mode);

    // Second PASTE_START
    r = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&r));

    // Still in paste mode
    ck_assert(repl->in_paste_mode);
}
END_TEST

static Suite *paste_suite(void)
{
    Suite *s = suite_create("REPL Paste Mode");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_paste_start_sets_flag);
    tcase_add_test(tc, test_paste_end_clears_flag);
    tcase_add_test(tc, test_cr_during_paste_inserts_newline);
    tcase_add_test(tc, test_chars_during_paste_insert_normally);
    tcase_add_test(tc, test_cr_after_paste_submits);
    tcase_add_test(tc, test_full_paste_simulation);
    tcase_add_test(tc, test_paste_end_without_start);
    tcase_add_test(tc, test_double_paste_start);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = paste_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_actions_paste_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
