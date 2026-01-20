/**
 * @file clear_db_test.c
 * @brief Unit tests for /clear command database error handling
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/logger.h"
#include "../../../src/shared.h"
#include "../../../src/db/connection.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/marks.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

// Mock counter for controlling which call fails
static int mock_insert_call_count = 0;
static int mock_insert_fail_on_call = -1;
static PGresult *mock_failed_result = (PGresult *)1;  // Non-null sentinel for failure
static PGresult *mock_success_result = (PGresult *)2;  // Non-null sentinel for success

// Mock pq_exec_params_ to fail on specified call
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

    mock_insert_call_count++;

    if (mock_insert_fail_on_call == mock_insert_call_count) {
        return mock_failed_result;
    }

    // Return success for all other calls
    return mock_success_result;
}

// Mock PQresultStatus_ to return error for our mock
ExecStatusType PQresultStatus_(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    if (res == mock_success_result) {
        return PGRES_COMMAND_OK;
    }
    // Should not happen in our tests
    return PGRES_FATAL_ERROR;
}

// Mock PQclear (no-op)
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

// Mock posix_rename_ (logger rotation)
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;  // Success
}

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/**
 * Create a REPL context with scrollback for clear testing.
 */
static ik_repl_ctx_t *create_test_repl_with_conversation(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    // Create logger (required by /clear command)
    shared->logger = ik_logger_create(parent, ".");

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context (messages array starts empty)
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    r->current = agent;

    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_conversation(ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock state
    mock_insert_call_count = 0;
    mock_insert_fail_on_call = -1;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Helper: Setup DB context and debug pipe with write_end
static void setup_db_and_pipe(ik_repl_ctx_t *r, ik_config_t *cfg, int *pipefd)
{
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db_ctx);
    db_ctx->conn = (PGconn *)0x1234;

    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);

    ck_assert_int_eq(pipe(pipefd), 0);
    debug_pipe->write_end = fdopen(pipefd[1], "w");
    ck_assert_ptr_nonnull(debug_pipe->write_end);

    r->shared->cfg = cfg;
    r->shared->db_ctx = db_ctx;
    r->shared->session_id = 1;
    r->shared->db_debug_pipe = debug_pipe;
}

