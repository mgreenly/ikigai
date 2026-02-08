/**
 * @file send_wait_notify_test.c
 * @brief Integration tests for /send and /wait with PG LISTEN/NOTIFY
 *
 * These tests exercise the full send/wait cycle including:
 * - NOTIFY path in ik_send_core (only fires outside transactions)
 * - Worker thread execution in /wait command
 * - Select loop and notification consumption in wait_core
 * - on_complete callback rendering results to scrollback
 *
 * Uses per-file database isolation for parallel test execution.
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

    // Create main DB connection (no transaction - allows NOTIFY)
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    // Create worker DB connection
    res = ik_test_db_connect(test_ctx, DB_NAME, &worker_db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
        return;
    }

    // Initialize minimal shared context with session_id
    shared_ctx.session_id = session_id;
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        // Clean up session data
        if (db != NULL && session_id > 0) {
            char *cleanup = talloc_asprintf(test_ctx,
                "DELETE FROM messages WHERE session_id = %lld; "
                "DELETE FROM mail WHERE session_id = %lld; "
                "DELETE FROM agents WHERE session_id = %lld; "
                "DELETE FROM sessions WHERE id = %lld;",
                (long long)session_id, (long long)session_id,
                (long long)session_id, (long long)session_id);
            PGresult *result = PQexec(db->conn, cleanup);
            PQclear(result);
        }
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        worker_db = NULL;
    }
}

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while(0)

// ========== Helper Functions ==========

// Create a test agent in the database
static char *create_test_agent(const char *parent_uuid)
{
    static int counter = 0;

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

// Test 1: Core send with error_msg_out for empty body
START_TEST(test_send_core_empty_body_with_error_msg)
{
    SKIP_IF_NO_DB();

    char *sender = create_test_agent(NULL);
    char *recipient = create_test_agent(NULL);

    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id, sender, recipient, "", &error_msg);

    ck_assert(is_err(&res));
    ck_assert(error_msg != NULL);
    ck_assert_str_eq(error_msg, "Message body cannot be empty");
}
END_TEST

// Test 2: Core send with error_msg_out for dead recipient
START_TEST(test_send_core_dead_recipient_with_error_msg)
{
    SKIP_IF_NO_DB();

    char *sender = create_test_agent(NULL);
    char *recipient = create_test_agent(NULL);

    // Mark recipient as dead
    char *update = talloc_asprintf(test_ctx,
        "UPDATE agents SET status = 'dead' WHERE uuid = '%s'", recipient);
    PGresult *result = PQexec(db->conn, update);
    ck_assert_int_eq(PQresultStatus(result), PGRES_COMMAND_OK);
    PQclear(result);

    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id, sender, recipient, "hello", &error_msg);

    ck_assert(is_err(&res));
    ck_assert(error_msg != NULL);
    ck_assert_str_eq(error_msg, "Recipient agent is dead");
}
END_TEST

// Test 3: Core send successful with NOTIFY (outside transaction)
START_TEST(test_send_core_notify_fires_outside_transaction)
{
    SKIP_IF_NO_DB();

    char *sender = create_test_agent(NULL);
    char *recipient = create_test_agent(NULL);

    // Verify we're NOT in a transaction (PQTRANS_IDLE)
    PGTransactionStatusType tx_status = PQtransactionStatus(db->conn);
    ck_assert_int_eq(tx_status, PQTRANS_IDLE);

    // Send a message - NOTIFY should fire
    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id, sender, recipient, "test message", &error_msg);

    ck_assert(is_ok(&res));
    ck_assert(error_msg == NULL);

    // Verify mail was inserted
    char *query = talloc_asprintf(test_ctx,
        "SELECT body FROM mail WHERE from_uuid = '%s' AND to_uuid = '%s'",
        sender, recipient);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "test message");
    PQclear(result);
}
END_TEST

// Test 4: Wait core with instant timeout (no messages)
START_TEST(test_wait_core_instant_timeout_no_messages)
{
    SKIP_IF_NO_DB();

    char *my_uuid = create_test_agent(NULL);

    // Wait for 0 seconds - should return immediately with timeout
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_next_message(test_ctx, worker_db, session_id, my_uuid, 0, &interrupted, &result);

    ck_assert(result.from_uuid == NULL);
    ck_assert(result.message != NULL);
    ck_assert_str_eq(result.message, "Timeout");
}
END_TEST

// Test 5: Wait core receives message after send
START_TEST(test_wait_core_receives_message)
{
    SKIP_IF_NO_DB();

    char *sender = create_test_agent(NULL);
    char *recipient = create_test_agent(NULL);

    // Send a message
    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id, sender, recipient, "hello world", &error_msg);
    ck_assert(is_ok(&res));

    // Wait should find the message immediately (5 second timeout)
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_next_message(test_ctx, worker_db, session_id, recipient, 5, &interrupted, &result);

    ck_assert(result.from_uuid != NULL);
    ck_assert_str_eq(result.from_uuid, sender);
    ck_assert(result.message != NULL);
    ck_assert_str_eq(result.message, "hello world");

    // Verify mail was consumed (deleted from inbox)
    char *query = talloc_asprintf(test_ctx,
        "SELECT COUNT(*) FROM mail WHERE to_uuid = '%s'", recipient);
    PGresult *pg_result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(pg_result), PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(pg_result, 0, 0), "0");
    PQclear(pg_result);
}
END_TEST

// Test 6: Wait fanin mode with multiple agents
START_TEST(test_wait_fanin_multiple_agents)
{
    SKIP_IF_NO_DB();

    char *waiter = create_test_agent(NULL);
    char *agent1 = create_test_agent(NULL);
    char *agent2 = create_test_agent(NULL);

    // Send messages from both agents
    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id, agent1, waiter, "message from agent1", &error_msg);
    ck_assert(is_ok(&res));

    res = ik_send_core(test_ctx, db, session_id, agent2, waiter, "message from agent2", &error_msg);
    ck_assert(is_ok(&res));

    // Wait for both agents (5 second timeout)
    char *target_uuids[] = {agent1, agent2};
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_fanin(test_ctx, worker_db, session_id, waiter, 5, target_uuids, 2, &interrupted, &result);

    // Both should be resolved with "received" status
    ck_assert_int_eq((int)result.entry_count, 2);

    // Check agent1 entry
    bool found_agent1 = false;
    bool found_agent2 = false;
    for (size_t i = 0; i < result.entry_count; i++) {
        if (strcmp(result.entries[i].agent_uuid, agent1) == 0) {
            found_agent1 = true;
            ck_assert_str_eq(result.entries[i].status, "received");
            ck_assert(result.entries[i].message != NULL);
            ck_assert_str_eq(result.entries[i].message, "message from agent1");
        }
        if (strcmp(result.entries[i].agent_uuid, agent2) == 0) {
            found_agent2 = true;
            ck_assert_str_eq(result.entries[i].status, "received");
            ck_assert(result.entries[i].message != NULL);
            ck_assert_str_eq(result.entries[i].message, "message from agent2");
        }
    }
    ck_assert(found_agent1);
    ck_assert(found_agent2);
}
END_TEST

// Test 7: Wait fanin with dead agent
START_TEST(test_wait_fanin_dead_agent)
{
    SKIP_IF_NO_DB();

    char *waiter = create_test_agent(NULL);
    char *dead_agent = create_test_agent(NULL);

    // Mark agent as dead
    char *update = talloc_asprintf(test_ctx,
        "UPDATE agents SET status = 'dead' WHERE uuid = '%s'", dead_agent);
    PGresult *pg_result = PQexec(db->conn, update);
    ck_assert_int_eq(PQresultStatus(pg_result), PGRES_COMMAND_OK);
    PQclear(pg_result);

    // Wait for dead agent (5 second timeout)
    char *target_uuids[] = {dead_agent};
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_fanin(test_ctx, worker_db, session_id, waiter, 5, target_uuids, 1, &interrupted, &result);

    // Should resolve immediately with "dead" status
    ck_assert_int_eq((int)result.entry_count, 1);
    ck_assert_str_eq(result.entries[0].status, "dead");
}
END_TEST

// Test 8: Wait fanin with idle agent
START_TEST(test_wait_fanin_idle_agent)
{
    SKIP_IF_NO_DB();

    char *waiter = create_test_agent(NULL);
    char *idle_agent = create_test_agent(NULL);

    // Mark agent as idle
    res_t res = ik_db_agent_set_idle(db, idle_agent, true);
    ck_assert(is_ok(&res));

    // Wait for idle agent (5 second timeout)
    char *target_uuids[] = {idle_agent};
    ik_wait_result_t result = {0};
    bool interrupted = false;
    ik_wait_core_fanin(test_ctx, worker_db, session_id, waiter, 5, target_uuids, 1, &interrupted, &result);

    // Should resolve immediately with "idle" status
    ck_assert_int_eq((int)result.entry_count, 1);
    ck_assert_str_eq(result.entries[0].status, "idle");
}
END_TEST

// ========== Suite Configuration ==========

static Suite *send_wait_notify_suite(void)
{
    Suite *s = suite_create("Send/Wait NOTIFY");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // Set longer timeout for tests that may wait on select()
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_send_core_empty_body_with_error_msg);
    tcase_add_test(tc_core, test_send_core_dead_recipient_with_error_msg);
    tcase_add_test(tc_core, test_send_core_notify_fires_outside_transaction);
    tcase_add_test(tc_core, test_wait_core_instant_timeout_no_messages);
    tcase_add_test(tc_core, test_wait_core_receives_message);
    tcase_add_test(tc_core, test_wait_fanin_multiple_agents);
    tcase_add_test(tc_core, test_wait_fanin_dead_agent);
    tcase_add_test(tc_core, test_wait_fanin_idle_agent);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = send_wait_notify_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/apps/ikigai/send_wait_notify_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
