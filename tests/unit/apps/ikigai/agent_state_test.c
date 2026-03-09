/* Unit tests for agent_state.c */

#include <check.h>
#include <pthread.h>
#include <stdlib.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/ansi.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/token_cache.h"
#include "shared/terminal.h"

/* --- Helpers --- */

static ik_message_t *append_msg(ik_agent_ctx_t *a, ik_role_t role,
                                const char *kind)
{
    size_t idx = a->message_count++;
    a->message_capacity = a->message_count;
    a->messages = talloc_realloc(a, a->messages, ik_message_t *,
                                 (unsigned int)a->message_count);
    ik_message_t *m = talloc_zero(a, ik_message_t);
    m->role = role;
    m->kind = (kind != NULL) ? talloc_strdup(m, kind) : NULL;
    a->messages[idx] = m;
    return m;
}

static void add_user_msg(ik_agent_ctx_t *a, const char *text)
{
    ik_message_t *m = append_msg(a, IK_ROLE_USER, "user");
    m->content_count = 1;
    m->content_blocks = talloc_array(m, ik_content_block_t, 1);
    m->content_blocks[0].type = IK_CONTENT_TEXT;
    m->content_blocks[0].data.text.text = talloc_strdup(m, text);
}

static void add_tool_result_msg(ik_agent_ctx_t *a, const char *content)
{
    ik_message_t *m = append_msg(a, IK_ROLE_TOOL, "tool_result");
    m->content_count = 1;
    m->content_blocks = talloc_array(m, ik_content_block_t, 1);
    m->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    m->content_blocks[0].data.tool_result.content =
        (content != NULL) ? talloc_strdup(m, content) : NULL;
}

static void add_tool_call_msg(ik_agent_ctx_t *a, const char *name)
{
    ik_message_t *m = append_msg(a, IK_ROLE_ASSISTANT, "tool_call");
    m->content_count = 1;
    m->content_blocks = talloc_array(m, ik_content_block_t, 1);
    m->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    m->content_blocks[0].data.tool_call.id        = talloc_strdup(m, "id1");
    m->content_blocks[0].data.tool_call.name      = talloc_strdup(m, name);
    m->content_blocks[0].data.tool_call.arguments = NULL;
}

static void add_empty_msg(ik_agent_ctx_t *a, ik_role_t role, const char *kind)
{
    ik_message_t *m = append_msg(a, role, kind);
    m->content_count = 0;
    m->content_blocks = NULL;
}

/* Shared setup: 2 turns, budget=10, turns recorded at 100 tokens each. */
static void setup_prune_cache(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);
    ik_token_cache_set_budget(cache, 10);
    ik_token_cache_add_turn(cache);
    ik_token_cache_add_turn(cache);
    ik_token_cache_record_turn(cache, 0, 100);
    ik_token_cache_record_turn(cache, 1, 100);
    agent->token_cache = cache;
}

/* --- PruneTokenCache tests --- */

/* if (!running) cleanup block frees snapshot buffers when dispatch no-ops. */
START_TEST(test_prune_cleans_snapshot_when_dispatch_does_not_start_thread) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "first");
    add_user_msg(agent, "second");
    agent->model    = talloc_strdup(agent, "test-model");
    agent->provider = talloc_strdup(agent, ""); /* empty → dispatch no-op */
    setup_prune_cache(ctx, agent);

    ck_assert_ptr_null(agent->summary_msgs_stubs);
    ck_assert_ptr_null(agent->summary_msgs_ptrs);
    ik_agent_prune_token_cache(agent);
    ck_assert_ptr_null(agent->summary_msgs_stubs);
    ck_assert_ptr_null(agent->summary_msgs_ptrs);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* token_cache NULL → prune returns at line 229. */
START_TEST(test_prune_null_cache_returns_immediately) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    agent->token_cache = NULL;
    ik_agent_prune_token_cache(agent);
    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* provider NULL → dispatch block condition FALSE, no stubs allocated. */