// Test: Clear with database error on clear event persist
START_TEST(test_clear_db_error_clear_event) {
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;

    int pipefd[2];
    setup_db_and_pipe(repl, cfg, pipefd);

    mock_insert_fail_on_call = 1;

    // Execute /clear - should log error but not fail
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear still happened despite DB error
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Clean up
    fclose(repl->shared->db_debug_pipe->write_end);
    close(pipefd[0]);
}
END_TEST
// Test: Clear with database error on system message persist
START_TEST(test_clear_db_error_system_message) {
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant");

    int pipefd[2];
    setup_db_and_pipe(repl, cfg, pipefd);

    mock_insert_fail_on_call = 2;

    // Execute /clear - should log error but not fail
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear still happened despite DB error
    // System message should be displayed in scrollback (with blank line after)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Clean up
    fclose(repl->shared->db_debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
// Test: Clear with system message successfully persisted to database
START_TEST(test_clear_db_success_system_message) {
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant");

    int pipefd[2];
    setup_db_and_pipe(repl, cfg, pipefd);

    mock_insert_fail_on_call = -1;

    // Execute /clear - should succeed with system message persisted
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened successfully
    // System message should be displayed in scrollback (with blank line after)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Clean up
    fclose(repl->shared->db_debug_pipe->write_end);
    close(pipefd[0]);
}

END_TEST
// Test: Clear without database context (no persistence)
START_TEST(test_clear_without_db_ctx) {
    // No database context set (db_ctx is NULL)
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 0;

    // Add content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Execute /clear - should succeed without attempting DB operations
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with DB error but no debug pipe (silent failure)
START_TEST(test_clear_db_error_no_debug_pipe) {
    // Create minimal config (no system message)
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;

    // Set up database context and session with proper mock structure
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db_ctx);
    db_ctx->conn = (PGconn *)0x1234;  // Fake connection pointer

    // Update repl->shared with all required fields
    repl->shared->cfg = cfg;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // No debug pipe set - db_debug_pipe is NULL
    repl->shared->db_debug_pipe = NULL;

    // Mock will return error on first call (clear event)
    mock_insert_fail_on_call = 1;

    // Execute /clear - should succeed without attempting to log error
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened despite DB error and no error logging
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with system message DB error but no debug pipe
START_TEST(test_clear_system_db_error_no_debug_pipe) {
    // Create config with system message
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are helpful");

    // Set up database context and session with proper mock structure
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db_ctx);
    db_ctx->conn = (PGconn *)0x1234;  // Fake connection pointer

    // Update repl->shared with all required fields
    repl->shared->cfg = cfg;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // No debug pipe set - db_debug_pipe is NULL
    repl->shared->db_debug_pipe = NULL;

    // Mock will return error on second call (system message)
    mock_insert_fail_on_call = 2;

    // Execute /clear - should succeed without attempting to log error
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened
    // System message should be displayed in scrollback (with blank line after)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with DB error and debug pipe but write_end is NULL
START_TEST(test_clear_db_error_write_end_null) {
    // Create minimal config
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;

    // Set up database context and session with proper mock structure
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db_ctx);
    db_ctx->conn = (PGconn *)0x1234;  // Fake connection pointer

    // Create debug pipe but with NULL write_end
    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = NULL;  // NULL write_end

    // Update repl->shared with all required fields
    repl->shared->cfg = cfg;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;
    repl->shared->db_debug_pipe = debug_pipe;

    // Mock will return error on first call (clear event)
    mock_insert_fail_on_call = 1;

    // Execute /clear - should succeed without attempting to log error
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with system message DB error and write_end is NULL
START_TEST(test_clear_system_db_error_write_end_null) {
    // Create config with system message
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are helpful");

    // Set up database context and session with proper mock structure
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db_ctx);
    db_ctx->conn = (PGconn *)0x1234;  // Fake connection pointer

    // Create debug pipe but with NULL write_end
    ik_debug_pipe_t *debug_pipe = talloc_zero(ctx, ik_debug_pipe_t);
    ck_assert_ptr_nonnull(debug_pipe);
    debug_pipe->write_end = NULL;  // NULL write_end

    // Update repl->shared with all required fields
    repl->shared->cfg = cfg;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;
    repl->shared->db_debug_pipe = debug_pipe;

    // Mock will return error on second call (system message)
    mock_insert_fail_on_call = 2;

    // Execute /clear - should succeed without attempting to log error
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened
    // System message should be displayed in scrollback (with blank line after)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with session_id <= 0 (no DB persistence)
START_TEST(test_clear_with_invalid_session_id) {
    // Set up database context but invalid session_id
    ik_db_ctx_t *db_ctx = (ik_db_ctx_t *)0x1234;  // Fake pointer
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 0;  // Invalid session ID

    // Add content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Execute /clear - should succeed without attempting DB operations
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST

static Suite *commands_clear_db_suite(void)
{
    Suite *s = suite_create("Commands/Clear DB");
    TCase *tc = tcase_create("Database Errors");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_clear_db_error_clear_event);
    tcase_add_test(tc, test_clear_db_error_system_message);
    tcase_add_test(tc, test_clear_db_success_system_message);
    tcase_add_test(tc, test_clear_without_db_ctx);
    tcase_add_test(tc, test_clear_db_error_no_debug_pipe);
    tcase_add_test(tc, test_clear_system_db_error_no_debug_pipe);
    tcase_add_test(tc, test_clear_db_error_write_end_null);
    tcase_add_test(tc, test_clear_system_db_error_write_end_null);
    tcase_add_test(tc, test_clear_with_invalid_session_id);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_clear_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
