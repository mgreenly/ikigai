/**
 * @file cmd_pin_db_test.c
 * @brief Mock-based tests for /pin and /unpin command DB persistence
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper_postgres.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>

static Suite *commands_pin_db_suite(void);

static void *ctx;
static ik_repl_ctx_t *repl;

static PGresult *mock_success_result = (PGresult *)2;

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
    return mock_success_result;
}

ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_success_result) {
        return PGRES_COMMAND_OK;
    }
    return PGRES_FATAL_ERROR;
}

void PQclear(PGresult *res)
{
    (void)res;
}

char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock DB error";
    return error_msg;
}

static ik_repl_ctx_t *create_test_repl_with_db(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->db_ctx = talloc_zero(shared, ik_db_ctx_t);
    ck_assert_ptr_nonnull(shared->db_ctx);
    shared->db_ctx->conn = (PGconn *)1;
    shared->session_id = 1;

    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->pinned_paths = NULL;
    agent->pinned_count = 0;
    agent->shared = shared;
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
}

static void teardown(void)
{
    talloc_free(ctx);
}

START_TEST(test_pin_db_persist_path) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /path/to/doc.md");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->pinned_count, 1);
}
END_TEST

START_TEST(test_unpin_db_persist_path) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /doc.md");
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/unpin /doc.md");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->pinned_count, 0);
}
END_TEST

static Suite *commands_pin_db_suite(void)
{
    Suite *s = suite_create("commands_pin_db");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_pin_db_persist_path);
    tcase_add_test(tc_core, test_unpin_db_persist_path);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_pin_db_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/cmd_pin_db_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
