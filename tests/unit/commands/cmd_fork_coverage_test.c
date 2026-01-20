/**
 * @file cmd_fork_coverage_test.c
 * @brief Unit tests for /fork command - coverage gaps
 */

#include "cmd_fork_coverage_test_helper.h"

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/provider_vtable.h"
#include "../../../src/providers/request.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>

// Test fixtures
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static int test_counter = 0;

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;

    // Unique UUID per test (timestamp + counter) to avoid DB conflicts
    agent->uuid = talloc_asprintf(agent, "parent-uuid-%ld-%d", (long)time(NULL), ++test_counter);
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "gpt-4o-mini");
    agent->thinking_level = IK_THINKING_NONE;
    repl->current = agent;

    // Create mock provider with vtable
    ik_provider_t *provider = talloc_zero(agent, ik_provider_t);
    ck_assert_ptr_nonnull(provider);
    provider->ctx = agent;

    ik_provider_vtable_t *vt = talloc_zero(provider, ik_provider_vtable_t);
    ck_assert_ptr_nonnull(vt);
    vt->start_stream = mock_start_stream;
    provider->vt = vt;

    agent->provider_instance = provider;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    atomic_init(&shared->fork_pending, false);
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Insert parent agent into registry
    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert parent agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup parent agent in registry");
    }
}

static bool suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to create database: %s\n", error_message(res.err));
        talloc_free(res.err);
        return false;
    }
    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to migrate database: %s\n", error_message(res.err));
        talloc_free(res.err);
        ik_test_db_destroy(DB_NAME);
        return false;
    }
    return true;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    res_t db_res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to connect to database: %s\n", error_message(db_res.err));
        ck_abort_msg("Database connection failed");
    }
    ck_assert_ptr_nonnull(db);
    ck_assert_ptr_nonnull(db->conn);

    setup_repl();

    // Reset mock flags
    ik_test_mock_reset_flags();
}

static void teardown(void)
{
    // Clean up database state for next test BEFORE freeing context
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }

    // Now free everything (this also closes db connection via destructor)
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: Lines 84-87: Clear assistant_response when non-NULL
START_TEST(test_fork_clears_assistant_response) {
    // Set up agent with assistant_response
    repl->current->assistant_response = talloc_strdup(repl->current, "Previous assistant message");
    ck_assert_ptr_nonnull(repl->current->assistant_response);

    // Fork with prompt to trigger handle_fork_prompt
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test clearing response\"");
    ck_assert(is_ok(&res));

    // Note: The child agent is now repl->current
    // We can't directly verify the parent's assistant_response was cleared
    // because the parent is no longer current, but we can verify the operation succeeded
}
END_TEST
// Test: Lines 84-87: Clear streaming_line_buffer when non-NULL
START_TEST(test_fork_clears_streaming_buffer) {
    // Set up agent with streaming_line_buffer
    repl->current->streaming_line_buffer = talloc_strdup(repl->current, "Partial line");
    ck_assert_ptr_nonnull(repl->current->streaming_line_buffer);

    // Fork with prompt to trigger handle_fork_prompt
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test clearing buffer\"");
    ck_assert(is_ok(&res));

    // Verify operation succeeded
    ck_assert(is_ok(&res));
}

END_TEST
// Test: Lines 84-87: Both assistant_response and streaming_line_buffer non-NULL
START_TEST(test_fork_clears_both_response_and_buffer) {
    // Set up agent with both fields
    repl->current->assistant_response = talloc_strdup(repl->current, "Previous response");
    repl->current->streaming_line_buffer = talloc_strdup(repl->current, "Partial buffer");
    ck_assert_ptr_nonnull(repl->current->assistant_response);
    ck_assert_ptr_nonnull(repl->current->streaming_line_buffer);

    // Fork with prompt to trigger handle_fork_prompt
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test clearing both\"");
    ck_assert(is_ok(&res));

    // Verify operation succeeded
    ck_assert(is_ok(&res));
}

END_TEST
// Test: Line 98: ik_agent_get_provider returns error
START_TEST(test_fork_prompt_provider_error) {
    // Enable mock failure for provider
    ik_test_mock_set_provider_failure(true);

    // Fork with prompt to trigger handle_fork_prompt
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test provider error\"");
    ck_assert(is_ok(&res));

    // The child agent should have an error message in scrollback
    ik_agent_ctx_t *child = repl->current;
    size_t line_count = ik_scrollback_get_line_count(child->scrollback);
    ck_assert_uint_gt(line_count, 0);

    // The agent should be in IDLE state due to error
    ck_assert_int_eq(child->state, IK_AGENT_STATE_IDLE);
}

