#include "tests/test_constants.h"
/**
 * @file cmd_delete_mail_db_errors_test.c
 * @brief Coverage tests for /delete-mail command database error paths
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"

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

// Mock results
static PGresult *mock_failed_result = (PGresult *)1;
static PGresult *mock_success_result = (PGresult *)2;
static int call_count = 0;

// Override pq_exec_params_ to succeed on first call (inbox fetch), fail on second (delete)
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

    call_count++;
    // First call is inbox fetch - succeed with empty result
    if (call_count == 1) {
        return mock_success_result;
    }
    // Second call is delete - fail with "not found"
    return mock_failed_result;
}

// Override PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    if (res == mock_success_result) {
        return PGRES_TUPLES_OK;
    }
    return PGRES_TUPLES_OK;
}

// Override PQntuples - return 1 for inbox (so position 1 is valid)
int PQntuples(const PGresult *res)
{
    if (res == mock_success_result) {
        return 1;  // One message in inbox
    }
    return 0;
}

// Override PQnfields
int PQnfields(const PGresult *res)
{
    if (res == mock_success_result) {
        return 6;  // id, from_uuid, to_uuid, body, timestamp, read
    }
    return 0;
}

// Override PQgetvalue
char *PQgetvalue(const PGresult *res, int row, int col)
{
    static char id_val[] = "999";
    static char from_val[] = "sender-uuid";
    static char to_val[] = "current-uuid-123";
    static char body_val[] = "Test message";
    static char timestamp_val[] = "1234567890";
    static char read_val[] = "0";
    static char empty_val[] = "";

    if (res == mock_success_result && row == 0) {
        switch (col) {
            case 0: return id_val;
            case 1: return from_val;
            case 2: return to_val;
            case 3: return body_val;
            case 4: return timestamp_val;
            case 5: return read_val;
            default: return empty_val;
        }
    }
    return empty_val;
}

// Override PQgetisnull
int PQgetisnull(const PGresult *res, int row, int col)
{
    (void)res;
    (void)row;
    (void)col;
    return 0;  // No NULLs
}

// Override PQerrorMessage - return "not found" to trigger line 355
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mail not found in database";
    return error_msg;
}

// Override PQclear
void PQclear(PGresult *res)
{
    (void)res;
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

    agent->uuid = talloc_strdup(agent, "current-uuid-123");
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
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

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

// Test: /delete-mail with "not found" error takes error path (line 377)
START_TEST(test_delete_mail_not_found_error_path) {
    // Reset call count for this test
    call_count = 0;

    // Use position 1 (valid in inbox), but delete will fail
    res_t res = ik_cmd_delete_mail(test_ctx, repl, "1");

    // Command should return OK after handling the error
    ck_assert(is_ok(&res));

    // Check that error message was added to scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

static Suite *delete_mail_db_errors_suite(void)
{
    Suite *s = suite_create("Delete Mail Command DB Errors");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_delete_mail_not_found_error_path);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = delete_mail_db_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_delete_mail_db_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? 0 : 1;
}
