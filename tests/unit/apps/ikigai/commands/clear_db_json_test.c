/**
 * @file clear_db_json_test.c
 * @brief Unit tests for /clear command JSON logging error handling
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "shared/logger.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/marks.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

// Mock counter for controlling which DB call fails
static int mock_insert_call_count = 0;
static int mock_insert_fail_on_call = -1;
static PGresult *mock_failed_result = (PGresult *)1;  // Non-null sentinel for failure
static PGresult *mock_success_result = (PGresult *)2;  // Non-null sentinel for success

// Mock counter for controlling yyjson_mut_obj_add_str_ failures
static int mock_add_str_call_count = 0;
static int mock_add_str_fail_on_call = -1;

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

// Mock yyjson_mut_obj_add_str_ to fail on specified call
bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    (void)doc;
    (void)obj;
    (void)key;
    (void)val;

    mock_add_str_call_count++;

    if (mock_add_str_fail_on_call == mock_add_str_call_count) {
        return false;  // Simulate failure
    }

    return true;  // Success
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
    agent->shared = shared;  // Agent needs shared for system prompt fallback
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
    mock_add_str_call_count = 0;
    mock_add_str_fail_on_call = -1;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Clear with DB error and yyjson_mut_obj_add_str_ failure in clear event logging
START_TEST(test_clear_db_error_json_add_fail_clear) {
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
    repl->current->shared = repl->shared;  // Agent needs shared for system prompt fallback

    // Mock DB to return error on first call (clear event)
    mock_insert_fail_on_call = 1;

    // Mock yyjson_mut_obj_add_str_ to fail on first call (line 72)
    mock_add_str_call_count = 0;
    mock_add_str_fail_on_call = 1;

    // Execute /clear - should succeed despite JSON logging failure
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear still happened
    // Scrollback is empty (system message stored but not displayed)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);

}
END_TEST
// Test: Clear with system message DB error and yyjson_mut_obj_add_str_ failure
START_TEST(test_clear_system_db_error_json_add_fail) {
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
    repl->current->shared = repl->shared;  // Agent needs shared for system prompt fallback

    // Mock DB to return error on second call (system message)
    mock_insert_fail_on_call = 2;

    // Mock yyjson_mut_obj_add_str_ to fail on first call (line 94)
    mock_add_str_call_count = 0;
    mock_add_str_fail_on_call = 1;

    // Execute /clear - should succeed despite JSON logging failure
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify clear happened
    // Scrollback is empty (system message stored but not displayed)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST

static Suite *commands_clear_db_json_suite(void)
{
    Suite *s = suite_create("Commands/Clear DB JSON");
    TCase *tc = tcase_create("JSON Logging Errors");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_clear_db_error_json_add_fail_clear);
    tcase_add_test(tc, test_clear_system_db_error_json_add_fail);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_clear_db_json_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/clear_db_json_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