START_TEST(test_prune_null_provider_skips_dispatch_block) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "first");
    add_user_msg(agent, "second");
    agent->model    = talloc_strdup(agent, "test-model");
    agent->provider = NULL;
    setup_prune_cache(ctx, agent);

    ik_agent_prune_token_cache(agent);
    ck_assert_ptr_null(agent->summary_msgs_stubs);
    ck_assert_ptr_null(agent->summary_msgs_ptrs);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* With scrollback: refresh_scrollback_with_hr is exercised. */
START_TEST(test_prune_with_scrollback_refreshes_scrollback) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "first");
    add_user_msg(agent, "second");
    agent->model      = talloc_strdup(agent, "test-model");
    agent->provider   = talloc_strdup(agent, "");
    agent->scrollback = ik_scrollback_create(ctx, 80);
    setup_prune_cache(ctx, agent);

    ik_agent_prune_token_cache(agent);
    ck_assert_ptr_null(agent->summary_msgs_stubs);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/*
 * Mixed message types to cover tool_result, tool_call, and empty-content
 * branches in refresh_scrollback_with_hr and build_summary_msg_snapshot.
 * Layout: [0] USER, [1] TOOL_RESULT, [2] TOOL_CALL, [3] empty, [4] USER.
 * After pruning turn 0, ctx_idx=4; snapshot covers [0..3]; scrollback all.
 */
START_TEST(test_prune_with_mixed_messages) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "first");
    add_tool_result_msg(agent, "result1");
    add_tool_call_msg(agent, "my_tool");
    add_empty_msg(agent, IK_ROLE_ASSISTANT, "assistant");
    add_user_msg(agent, "second");
    agent->model      = talloc_strdup(agent, "test-model");
    agent->provider   = talloc_strdup(agent, "");
    agent->scrollback = ik_scrollback_create(ctx, 80);
    setup_prune_cache(ctx, agent);

    ik_agent_prune_token_cache(agent);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* --- RecordAndPrune tests --- */

/* was_success=false → early return, turn token unchanged. */
START_TEST(test_record_and_prune_no_op_on_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "msg");
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);
    ik_token_cache_set_budget(cache, 100000);
    ik_token_cache_add_turn(cache);
    ik_token_cache_record_turn(cache, 0, 10);
    agent->token_cache = cache;

    ik_agent_record_and_prune_token_cache(agent, false);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(cache, 0), 10);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* delta > 0 → records actual token delta. */
START_TEST(test_record_and_prune_records_positive_delta) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "msg");
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);
    ik_token_cache_set_budget(cache, 100000);
    ik_token_cache_add_turn(cache);
    agent->token_cache = cache;
    agent->prev_response_input_tokens = 100;
    agent->response_input_tokens      = 142;

    ik_agent_record_and_prune_token_cache(agent, true);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(cache, 0), 42);
    ck_assert_int_eq(agent->prev_response_input_tokens, 142);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* delta <= 0 → falls back to existing bytes estimate. */
START_TEST(test_record_and_prune_falls_back_to_estimate) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    add_user_msg(agent, "msg");
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);
    ik_token_cache_set_budget(cache, 100000);
    ik_token_cache_add_turn(cache);
    ik_token_cache_record_turn(cache, 0, 77);
    agent->token_cache = cache;
    agent->prev_response_input_tokens = 50;
    agent->response_input_tokens      = 50;

    ik_agent_record_and_prune_token_cache(agent, true);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(cache, 0), 77);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* token_cache NULL → early return even when was_success=true. */
START_TEST(test_record_and_prune_null_cache_noop) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    agent->response_input_tokens      = 99;
    agent->prev_response_input_tokens = 0;

    ik_agent_record_and_prune_token_cache(agent, true);
    ck_assert_int_eq(agent->prev_response_input_tokens, 0);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* turn_count == 0 → skips turn block, still calls prune and updates prev. */
START_TEST(test_record_and_prune_zero_turns_calls_prune) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);
    ik_token_cache_set_budget(cache, 100000);
    agent->token_cache                = cache;
    agent->response_input_tokens      = 10;
    agent->prev_response_input_tokens = 0;

    ik_agent_record_and_prune_token_cache(agent, true);
    ck_assert_int_eq(agent->prev_response_input_tokens, 10);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* est == 0 → if (est > 0) FALSE branch: nothing re-recorded. */
