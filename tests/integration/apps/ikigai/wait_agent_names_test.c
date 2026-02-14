/**
 * @file wait_agent_names_test.c
 * @brief Integration tests for agent name lookup in wait fanin
 *
 * Tests verify that wait fanin correctly retrieves and populates agent names
 * from the database for both named and unnamed agents.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/commands_wait_core.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/session.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static ik_db_ctx_t *worker_db;
static int64_t session_id;
static ik_shared_ctx_t shared_ctx;

// Suite-level setup
static void suite_setup(void)
{
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

// Suite-level teardown
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);

    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    res = ik_test_db_connect(test_ctx, DB_NAME, &worker_db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    shared_ctx.session_id = session_id;
    shared_ctx.db_ctx = db;
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
    }
}

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while(0)

// ========== Helper Functions ==========

static char *create_test_agent(const char *parent_uuid)
{
    static int32_t counter = 0;

    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_asprintf(test_ctx, "test-agent-%d-%ld", ++counter, (long)time(NULL));
    agent.name = NULL;
    agent.parent_uuid = parent_uuid ? talloc_strdup(test_ctx, parent_uuid) : NULL;
    agent.created_at = (int64_t)time(NULL);
    agent.fork_message_id = 0;
    agent.shared = &shared_ctx;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    return agent.uuid;
}

// ========== Tests ==========

// Test 1: Wait fanin verifies agent_name for unnamed agents
START_TEST(test_wait_fanin_unnamed_agents)
{
    SKIP_IF_NO_DB();

    char *waiter = create_test_agent(NULL);
    char *agent1 = create_test_agent(NULL);
    char *agent2 = create_test_agent(NULL);

    res_t res = ik_send_core(test_ctx, db, session_id, agent1, waiter, "msg1", NULL);
    ck_assert(is_ok(&res));

    res = ik_send_core(test_ctx, db, session_id, agent2, waiter, "msg2", NULL);
    ck_assert(is_ok(&res));

    char *target_uuids[] = {agent1, agent2};
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_fanin(test_ctx, worker_db, session_id, waiter, 5, target_uuids, 2, &interrupted, &result);

    ck_assert_int_eq((int)result.entry_count, 2);
    for (size_t i = 0; i < result.entry_count; i++) {
        ck_assert_str_eq(result.entries[i].agent_name, "undefined");
    }
}
END_TEST

// Test 2: Wait fanin verifies agent_name for named agents
START_TEST(test_wait_fanin_named_agents)
{
    SKIP_IF_NO_DB();

    char *waiter = create_test_agent(NULL);
    char *agent1 = create_test_agent(NULL);
    char *agent2 = create_test_agent(NULL);

    char *update1 = talloc_asprintf(test_ctx,
        "UPDATE agents SET name = 'Agent One' WHERE uuid = '%s'", agent1);
    PGresult *pg_result = PQexec(db->conn, update1);
    ck_assert_int_eq(PQresultStatus(pg_result), PGRES_COMMAND_OK);
    PQclear(pg_result);

    char *update2 = talloc_asprintf(test_ctx,
        "UPDATE agents SET name = 'Agent Two' WHERE uuid = '%s'", agent2);
    pg_result = PQexec(db->conn, update2);
    ck_assert_int_eq(PQresultStatus(pg_result), PGRES_COMMAND_OK);
    PQclear(pg_result);

    res_t res = ik_send_core(test_ctx, db, session_id, agent1, waiter, "msg from one", NULL);
    ck_assert(is_ok(&res));

    res = ik_send_core(test_ctx, db, session_id, agent2, waiter, "msg from two", NULL);
    ck_assert(is_ok(&res));

    char *target_uuids[] = {agent1, agent2};
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_fanin(test_ctx, worker_db, session_id, waiter, 5, target_uuids, 2, &interrupted, &result);

    ck_assert_int_eq((int)result.entry_count, 2);

    bool found_agent1 = false;
    bool found_agent2 = false;
    for (size_t i = 0; i < result.entry_count; i++) {
        if (strcmp(result.entries[i].agent_uuid, agent1) == 0) {
            found_agent1 = true;
            ck_assert_str_eq(result.entries[i].agent_name, "Agent One");
        }
        if (strcmp(result.entries[i].agent_uuid, agent2) == 0) {
            found_agent2 = true;
            ck_assert_str_eq(result.entries[i].agent_name, "Agent Two");
        }
    }
    ck_assert(found_agent1);
    ck_assert(found_agent2);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *wait_agent_names_suite(void)
{
    Suite *s = suite_create("Wait Agent Names");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_wait_fanin_unnamed_agents);
    tcase_add_test(tc_core, test_wait_fanin_named_agents);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = wait_agent_names_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
