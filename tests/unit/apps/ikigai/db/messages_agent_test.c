#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/session.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========
// Each test file gets its own database for parallel execution

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup: Create and migrate database (runs once)
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

// Suite-level teardown: Destroy database (runs once)
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup: Connect, begin transaction, create session
static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    // Create a session for message tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Per-test teardown: Rollback and cleanup
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        if (db != NULL) {
            ik_test_db_rollback(db);
        }
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Schema Tests ==========

// Test: messages table has agent_uuid column after migration
START_TEST(test_messages_has_agent_uuid_column) {
    SKIP_IF_NO_DB();

    // Query information_schema to check for column
    const char *query =
        "SELECT column_name, data_type, is_nullable "
        "FROM information_schema.columns "
        "WHERE table_name = 'messages' AND column_name = 'agent_uuid'";

    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // Verify column type is TEXT
    ck_assert_str_eq(PQgetvalue(result, 0, 1), "text");

    // Verify column is nullable (for existing data compatibility)
    ck_assert_str_eq(PQgetvalue(result, 0, 2), "YES");

    PQclear(result);
}
END_TEST
// Test: agent_uuid references agents(uuid) with FK constraint
START_TEST(test_agent_uuid_fk_constraint) {
    SKIP_IF_NO_DB();

    // Query pg_constraint to check for FK
    const char *query =
        "SELECT tc.constraint_name, ccu.table_name AS foreign_table_name, "
        "ccu.column_name AS foreign_column_name "
        "FROM information_schema.table_constraints AS tc "
        "JOIN information_schema.constraint_column_usage AS ccu "
        "ON ccu.constraint_name = tc.constraint_name "
        "WHERE tc.constraint_type = 'FOREIGN KEY' "
        "AND tc.table_name = 'messages' "
        "AND EXISTS (SELECT 1 FROM information_schema.key_column_usage kcu "
        "            WHERE kcu.constraint_name = tc.constraint_name "
        "            AND kcu.column_name = 'agent_uuid')";

    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // Verify FK references agents(uuid)
    ck_assert_str_eq(PQgetvalue(result, 0, 1), "agents");
    ck_assert_str_eq(PQgetvalue(result, 0, 2), "uuid");

    PQclear(result);
}

END_TEST
// Test: idx_messages_agent index exists
START_TEST(test_idx_messages_agent_exists) {
    SKIP_IF_NO_DB();

    // Query pg_indexes to check for index
    const char *query =
        "SELECT indexname FROM pg_indexes "
        "WHERE tablename = 'messages' AND indexname = 'idx_messages_agent'";

    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    PQclear(result);
}

END_TEST
// ========== Insert Tests ==========

// Test: message insert with agent_uuid succeeds
START_TEST(test_message_insert_with_agent_uuid) {
    SKIP_IF_NO_DB();

    // First insert an agent into agents table
    const char *insert_agent =
        "INSERT INTO agents (uuid, status, created_at) "
        "VALUES ('test-agent-uuid-12345', 'running', 1234567890)";

    PGresult *agent_result = PQexec(db->conn, insert_agent);
    ck_assert_int_eq(PQresultStatus(agent_result), PGRES_COMMAND_OK);
    PQclear(agent_result);

    // Now insert message with agent_uuid
    res_t res = ik_db_message_insert(db, session_id, "test-agent-uuid-12345",
                                     "user", "Hello from agent", NULL);
    ck_assert(is_ok(&res));

    // Verify message was inserted with agent_uuid
    const char *query =
        "SELECT agent_uuid FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "test-agent-uuid-12345");

    PQclear(result);
}

END_TEST
// Test: message insert with NULL agent_uuid succeeds (backward compatibility)
START_TEST(test_message_insert_null_agent_uuid) {
    SKIP_IF_NO_DB();

    // Insert message with NULL agent_uuid
    res_t res = ik_db_message_insert(db, session_id, NULL,
                                     "user", "Hello without agent", NULL);
    ck_assert(is_ok(&res));

    // Verify message was inserted with NULL agent_uuid
    const char *query =
        "SELECT agent_uuid FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(PQgetisnull(result, 0, 0));

    PQclear(result);
}

END_TEST
// Test: query by agent_uuid returns correct subset
START_TEST(test_query_by_agent_uuid) {
    SKIP_IF_NO_DB();

    // Insert two agents
    PGresult *r1 = PQexec(db->conn,
                          "INSERT INTO agents (uuid, status, created_at) "
                          "VALUES ('agent-1', 'running', 1234567890)");
    ck_assert_int_eq(PQresultStatus(r1), PGRES_COMMAND_OK);
    PQclear(r1);

    PGresult *r2 = PQexec(db->conn,
                          "INSERT INTO agents (uuid, status, created_at) "
                          "VALUES ('agent-2', 'running', 1234567890)");
    ck_assert_int_eq(PQresultStatus(r2), PGRES_COMMAND_OK);
    PQclear(r2);

    // Insert messages for each agent
    res_t res1 = ik_db_message_insert(db, session_id, "agent-1", "user", "Msg1", NULL);
    ck_assert(is_ok(&res1));

    res_t res2 = ik_db_message_insert(db, session_id, "agent-2", "user", "Msg2", NULL);
    ck_assert(is_ok(&res2));

    res_t res3 = ik_db_message_insert(db, session_id, "agent-1", "assistant", "Response1", NULL);
    ck_assert(is_ok(&res3));

    // Query for agent-1 only
    const char *query =
        "SELECT content FROM messages WHERE agent_uuid = $1 ORDER BY id";
    const char *params[] = {"agent-1"};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 2);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "Msg1");
    ck_assert_str_eq(PQgetvalue(result, 1, 0), "Response1");

    PQclear(result);
}

