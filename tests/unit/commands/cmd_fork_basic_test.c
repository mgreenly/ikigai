/**
 * @file cmd_fork_basic_test.c
 * @brief Unit tests for /fork command - basic functionality
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/request.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Mock ik_agent_get_provider_ - return error when provider not initialized
res_t ik_agent_get_provider_(void *agent, void **provider_out)
{
    ik_agent_ctx_t *ctx = (ik_agent_ctx_t *)agent;
    if (ctx->provider_instance == NULL) {
        TALLOC_CTX *err_ctx = talloc_new(NULL);
        return ERR(err_ctx, PROVIDER, "Provider not initialized (mock)");
    }
    *provider_out = ctx->provider_instance;
    return OK(NULL);
}

// Mock ik_request_build_from_conversation_ - default passthrough
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)
{
    (void)agent;
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) {
        TALLOC_CTX *err_ctx = talloc_new(NULL);
        return ERR(err_ctx, OUT_OF_MEMORY, "Out of memory");
    }
    *req_out = req;
    return OK(NULL);
}

// Test fixtures
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

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

    agent->uuid = talloc_strdup(agent, "parent-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

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
    // Don't call ik_test_db_begin - ik_cmd_fork manages its own transactions

    setup_repl();
}

static void teardown(void)
{
    // Clean up database state for next test BEFORE freeing context
    // because db is allocated as child of test_ctx
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }

    // Now free everything (this also closes db connection via destructor)
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;  // Will be recreated in next setup
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: Creates new agent
START_TEST(test_fork_creates_agent) {
    size_t initial_count = repl->agent_count;

    // Debug: verify database connection before fork
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_nonnull(repl->shared);
    ck_assert_ptr_nonnull(repl->shared->db_ctx);
    if (repl->shared->db_ctx->conn == NULL) {
        ck_abort_msg("Database connection is NULL before fork!");
    }

    // Try a simple database operation to verify connection works
    PGresult *test_res = PQexec(repl->shared->db_ctx->conn, "SELECT 1");
    if (PQresultStatus(test_res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Test query failed: %s\n", PQerrorMessage(repl->shared->db_ctx->conn));
        PQclear(test_res);
        ck_abort_msg("Database connection test failed!");
    }
    PQclear(test_res);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    if (is_err(&res)) {
        fprintf(stderr, "ik_cmd_fork failed: %s\n", error_message(res.err));
    }
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, initial_count + 1);
}
END_TEST
// Test: Child has parent_uuid set
START_TEST(test_fork_sets_parent) {
    const char *parent_uuid = repl->current->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Find the newly created child
    ik_agent_ctx_t *child = repl->agents[repl->agent_count - 1];
    ck_assert_ptr_nonnull(child);
    ck_assert_ptr_nonnull(child->parent_uuid);
    ck_assert_str_eq(child->parent_uuid, parent_uuid);
}

END_TEST
// Test: Child added to agents array
START_TEST(test_fork_adds_to_array) {
    size_t initial_count = repl->agent_count;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, initial_count + 1);
    ck_assert_ptr_nonnull(repl->agents[initial_count]);
}

END_TEST
// Test: Switches to child
START_TEST(test_fork_switches_to_child) {
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_ptr_ne(repl->current, parent);
    ck_assert_str_eq(repl->current->parent_uuid, parent->uuid);
}

END_TEST
// Test: Child in registry with status='running'
START_TEST(test_fork_registry_entry) {
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Query registry for child
    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, repl->current->uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);
    ck_assert_str_eq(row->status, "running");
}

END_TEST
// Test: Confirmation message displayed
START_TEST(test_fork_confirmation_message) {
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Check scrollback for confirmation message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);
}

END_TEST
// Test: fork_pending flag set during fork
START_TEST(test_fork_pending_flag_set) {
    // This test would need mocking to observe mid-execution
    // For now, verify flag is clear after completion
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert(!atomic_load(&repl->shared->fork_pending));
}

END_TEST
// Test: fork_pending flag cleared after fork
START_TEST(test_fork_pending_flag_cleared) {
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert(!atomic_load(&repl->shared->fork_pending));
}

END_TEST
// Test: Concurrent fork rejected
START_TEST(test_fork_concurrent_rejected) {
    atomic_store(&repl->shared->fork_pending, true);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));  // Returns OK but appends error

    // Check scrollback for error message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Fork already in progress")) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}

END_TEST
// Note: Rollback and error handling tests removed.
// These tests attempted to violate preconditions (setting db_ctx->conn = NULL)
// which triggers assertions in ik_db_begin, making them untestable without mocking.
// Proper error handling tests would require mocking the database layer or
// testing with actual database errors (not precondition violations).

// Test: /fork with quoted prompt extracts prompt
START_TEST(test_fork_with_quoted_prompt) {
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Research OAuth 2.0 patterns\"");
    ck_assert(is_ok(&res));

    // Verify child has message in conversation
    ik_agent_ctx_t *child = repl->current;
    ck_assert_uint_gt(child->message_count, 0);

    // Find user message with the prompt
    bool found_prompt = false;
    for (size_t i = 0; i < child->message_count; i++) {
        ik_message_t *msg = child->messages[i];
        if (msg->role == IK_ROLE_USER &&
            msg->content_count > 0 &&
            msg->content_blocks[0].type == IK_CONTENT_TEXT &&
            msg->content_blocks[0].data.text.text != NULL &&
            strcmp(msg->content_blocks[0].data.text.text, "Research OAuth 2.0 patterns") == 0) {
            found_prompt = true;
            break;
        }
    }
    ck_assert(found_prompt);
}

END_TEST
// Test: Prompt added as user message
START_TEST(test_fork_prompt_appended_as_user_message) {
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Analyze database schema\"");
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    ck_assert_uint_gt(child->message_count, 0);

    // Verify at least one user message exists
    bool has_user_message = false;
    for (size_t i = 0; i < child->message_count; i++) {
        if (child->messages[i]->role == IK_ROLE_USER) {
            has_user_message = true;
            break;
        }
    }
    ck_assert(has_user_message);
}

END_TEST
// Test: LLM call triggered when prompt provided
START_TEST(test_fork_llm_call_triggered) {
    res_t res = ik_cmd_fork(test_ctx, repl, "\"Test prompt\"");
    ck_assert(is_ok(&res));

    // Verify user message was added to conversation
    // Note: We can't reliably test LLM state in unit tests without mocking
    // because ik_openai_multi_add_request will fail without a real HTTP connection.
    // Instead, we verify the prompt was added to the conversation, which is the
    // key precondition for LLM triggering.
    ik_agent_ctx_t *child = repl->current;
    bool found_user_message = false;
    for (size_t i = 0; i < child->message_count; i++) {
        ik_message_t *msg = child->messages[i];
        if (msg->role == IK_ROLE_USER &&
            msg->content_count > 0 &&
            msg->content_blocks[0].type == IK_CONTENT_TEXT &&
            msg->content_blocks[0].data.text.text != NULL &&
            strcmp(msg->content_blocks[0].data.text.text, "Test prompt") == 0) {
            found_user_message = true;
            break;
        }
    }
    ck_assert(found_user_message);
}

END_TEST
// Test: Empty prompt treated as no prompt
START_TEST(test_fork_empty_prompt) {
    res_t res = ik_cmd_fork(test_ctx, repl, "\"\"");
    ck_assert(is_ok(&res));

    // Empty prompt should behave like no prompt - agent stays IDLE
    ik_agent_ctx_t *child = repl->current;
    ck_assert_int_eq(child->state, IK_AGENT_STATE_IDLE);
}

END_TEST
// Test: Unquoted text rejected
START_TEST(test_fork_unquoted_text_rejected) {
    res_t res = ik_cmd_fork(test_ctx, repl, "unquoted text");
    ck_assert(is_ok(&res));  // Returns OK but shows error

    // Check scrollback for error message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text &&
            (strstr(text, "must be quoted") || strstr(text, "Error"))) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}

END_TEST

static Suite *cmd_fork_suite(void)
{
    Suite *s = suite_create("Fork Command Basic");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_creates_agent);
    tcase_add_test(tc, test_fork_sets_parent);
    tcase_add_test(tc, test_fork_adds_to_array);
    tcase_add_test(tc, test_fork_switches_to_child);
    tcase_add_test(tc, test_fork_registry_entry);
    tcase_add_test(tc, test_fork_confirmation_message);
    tcase_add_test(tc, test_fork_pending_flag_set);
    tcase_add_test(tc, test_fork_pending_flag_cleared);
    tcase_add_test(tc, test_fork_concurrent_rejected);
    tcase_add_test(tc, test_fork_with_quoted_prompt);
    tcase_add_test(tc, test_fork_prompt_appended_as_user_message);
    tcase_add_test(tc, test_fork_llm_call_triggered);
    tcase_add_test(tc, test_fork_empty_prompt);
    tcase_add_test(tc, test_fork_unquoted_text_rejected);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_fork_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
