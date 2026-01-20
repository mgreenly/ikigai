/**
 * @file cmd_persist_test.c
 * @brief Unit tests for slash command output persistence
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/logger.h"
#include "../../../src/shared.h"
#include "../../../src/db/connection.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

// Mock state for tracking ik_db_message_insert calls
static int insert_call_count = 0;
static char *last_kind = NULL;
static char *last_content = NULL;
static char *last_data_json = NULL;

// Mock pq_exec_params_ to succeed and track calls
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
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    insert_call_count++;

    // Track parameters for verification
    // paramValues[2] is kind, paramValues[3] is content, paramValues[4] is data_json
    if (nParams >= 5) {
        if (last_kind) talloc_free(last_kind);
        if (last_content) talloc_free(last_content);
        if (last_data_json) talloc_free(last_data_json);

        last_kind = paramValues[2] ? talloc_strdup(ctx, paramValues[2]) : NULL;
        last_content = paramValues[3] ? talloc_strdup(ctx, paramValues[3]) : NULL;
        last_data_json = paramValues[4] ? talloc_strdup(ctx, paramValues[4]) : NULL;
    }

    // Return success sentinel (cast to avoid incomplete type issue)
    return (PGresult *)0x1;
}

// Mock PQresultStatus to return success
ExecStatusType PQresultStatus(const PGresult *res)
{
    (void)res;
    return PGRES_COMMAND_OK;
}

// Mock PQresultStatus_ (the actual function called by the code)
ExecStatusType PQresultStatus_(const PGresult *res)
{
    (void)res;
    return PGRES_COMMAND_OK;
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
    static char error_msg[] = "No error";
    return error_msg;
}

/**
 * Create a REPL context with database for command persistence testing.
 */
static ik_repl_ctx_t *create_test_repl_with_db(void *parent)
{
    // Create scrollback buffer
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create conversation

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create database context
    ik_db_ctx_t *db_ctx = talloc_zero(parent, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db_ctx);
    db_ctx->conn = (PGconn *)0x1234;  // Fake connection pointer

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db_ctx;
    shared->session_id = 1;

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
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

    repl = create_test_repl_with_db(ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock state
    insert_call_count = 0;
    last_kind = NULL;
    last_content = NULL;
    last_data_json = NULL;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: /help command output is persisted with kind="command"
START_TEST(test_help_persisted) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Should have called ik_db_message_insert once
    ck_assert_int_eq(insert_call_count, 1);

    // Verify kind is "command"
    ck_assert_ptr_nonnull(last_kind);
    ck_assert_str_eq(last_kind, "command");

    // Verify content starts with "/help" and contains output
    ck_assert_ptr_nonnull(last_content);
    ck_assert(strncmp(last_content, "/help\n", 6) == 0);
    ck_assert(strstr(last_content, "Available commands:") != NULL);

    // Verify data_json contains command metadata
    ck_assert_ptr_nonnull(last_data_json);
    ck_assert(strstr(last_data_json, "\"command\":\"help\"") != NULL);
}
END_TEST
// Test: /model command output is persisted
START_TEST(test_model_persisted) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_ok(&res));

    // Should have called DB twice: once for agent update, once for message insert
    ck_assert_int_eq(insert_call_count, 2);

    // Verify kind is "command"
    ck_assert_ptr_nonnull(last_kind);
    ck_assert_str_eq(last_kind, "command");

    // Verify content contains command and output
    ck_assert_ptr_nonnull(last_content);
    ck_assert(strncmp(last_content, "/model gpt-4\n", 13) == 0);
    ck_assert(strstr(last_content, "Switched to") != NULL);

    // Verify data_json contains command metadata
    ck_assert_ptr_nonnull(last_data_json);
    ck_assert(strstr(last_data_json, "\"command\":\"model\"") != NULL);
    ck_assert(strstr(last_data_json, "\"args\":\"gpt-4\"") != NULL);
}

END_TEST
// Test: /debug command output is persisted
START_TEST(test_debug_persisted) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/debug on");
    ck_assert(is_ok(&res));

    // Should have called ik_db_message_insert once
    ck_assert_int_eq(insert_call_count, 1);

    // Verify kind is "command"
    ck_assert_ptr_nonnull(last_kind);
    ck_assert_str_eq(last_kind, "command");

    // Verify content contains command and output
    ck_assert_ptr_nonnull(last_content);
    ck_assert(strncmp(last_content, "/debug on\n", 10) == 0);
    ck_assert(strstr(last_content, "Debug output enabled") != NULL);
}

END_TEST
// Test: Command persistence without database context (should not crash)
START_TEST(test_command_persist_no_db) {
    // Remove database context
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 0;

    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Should not have called ik_db_message_insert
    ck_assert_int_eq(insert_call_count, 0);

    // Command should still execute and show output in scrollback
    ck_assert(ik_scrollback_get_line_count(repl->current->scrollback) > 0);
}

END_TEST
// Test: Unknown command is not persisted
START_TEST(test_unknown_command_not_persisted) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/unknown");
    ck_assert(is_err(&res));
    talloc_free(res.err);

    // Should not have called ik_db_message_insert for error
    ck_assert_int_eq(insert_call_count, 0);
}

END_TEST

static Suite *cmd_persist_suite(void)
{
    Suite *s = suite_create("Command Persistence");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_help_persisted);
    tcase_add_test(tc, test_model_persisted);
    tcase_add_test(tc, test_debug_persisted);
    tcase_add_test(tc, test_command_persist_no_db);
    tcase_add_test(tc, test_unknown_command_not_persisted);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = cmd_persist_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