END_TEST
// Test: query with agent_uuid and id range works (replay algorithm)
START_TEST(test_query_agent_uuid_with_range) {
    SKIP_IF_NO_DB();

    // Insert an agent
    PGresult *r1 = PQexec(db->conn,
                          "INSERT INTO agents (uuid, status, created_at) "
                          "VALUES ('range-agent', 'running', 1234567890)");
    ck_assert_int_eq(PQresultStatus(r1), PGRES_COMMAND_OK);
    PQclear(r1);

    // Insert multiple messages
    res_t res1 = ik_db_message_insert(db, session_id, "range-agent", "user", "Msg1", NULL);
    ck_assert(is_ok(&res1));

    res_t res2 = ik_db_message_insert(db, session_id, "range-agent", "assistant", "Msg2", NULL);
    ck_assert(is_ok(&res2));

    res_t res3 = ik_db_message_insert(db, session_id, "range-agent", "user", "Msg3", NULL);
    ck_assert(is_ok(&res3));

    res_t res4 = ik_db_message_insert(db, session_id, "range-agent", "assistant", "Msg4", NULL);
    ck_assert(is_ok(&res4));

    // Get message IDs
    const char *id_query =
        "SELECT id FROM messages WHERE agent_uuid = $1 ORDER BY id";
    const char *id_params[] = {"range-agent"};
    PGresult *id_result = PQexecParams(db->conn, id_query, 1, NULL, id_params, NULL, NULL, 0);
    ck_assert_int_eq(PQntuples(id_result), 4);

    int64_t id1 = atoll(PQgetvalue(id_result, 0, 0));
    int64_t id3 = atoll(PQgetvalue(id_result, 2, 0));
    PQclear(id_result);

    // Query with range: id > id1 AND id <= id3 (should return Msg2, Msg3)
    const char *range_query =
        "SELECT content FROM messages "
        "WHERE agent_uuid = $1 AND id > $2 AND ($3::bigint = 0 OR id <= $3::bigint) "
        "ORDER BY created_at";

    char start_id_str[32], end_id_str[32];
    snprintf(start_id_str, sizeof(start_id_str), "%lld", (long long)id1);
    snprintf(end_id_str, sizeof(end_id_str), "%lld", (long long)id3);

    const char *range_params[] = {"range-agent", start_id_str, end_id_str};
    PGresult *range_result = PQexecParams(db->conn, range_query, 3, NULL, range_params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(range_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(range_result), 2);
    ck_assert_str_eq(PQgetvalue(range_result, 0, 0), "Msg2");
    ck_assert_str_eq(PQgetvalue(range_result, 1, 0), "Msg3");

    PQclear(range_result);
}

END_TEST
// ========== Event Kind Tests ==========

// Test: "agent_killed" is valid event kind
START_TEST(test_agent_killed_is_valid_kind) {
    SKIP_IF_NO_DB();

    ck_assert(ik_db_message_is_valid_kind("agent_killed"));
}

END_TEST
// Test: message insert with kind="agent_killed" succeeds
START_TEST(test_message_insert_agent_killed) {
    SKIP_IF_NO_DB();

    // Insert an agent
    PGresult *r1 = PQexec(db->conn,
                          "INSERT INTO agents (uuid, status, created_at) "
                          "VALUES ('killed-agent', 'running', 1234567890)");
    ck_assert_int_eq(PQresultStatus(r1), PGRES_COMMAND_OK);
    PQclear(r1);

    // Insert agent_killed event
    const char *data_json = "{\"killed_by\":\"user\",\"target\":\"killed-agent\"}";
    res_t res = ik_db_message_insert(db, session_id, "killed-agent",
                                     "agent_killed", NULL, data_json);
    ck_assert(is_ok(&res));

    // Verify message was inserted
    const char *query =
        "SELECT kind, data::text FROM messages WHERE session_id = $1 AND kind = 'agent_killed'";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "agent_killed");

    const char *json_result = PQgetvalue(result, 0, 1);
    ck_assert(strstr(json_result, "killed_by") != NULL);
    ck_assert(strstr(json_result, "user") != NULL);

    PQclear(result);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *messages_agent_suite(void)
{
    Suite *s = suite_create("Messages Agent UUID");

    TCase *tc_schema = tcase_create("Schema");
    tcase_set_timeout(tc_schema, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_schema, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_schema, test_setup, test_teardown);
    tcase_add_test(tc_schema, test_messages_has_agent_uuid_column);
    tcase_add_test(tc_schema, test_agent_uuid_fk_constraint);
    tcase_add_test(tc_schema, test_idx_messages_agent_exists);
    suite_add_tcase(s, tc_schema);

    TCase *tc_insert = tcase_create("Insert");
    tcase_set_timeout(tc_insert, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_insert, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_insert, test_setup, test_teardown);
    tcase_add_test(tc_insert, test_message_insert_with_agent_uuid);
    tcase_add_test(tc_insert, test_message_insert_null_agent_uuid);
    tcase_add_test(tc_insert, test_query_by_agent_uuid);
    tcase_add_test(tc_insert, test_query_agent_uuid_with_range);
    suite_add_tcase(s, tc_insert);

    TCase *tc_kinds = tcase_create("Event Kinds");
    tcase_set_timeout(tc_kinds, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_kinds, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_kinds, test_setup, test_teardown);
    tcase_add_test(tc_kinds, test_agent_killed_is_valid_kind);
    tcase_add_test(tc_kinds, test_message_insert_agent_killed);
    suite_add_tcase(s, tc_kinds);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = messages_agent_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/messages_agent_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