START_TEST(test_record_and_prune_zero_estimate_skips_record) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    pthread_mutex_init(&agent->summary_thread_mutex, NULL);
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);
    ik_token_cache_set_budget(cache, 100000);
    ik_token_cache_add_turn(cache); /* uncached; no messages → estimate=0 */
    agent->token_cache                = cache;
    agent->prev_response_input_tokens = 5;
    agent->response_input_tokens      = 5;

    ik_agent_record_and_prune_token_cache(agent, true);
    ck_assert_int_eq(agent->prev_response_input_tokens, 5);

    pthread_mutex_destroy(&agent->summary_thread_mutex);
    talloc_free(ctx);
}
END_TEST

/* --- AppendContextHr tests --- */

/* No shared/term → uses 80 cols fallback. */
START_TEST(test_append_context_hr_without_term) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    agent->scrollback = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(agent->scrollback);
    ik_agent_append_context_hr(agent);
    talloc_free(ctx);
}
END_TEST

/* shared->term set → uses term->screen_cols. */
START_TEST(test_append_context_hr_with_term) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    agent->scrollback       = ik_scrollback_create(ctx, 80);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ik_term_ctx_t *term     = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_cols = 40;
    shared->term  = term;
    agent->shared = shared;
    ik_agent_append_context_hr(agent);
    talloc_free(ctx);
}
END_TEST

/* screen_cols < label_len → remaining < 0 clamped to 0. */
START_TEST(test_append_context_hr_tiny_cols) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent   = talloc_zero(ctx, ik_agent_ctx_t);
    agent->scrollback       = ik_scrollback_create(ctx, 4);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ik_term_ctx_t *term     = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_cols = 4;
    shared->term  = term;
    agent->shared = shared;
    ik_agent_append_context_hr(agent);
    talloc_free(ctx);
}
END_TEST

/* colors disabled → ik_ansi_colors_enabled() FALSE branch in ik_agent_append_context_hr. */
START_TEST(test_append_context_hr_colors_disabled) {
    setenv("NO_COLOR", "1", 1);
    ik_ansi_init();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    agent->scrollback     = ik_scrollback_create(ctx, 80);
    ik_agent_append_context_hr(agent);
    talloc_free(ctx);
    unsetenv("NO_COLOR");
    ik_ansi_init();
}
END_TEST

/* --- Suite --- */

static Suite *agent_state_suite(void)
{
    Suite *s = suite_create("agent_state");

    TCase *tc = tcase_create("PruneTokenCache");
    tcase_add_test(tc,
        test_prune_cleans_snapshot_when_dispatch_does_not_start_thread);
    tcase_add_test(tc, test_prune_null_cache_returns_immediately);
    tcase_add_test(tc, test_prune_null_provider_skips_dispatch_block);
    tcase_add_test(tc, test_prune_with_scrollback_refreshes_scrollback);
    tcase_add_test(tc, test_prune_with_mixed_messages);
    suite_add_tcase(s, tc);

    TCase *tc_record = tcase_create("RecordAndPrune");
    tcase_add_test(tc_record, test_record_and_prune_no_op_on_failure);
    tcase_add_test(tc_record, test_record_and_prune_records_positive_delta);
    tcase_add_test(tc_record, test_record_and_prune_falls_back_to_estimate);
    tcase_add_test(tc_record, test_record_and_prune_null_cache_noop);
    tcase_add_test(tc_record, test_record_and_prune_zero_turns_calls_prune);
    tcase_add_test(tc_record, test_record_and_prune_zero_estimate_skips_record);
    suite_add_tcase(s, tc_record);

    TCase *tc_hr = tcase_create("AppendContextHr");
    tcase_add_test(tc_hr, test_append_context_hr_without_term);
    tcase_add_test(tc_hr, test_append_context_hr_with_term);
    tcase_add_test(tc_hr, test_append_context_hr_tiny_cols);
    tcase_add_test(tc_hr, test_append_context_hr_colors_disabled);
    suite_add_tcase(s, tc_hr);

    return s;
}

int32_t main(void)
{
    Suite *s = agent_state_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent_state_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
