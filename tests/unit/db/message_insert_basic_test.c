#include "../../../src/db/message.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

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

// ========== Tests ==========

// Test: Insert clear event with NULL content
START_TEST(test_db_message_insert_clear_event) {
    SKIP_IF_NO_DB();

    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, content FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "clear");
    ck_assert(PQgetisnull(result, 0, 1));

    PQclear(result);
}
END_TEST
// Test: Insert system event with content
START_TEST(test_db_message_insert_system_event)
{
    SKIP_IF_NO_DB();

    const char *system_prompt = "You are a helpful assistant.";
    const char *data_json = "{}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "system", system_prompt, data_json);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, content FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "system");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), system_prompt);

    PQclear(result);
}

END_TEST
// Test: Insert user event with JSONB data
START_TEST(test_db_message_insert_user_event)
{
    SKIP_IF_NO_DB();

    const char *user_msg = "Hello, how are you?";
    const char *data_json = "{\"model\":\"gpt-4\",\"temperature\":1.0,\"max_completion_tokens\":4096}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "user", user_msg, data_json);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "user");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), user_msg);

    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "gpt-4") != NULL);
    ck_assert(strstr(json_result, "temperature") != NULL);

    PQclear(result);
}

END_TEST
// Test: Insert assistant event with response metadata
START_TEST(test_db_message_insert_assistant_event)
{
    SKIP_IF_NO_DB();

    const char *assistant_msg = "I'm doing well, thank you!";
    const char *data_json = "{\"model\":\"gpt-4\",\"tokens\":150,\"finish_reason\":\"stop\"}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "assistant", assistant_msg, data_json);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "assistant");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), assistant_msg);

    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "tokens") != NULL);
    ck_assert(strstr(json_result, "150") != NULL);

    PQclear(result);
}

END_TEST
// Test: Insert mark event with label
START_TEST(test_db_message_insert_mark_event)
{
    SKIP_IF_NO_DB();

    const char *mark_label = "approach-a";
    const char *data_json = "{\"label\":\"approach-a\"}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "mark", mark_label, data_json);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, content FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "mark");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), mark_label);

    PQclear(result);
}

END_TEST
// Test: Insert rewind event with target reference
START_TEST(test_db_message_insert_rewind_event)
{
    SKIP_IF_NO_DB();

    const char *target_label = "approach-a";
    const char *data_json = "{\"target_message_id\":42,\"target_label\":\"approach-a\"}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "rewind", target_label, data_json);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "rewind");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), target_label);

    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "target_message_id") != NULL);

    PQclear(result);
}

END_TEST
// Test: Insert with empty string content
START_TEST(test_db_message_insert_empty_content)
{
    SKIP_IF_NO_DB();

    res_t res = ik_db_message_insert(db, session_id, NULL, "user", "", NULL);
    ck_assert(is_ok(&res));

    const char *query = "SELECT content FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(!PQgetisnull(result, 0, 0));
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "");

    PQclear(result);
}

END_TEST
// Test: Insert with NULL data field
START_TEST(test_db_message_insert_null_data)
{
    SKIP_IF_NO_DB();

    res_t res = ik_db_message_insert(db, session_id, NULL, "system", "Test", NULL);
    ck_assert(is_ok(&res));

    const char *query = "SELECT data FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(PQgetisnull(result, 0, 0));

    PQclear(result);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *message_insert_basic_suite(void)
{
    Suite *s = suite_create("Message Insert Basic");

    TCase *tc_insert = tcase_create("Insert");
    tcase_set_timeout(tc_insert, 30);

    tcase_add_unchecked_fixture(tc_insert, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_insert, test_setup, test_teardown);

    tcase_add_test(tc_insert, test_db_message_insert_clear_event);
    tcase_add_test(tc_insert, test_db_message_insert_system_event);
    tcase_add_test(tc_insert, test_db_message_insert_user_event);
    tcase_add_test(tc_insert, test_db_message_insert_assistant_event);
    tcase_add_test(tc_insert, test_db_message_insert_mark_event);
    tcase_add_test(tc_insert, test_db_message_insert_rewind_event);
    tcase_add_test(tc_insert, test_db_message_insert_empty_content);
    tcase_add_test(tc_insert, test_db_message_insert_null_data);
    suite_add_tcase(s, tc_insert);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_insert_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
