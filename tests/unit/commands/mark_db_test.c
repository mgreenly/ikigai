/**
 * @file mark_db_test.c
 * @brief Mock-based tests for /mark and /rewind command DB error handling
 *
 * Note: This file uses mocks that override libpq functions globally.
 * Real database integration tests should be in a separate file without mocks.
 */

#include "../../../src/commands.h"
#include "../../../src/commands_mark.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

// Mock result for PQexecParams failure
static PGresult *mock_failed_result = (PGresult *)1;  // Non-null sentinel
static ExecStatusType mock_status = PGRES_FATAL_ERROR;

// Mock pq_exec_params_ to fail
PGresult *pq_exec_params_(PGconn *conn,
                          const char *command,
                          int nParams,
                          const Oid *paramTypes,
                          const char *const *paramValues,
                          const int *paramLengths,
                          const int *paramFormats,
                          int resultFormat)
{
    (void)conn;
    (void)command;
    (void)nParams;
    (void)paramTypes;
    (void)paramValues;
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    // Return a mock result that simulates failure
    return mock_failed_result;
}

// Mock PQresultStatus to return our configured status
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return mock_status;
    }
    // Should not reach here in tests
    return PGRES_FATAL_ERROR;
}

// Mock PQclear (no-op for our static mock)
void PQclear(PGresult *res)
{
    (void)res;
}

// Mock PQerrorMessage
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock DB error";
    return error_msg;
}

/**
 * Create a REPL context with DB context for testing
 */
static ik_repl_ctx_t *create_test_repl_with_db(void *parent)
{
    // Create scrollback buffer
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create conversation
    res_t res = ik_openai_conversation_create(parent);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);

    // Create REPL context
    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    r->scrollback = scrollback;
    r->conversation = conv;
    r->shared = shared;
    r->marks = NULL;
    r->mark_count = 0;
    r->db_ctx = NULL;
    r->current_session_id = 0;
    r->db_debug_pipe = NULL;

    return r;
}

// Per-test setup
static void test_setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    repl = create_test_repl_with_db(test_ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock status
    mock_status = PGRES_FATAL_ERROR;
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        repl = NULL;
    }
}

// ========== Tests ==========

// Test: DB error during mark persistence with NULL label (lines 81, 88-93)
START_TEST(test_mark_db_insert_error_with_null_label) {
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->db_ctx = mock_db;
    repl->current_session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create a debug pipe to capture error messages
    repl->db_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->db_debug_pipe->write_end = tmpfile();
    ck_assert_ptr_nonnull(repl->db_debug_pipe->write_end);

    // Create unlabeled mark - DB insert will fail but command succeeds
    res_t res = ik_cmd_mark(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Mark should still be created in memory
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_ptr_null(repl->marks[0]->label);

    // Read debug output to verify error was logged
    rewind(repl->db_debug_pipe->write_end);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, repl->db_debug_pipe->write_end);
    ck_assert(bytes_read > 0);
    ck_assert(strstr(buffer, "Warning: Failed to persist mark event") != NULL);

    fclose(repl->db_debug_pipe->write_end);
}
END_TEST
// Test: DB error during mark persistence with label
START_TEST(test_mark_db_insert_error_with_label)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->db_ctx = mock_db;
    repl->current_session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create a debug pipe
    repl->db_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->db_debug_pipe->write_end = tmpfile();
    ck_assert_ptr_nonnull(repl->db_debug_pipe->write_end);

    // Create labeled mark - DB insert will fail but command succeeds
    res_t res = ik_cmd_mark(test_ctx, repl, "testlabel");
    ck_assert(is_ok(&res));

    // Mark should still be created in memory
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_str_eq(repl->marks[0]->label, "testlabel");

    // Read debug output
    rewind(repl->db_debug_pipe->write_end);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, repl->db_debug_pipe->write_end);
    ck_assert(bytes_read > 0);
    ck_assert(strstr(buffer, "Warning: Failed to persist mark event") != NULL);

    fclose(repl->db_debug_pipe->write_end);
}

END_TEST
// Test: Rewind error handling when mark not found (lines 132-137)
START_TEST(test_rewind_error_handling)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->db_ctx = mock_db;
    repl->current_session_id = 1;

    // Create a mark
    res_t res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Try to rewind to a non-existent mark
    res = ik_cmd_rewind(test_ctx, repl, "nonexistent");
    ck_assert(is_ok(&res));  // Command doesn't propagate error

    // Verify error message was added to scrollback
    ck_assert(repl->scrollback->count > 0);
}

END_TEST
// Test: DB error during rewind persistence (lines 152-157)
// Note: This test verifies that rewind works in memory even when DB is unavailable
// The actual DB persistence error logging requires a valid target_message_id from DB
// which isn't available with mocks. The mark_errors_test.c tests this path more thoroughly.
START_TEST(test_rewind_db_insert_error)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->db_ctx = mock_db;
    repl->current_session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create a debug pipe
    repl->db_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->db_debug_pipe->write_end = tmpfile();
    ck_assert_ptr_nonnull(repl->db_debug_pipe->write_end);

    // Create a mark in memory only (for rewind to work)
    res_t res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Add a message
    res = ik_openai_msg_create(repl->conversation, "user", "test");
    ck_assert(is_ok(&res));
    res = ik_openai_conversation_add_msg(repl->conversation, res.ok);
    ck_assert(is_ok(&res));

    // Rewind - should succeed in memory even with DB issues
    res = ik_cmd_rewind(test_ctx, repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Rewind should succeed in memory
    ck_assert_uint_eq(repl->conversation->message_count, 0);

    fclose(repl->db_debug_pipe->write_end);
}

END_TEST
// Test: DB error with NULL debug pipe
START_TEST(test_mark_db_error_no_debug_pipe)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->db_ctx = mock_db;
    repl->current_session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Ensure debug pipe is NULL
    repl->db_debug_pipe = NULL;

    // Create mark - should not crash even without debug pipe
    res_t res = ik_cmd_mark(test_ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->mark_count, 1);
}

END_TEST
// Test: DB error with debug pipe but NULL write_end
START_TEST(test_mark_db_error_null_write_end)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->db_ctx = mock_db;
    repl->current_session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create debug pipe with NULL write_end
    repl->db_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->db_debug_pipe->write_end = NULL;

    // Create mark - should not crash
    res_t res = ik_cmd_mark(test_ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->mark_count, 1);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *commands_mark_db_suite(void)
{
    Suite *s = suite_create("Commands: Mark/Rewind DB");

    // All tests use mocks (no real database)
    TCase *tc_db_errors = tcase_create("Database Error Handling");
    tcase_add_checked_fixture(tc_db_errors, test_setup, test_teardown);
    tcase_add_test(tc_db_errors, test_mark_db_insert_error_with_null_label);
    tcase_add_test(tc_db_errors, test_mark_db_insert_error_with_label);
    tcase_add_test(tc_db_errors, test_rewind_error_handling);
    tcase_add_test(tc_db_errors, test_rewind_db_insert_error);
    tcase_add_test(tc_db_errors, test_mark_db_error_no_debug_pipe);
    tcase_add_test(tc_db_errors, test_mark_db_error_null_write_end);
    suite_add_tcase(s, tc_db_errors);

    return s;
}

int main(void)
{
    int failed = 0;
    Suite *s = commands_mark_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? 0 : 1;
}