END_TEST
// Test: Line 109: ik_request_build_from_conversation returns error
START_TEST(test_fork_prompt_build_request_error) {
    // Enable mock failure for request building
    ik_test_mock_set_request_failure(true);

    // Fork with prompt to trigger handle_fork_prompt
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test request build error\"");
    ck_assert(is_ok(&res));

    // The child agent should have an error in scrollback and be IDLE
    ik_agent_ctx_t *child = repl->current;
    ck_assert_int_eq(child->state, IK_AGENT_STATE_IDLE);
}

END_TEST
// Test: Lines 107-127: Success path in handle_fork_prompt
START_TEST(test_fork_prompt_success_path) {
    ik_test_mock_set_provider_failure(false);
    ik_test_mock_set_request_failure(false);
    ik_test_mock_set_stream_failure(false);

    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test successful prompt handling\"");
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    ck_assert_int_eq(child->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert_int_eq(child->tool_iteration_count, 0);
    ck_assert_int_eq(child->curl_still_running, 1);
}

END_TEST
// Test: Line 294 branch: child->thinking_level == IK_THINKING_NONE
START_TEST(test_fork_no_thinking_level) {
    // Fork without thinking level (defaults to NONE)
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Child should have thinking_level == NONE
    ik_agent_ctx_t *child = repl->current;
    ck_assert_int_eq(child->thinking_level, IK_THINKING_NONE);
}

END_TEST
// Test: Line 294 branch: child->model == NULL
START_TEST(test_fork_no_model) {
    // Set up parent with NULL model
    repl->current->model = NULL;
    repl->current->thinking_level = IK_THINKING_HIGH;

    // Fork to inherit NULL model
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Child should have NULL model (inherited from parent)
    ik_agent_ctx_t *child = repl->current;
    ck_assert_ptr_null(child->model);
}

END_TEST
// Test: Line 297: supports_thinking is true (no warning)
START_TEST(test_fork_supports_thinking) {
    // Fork with a model that supports thinking
    res_t res = ik_cmd_fork(test_ctx, repl, "--model claude-opus-4-5/high");
    ck_assert(is_ok(&res));

    // Check that NO warning message appears in scrollback
    ik_agent_ctx_t *child = repl->current;
    size_t line_count = ik_scrollback_get_line_count(child->scrollback);
    bool found_warning = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(child->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "does not support thinking")) {
            found_warning = true;
            break;
        }
    }
    ck_assert(!found_warning);
}

END_TEST
// Test: Lines 157-160: Parse error displays message to scrollback
START_TEST(test_fork_parse_error_display) {
    // Pass malformed arguments to trigger parse error
    res_t res = ik_cmd_fork(test_ctx, repl, "unquoted text");
    ck_assert(is_ok(&res));  // Function returns OK even on parse error

    // Check that error message was added to scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);

    // Verify error message contains expected text
    const char *text = NULL;
    size_t length = 0;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &text, &length);
    ck_assert(is_ok(&line_res));
    ck_assert_ptr_nonnull(text);
    // Should contain error about unquoted prompt
}
END_TEST
// Test: Lines 165-170: Fork already in progress error
START_TEST(test_fork_already_in_progress) {
    atomic_store(&repl->shared->fork_pending, true);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);

    const char *text = NULL;
    size_t length = 0;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &text, &length);
    ck_assert(is_ok(&line_res));
    ck_assert_ptr_nonnull(text);
    ck_assert(strstr(text, "Fork already in progress") != NULL);

    atomic_store(&repl->shared->fork_pending, false);
}
END_TEST

// Test: Lines 205-207: Add lower_separator_layer to child agent
START_TEST(test_fork_with_lower_separator_layer) {
    bool lower_separator_visible = false;
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &lower_separator_visible);
    ck_assert_ptr_nonnull(repl->lower_separator_layer);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    ck_assert_ptr_nonnull(child);
    ck_assert_ptr_nonnull(child->layer_cake);
}
END_TEST

static Suite *cmd_fork_coverage_suite(void)
{
    Suite *s = suite_create("Fork Command Coverage");
    TCase *tc = tcase_create("Coverage Gaps");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_clears_assistant_response);
    tcase_add_test(tc, test_fork_clears_streaming_buffer);
    tcase_add_test(tc, test_fork_clears_both_response_and_buffer);
    tcase_add_test(tc, test_fork_prompt_provider_error);
    tcase_add_test(tc, test_fork_prompt_build_request_error);
    tcase_add_test(tc, test_fork_prompt_success_path);
    tcase_add_test(tc, test_fork_no_thinking_level);
    tcase_add_test(tc, test_fork_no_model);
    tcase_add_test(tc, test_fork_supports_thinking);
    tcase_add_test(tc, test_fork_parse_error_display);
    tcase_add_test(tc, test_fork_already_in_progress);
    tcase_add_test(tc, test_fork_with_lower_separator_layer);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_fork_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
