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
                            const char *kind,
                            const char *content,
                            const char *data_json)
{
    (void)db;
    (void)session_id;
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

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Reset mock state
    mock_db_insert_should_fail = false;
    mock_error_ctx = test_ctx;  // Use test_ctx for error allocation

    // Create REPL context
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    repl->shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);

    // Create conversation
    res_t res = ik_openai_conversation_create(test_ctx);
    ck_assert(is_ok(&res));
    repl->conversation = res.ok;
    ck_assert_ptr_nonnull(repl->conversation);

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
    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->db_debug_pipe = NULL;

    // Configure mock to fail
    mock_db_insert_should_fail = true;

    handle_request_success(repl);

    // Message should still be added to conversation despite DB error
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}
END_TEST
// Test: DB error with debug pipe
START_TEST(test_db_error_with_debug_pipe)
{
    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");

    // Create debug pipe
    ik_debug_pipe_t *debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);

    int pipefd[2];
    int pipe_result = pipe(pipefd);
    ck_assert_int_eq(pipe_result, 0);

    debug_pipe->write_end = fdopen(pipefd[1], "w");
    ck_assert_ptr_nonnull(debug_pipe->write_end);
    repl->db_debug_pipe = debug_pipe;

    // Configure mock to fail
    mock_db_insert_should_fail = true;

    handle_request_success(repl);

    // Message should still be added despite DB error
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);

    // Check that error was written to debug pipe
    fflush(debug_pipe->write_end);

    // Set pipe to non-blocking to avoid timeout
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    char buffer[512];
    ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
    ck_assert(bytes_read > 0);
    buffer[bytes_read] = '\0';

    // Verify error message content
    ck_assert_ptr_nonnull(strstr(buffer, "Warning"));
    ck_assert_ptr_nonnull(strstr(buffer, "Failed to persist"));
    ck_assert_ptr_nonnull(strstr(buffer, mock_error_message));

    // Cleanup
    fclose(debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
// Test: DB error with debug pipe but NULL write_end
START_TEST(test_db_error_with_debug_pipe_null_write_end)
{
    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");

    // Create debug pipe with NULL write_end
    ik_debug_pipe_t *debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = NULL;
    repl->db_debug_pipe = debug_pipe;

    // Configure mock to fail
    mock_db_insert_should_fail = true;

    handle_request_success(repl);

    // Message should still be added despite DB error
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST

// ========== Test Suite ==========

static Suite *handle_request_success_db_error_suite(void)
{
    Suite *s = suite_create("handle_request_success DB Error");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_db_error_no_debug_pipe);
    tcase_add_test(tc_core, test_db_error_with_debug_pipe);
    tcase_add_test(tc_core, test_db_error_with_debug_pipe_null_write_end);
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
