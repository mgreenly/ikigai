#include "agent.h"
/**
 * @file handle_request_success_db_error_test.c
 * @brief Tests for database error handling in handle_request_success
 *
 * This test covers the uncovered error path when ik_db_message_insert fails.
 * It uses mocking to inject database errors without requiring a live PostgreSQL instance.
 */

#include "../../../src/repl.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/db/connection.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

// ========== Mock State ==========

static bool mock_db_insert_should_fail = false;
static const char *mock_error_message = "Mock database error";
static TALLOC_CTX *mock_error_ctx = NULL;

// ========== Mock Implementation ==========

// Mock ik_db_message_insert_ to inject failures
res_t ik_db_message_insert_(void *db,
                            int64_t session_id,
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;

    if (mock_db_insert_should_fail) {
        return ERR(mock_error_ctx, DB_CONNECT, "%s", mock_error_message);
    }
    return OK(NULL);
}

// ========== Test Setup/Teardown ==========

static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Reset mock state
    mock_db_insert_should_fail = false;
    mock_error_ctx = test_ctx;  // Use test_ctx for error allocation

    // Create REPL context
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    // Create shared context
    repl->shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);

    // Create logger instance
    char tmpdir[] = "/tmp/ikigai_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    repl->shared->logger = ik_logger_create(repl->shared, tmpdir);
    ck_assert_ptr_nonnull(repl->shared->logger);

    // Create conversation
    agent->conversation = ik_openai_conversation_create(test_ctx);
    ck_assert_ptr_nonnull(agent->conversation);
    repl->current = agent;

    // Set up minimal database context (we use a dummy pointer since we're mocking)
    repl->shared->db_ctx = (ik_db_ctx_t *)0x1;  // Non-NULL dummy pointer
    repl->shared->session_id = 1;       // Valid session ID
}

static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

// ========== Tests ==========

// Test: DB error without debug pipe
START_TEST(test_db_error_no_debug_pipe) {
    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->shared->db_debug_pipe = NULL;

    // Configure mock to fail
    mock_db_insert_should_fail = true;

    ik_repl_handle_agent_request_success(repl, repl->current);

    // Message should still be added to conversation despite DB error
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}
END_TEST
// Test: DB error with logger (replaces debug pipe test)
START_TEST(test_db_error_with_logger)
{
    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->response_model = talloc_strdup(test_ctx, "gpt-4");

    // Configure mock to fail
    mock_db_insert_should_fail = true;

    ik_repl_handle_agent_request_success(repl, repl->current);

    // Message should still be added to conversation despite DB error
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);

    // Note: Error is now logged via JSONL logger instead of debug pipe.
    // The logger writes to .ikigai/logs/ikigai.log, which is captured during test execution.
    // We verify the function completes successfully and the conversation is updated.
}

END_TEST
// Test: DB error - removed (debug pipe no longer used)
// The previous test for NULL write_end is no longer relevant since we use logger

// ========== Test Suite ==========

static Suite *handle_request_success_db_error_suite(void)
{
    Suite *s = suite_create("handle_request_success DB Error");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_db_error_no_debug_pipe);
    tcase_add_test(tc_core, test_db_error_with_logger);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = handle_request_success_db_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
