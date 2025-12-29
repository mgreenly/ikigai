/**
 * @file mark_db_test.c
 * @brief Mock-based tests for /mark and /rewind command DB error handling
 *
 * Note: This file uses mocks that override libpq functions globally.
 * Real database integration tests should be in a separate file without mocks.
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/commands_mark.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/logger.h"
#include "../../../src/marks.h"
#include "../../../src/message.h"
#include "../../../src/providers/provider.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../../src/vendor/yyjson/yyjson.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static char test_dir[256];
static char log_file_path[512];

// Mock result for PQexecParams failure
static PGresult *mock_failed_result = (PGresult *)1;  // Non-null sentinel
static PGresult *mock_success_result = (PGresult *)2;  // Non-null sentinel for success
static ExecStatusType mock_status = PGRES_FATAL_ERROR;
static bool use_success_mock = false;

// Mock pq_exec_params_ to fail or succeed based on flag
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

    // Return success or failure mock based on flag
    return use_success_mock ? mock_success_result : mock_failed_result;
}

// Mock PQresultStatus to return our configured status
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return mock_status;
    }
    if (res == mock_success_result) {
        return PGRES_COMMAND_OK;
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

    // Create REPL context
    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    r->current = agent;

    r->shared = shared;
    r->current->marks = NULL;
    r->current->mark_count = 0;
    r->shared->db_ctx = NULL;
    r->shared->session_id = 0;

    return r;
}

// Helper to read log file
static char *read_log_file(void)
{
    FILE *f = fopen(log_file_path, "r");
    if (!f) return NULL;

    static char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
    return buffer;
}

// Per-test setup
static void test_setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Set up logger
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_mark_db_test_%d", getpid());
    mkdir(test_dir, 0755);
    ik_log_init(test_dir);
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);

    repl = create_test_repl_with_db(test_ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock status
    mock_status = PGRES_FATAL_ERROR;
    use_success_mock = false;
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        repl = NULL;
    }

    // Cleanup logger
    ik_log_shutdown();
    unlink(log_file_path);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}

// ========== Tests ==========

// Test: DB error during mark persistence with NULL label
START_TEST(test_mark_db_insert_error_with_null_label) {
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = mock_db;
    repl->shared->session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create unlabeled mark - DB insert will fail but command succeeds
    res_t res = ik_cmd_mark(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Mark should still be created in memory
    ck_assert_uint_eq(repl->current->mark_count, 1);
    ck_assert_ptr_null(repl->current->marks[0]->label);

    // Read log output to verify error was logged
    char *log_output = read_log_file();
    ck_assert_ptr_nonnull(log_output);

    // Parse the log output as JSON
    yyjson_doc *parsed = yyjson_read(log_output, strlen(log_output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *level = yyjson_obj_get(root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert_str_eq(yyjson_get_str(level), "warn");

    yyjson_val *logline = yyjson_obj_get(root, "logline");
    ck_assert_ptr_nonnull(logline);

    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "db_persist_failed");

    yyjson_val *operation = yyjson_obj_get(logline, "operation");
    ck_assert_ptr_nonnull(operation);
    ck_assert_str_eq(yyjson_get_str(operation), "persist_mark");

    yyjson_val *error = yyjson_obj_get(logline, "error");
    ck_assert_ptr_nonnull(error);

    yyjson_doc_free(parsed);
}
END_TEST
// Test: DB error during mark persistence with label
START_TEST(test_mark_db_insert_error_with_label)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = mock_db;
    repl->shared->session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create labeled mark - DB insert will fail but command succeeds
    res_t res = ik_cmd_mark(test_ctx, repl, "testlabel");
    ck_assert(is_ok(&res));

    // Mark should still be created in memory
    ck_assert_uint_eq(repl->current->mark_count, 1);
    ck_assert_str_eq(repl->current->marks[0]->label, "testlabel");

    // Read log output to verify error was logged
    char *log_output = read_log_file();
    ck_assert_ptr_nonnull(log_output);

    // Parse the log output as JSON
    yyjson_doc *parsed = yyjson_read(log_output, strlen(log_output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *level = yyjson_obj_get(root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert_str_eq(yyjson_get_str(level), "warn");

    yyjson_val *logline = yyjson_obj_get(root, "logline");
    ck_assert_ptr_nonnull(logline);

    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "db_persist_failed");

    yyjson_val *operation = yyjson_obj_get(logline, "operation");
    ck_assert_ptr_nonnull(operation);
    ck_assert_str_eq(yyjson_get_str(operation), "persist_mark");

    yyjson_val *error = yyjson_obj_get(logline, "error");
    ck_assert_ptr_nonnull(error);

    yyjson_doc_free(parsed);
}

END_TEST
// Test: Rewind error handling when mark not found (lines 132-137)
START_TEST(test_rewind_error_handling)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = mock_db;
    repl->shared->session_id = 1;

    // Create a mark
    res_t res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Try to rewind to a non-existent mark
    res = ik_cmd_rewind(test_ctx, repl, "nonexistent");
    ck_assert(is_ok(&res));  // Command doesn't propagate error

    // Verify error message was added to scrollback
    ck_assert(repl->current->scrollback->count > 0);
}

END_TEST
// Test: DB error during rewind persistence
// Note: This test verifies that rewind works in memory even when DB is unavailable
START_TEST(test_rewind_db_insert_error)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = mock_db;
    repl->shared->session_id = 1;

    // Set mock to fail
    mock_status = PGRES_FATAL_ERROR;

    // Create a mark in memory only (for rewind to work)
    res_t res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Add a message
    ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "test");
    res = ik_agent_add_message(repl->current, msg);
    ck_assert(is_ok(&res));

    // Rewind - should succeed in memory even with DB issues
    res = ik_cmd_rewind(test_ctx, repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Rewind should succeed in memory
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Note: The logger output won't be generated in this test because
    // target_message_id is 0 (no DB query succeeds with mocks), so the
    // db_persist_failed log only happens when target_message_id > 0
}

END_TEST
// Test: DB success during mark persistence (covers line 98 false branch)
START_TEST(test_mark_db_insert_success)
{
    // Set up mock DB context
    ik_db_ctx_t *mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = mock_db;
    repl->shared->session_id = 1;

    // Set mock to succeed
    use_success_mock = true;

    // Create labeled mark - DB insert will succeed
    res_t res = ik_cmd_mark(test_ctx, repl, "success_label");
    ck_assert(is_ok(&res));

    // Mark should be created in memory
    ck_assert_uint_eq(repl->current->mark_count, 1);
    ck_assert_str_eq(repl->current->marks[0]->label, "success_label");

    // Read log file - should be empty or minimal (no error logged)
    char *log_output = read_log_file();
    // Either file doesn't exist or it doesn't contain db_persist_failed
    if (log_output != NULL) {
        ck_assert(strstr(log_output, "db_persist_failed") == NULL);
    }
}

END_TEST

// ========== Suite Configuration ==========

static Suite *commands_mark_db_suite(void)
{
    Suite *s = suite_create("Commands: Mark/Rewind DB");

    // All tests use mocks (no real database)
    TCase *tc_db_errors = tcase_create("Database Error Handling");
    tcase_set_timeout(tc_db_errors, 30);
    tcase_add_checked_fixture(tc_db_errors, test_setup, test_teardown);
    tcase_add_test(tc_db_errors, test_mark_db_insert_error_with_null_label);
    tcase_add_test(tc_db_errors, test_mark_db_insert_error_with_label);
    tcase_add_test(tc_db_errors, test_rewind_error_handling);
    tcase_add_test(tc_db_errors, test_rewind_db_insert_error);
    tcase_add_test(tc_db_errors, test_mark_db_insert_success);
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
