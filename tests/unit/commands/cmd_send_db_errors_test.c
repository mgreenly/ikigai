/**
 * @file cmd_send_db_errors_test.c
 * @brief Coverage tests for /send command database error paths
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/mail.h"
#include "../../../src/error.h"
#include "../../../src/mail/msg.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Test fixtures
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static ik_db_ctx_t *db;

// Mock control flags
static bool mock_agent_get_fail = false;
static bool mock_mail_insert_fail = false;

// Mock results
static PGresult *mock_failed_result = (PGresult *)1;
static PGresult *mock_success_result = (PGresult *)2;

// Override pq_exec_params_ to control failures
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
    (void)nParams;
    (void)paramTypes;
    (void)paramValues;
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    // Check if this is an agent_get query
    if (command != NULL && strstr(command,
                                  "SELECT uuid, name, parent_uuid, fork_message_id, status") != NULL && strstr(command,
                                                                                                               "FROM agents WHERE uuid")
        != NULL) {
        if (mock_agent_get_fail) {
            return mock_failed_result;
        }
    }

    // Check if this is a mail insert query
    if (command != NULL && strstr(command, "INSERT INTO mail") != NULL) {
        if (mock_mail_insert_fail) {
            return mock_failed_result;
        }
    }

    return mock_success_result;
}

// Override PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    return PGRES_TUPLES_OK;
}

// Override PQerrorMessage
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock database error";
    return error_msg;
}

// Override PQclear
void PQclear(PGresult *res)
{
    (void)res;
}

// Override PQntuples - for agent_get success case
int PQntuples(const PGresult *res)
{
    if (res == mock_success_result) {
        return 1;
    }
    return 0;
}

// Override PQgetvalue - return minimal agent data
char *PQgetvalue(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;

    static char uuid[] = "recipient-uuid-456";
    static char name[] = "";
    static char parent_uuid[] = "";
    static char fork_message_id[] = "0";
    static char status[] = "running";
    static char created_at[] = "1234567891";
    static char ended_at[] = "0";

    // Note: ik_db_agent_get query returns columns in this order:
    // uuid, name, parent_uuid, fork_message_id, status, created_at, ended_at
    switch (column_number) {
        case 0: return uuid;
        case 1: return name;
        case 2: return parent_uuid;
        case 3: return fork_message_id;
        case 4: return status;
        case 5: return created_at;
        case 6: return ended_at;
        default: { static char empty[] = ""; return empty; }
    }
}

// Override PQgetisnull
int PQgetisnull(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;

    // name and parent_uuid are null
    if (column_number == 1 || column_number == 2) {
        return 1;
    }
    return 0;
}

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;

    agent->uuid = talloc_strdup(agent, "sender-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

    // Create mock db context
    db = talloc_zero(test_ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(db);
    db->conn = (PGconn *)0xDEADBEEF; // Non-NULL placeholder

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    shared->session_id = 1;
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Add recipient to agents array
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "recipient-uuid-456");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567891;
    recipient->fork_message_id = 0;
    repl->agents[repl->agent_count++] = recipient;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Reset mock flags
    mock_agent_get_fail = false;
    mock_mail_insert_fail = false;

    setup_repl();
}

static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    db = NULL;
    repl = NULL;
}

// Test: /send propagates ik_db_agent_get error (line 110)
START_TEST(test_send_db_agent_get_error) {
    // Enable agent_get failure
    mock_agent_get_fail = true;

    res_t res = ik_cmd_send(test_ctx, repl, "recipient-uuid-456 \"Test message\"");

    // Should propagate the error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    talloc_free(res.err);
}
END_TEST
// Test: /send propagates ik_db_mail_insert error (line 136)
START_TEST(test_send_db_mail_insert_error)
{
    // Enable mail_insert failure
    mock_mail_insert_fail = true;

    res_t res = ik_cmd_send(test_ctx, repl, "recipient-uuid-456 \"Test message\"");

    // Should propagate the error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    talloc_free(res.err);
}

END_TEST

static Suite *send_db_errors_suite(void)
{
    Suite *s = suite_create("Send Command DB Errors");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_send_db_agent_get_error);
    tcase_add_test(tc, test_send_db_mail_insert_error);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = send_db_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? 0 : 1;
}
