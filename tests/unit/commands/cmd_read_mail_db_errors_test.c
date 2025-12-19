/**
 * @file cmd_read_mail_db_errors_test.c
 * @brief Coverage tests for /read-mail command database error paths
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
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

// Mock results
static PGresult *mock_failed_result = (PGresult *)1;

// Override pq_exec_params_ to always fail
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

    return mock_failed_result;
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

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_openai_conversation_t *conv = ik_openai_conversation_create(test_ctx);

    ik_cfg_t *cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;
    agent->conversation = conv;
    agent->uuid = talloc_strdup(agent, "recipient-uuid-123");
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

// Test: /read-mail propagates ik_db_mail_inbox error (line 286)
START_TEST(test_read_mail_db_inbox_error)
{
    res_t res = ik_cmd_read_mail(test_ctx, repl, "1");

    // Should propagate the error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    talloc_free(res.err);
}
END_TEST

// Note: Lines 306, 313, 319, 325 are scrollback_append_line errors
// which are OUT_OF_MEMORY errors from talloc and very difficult to test

static Suite *read_mail_db_errors_suite(void)
{
    Suite *s = suite_create("Read Mail Command DB Errors");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_read_mail_db_inbox_error);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = read_mail_db_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? 0 : 1;
}
