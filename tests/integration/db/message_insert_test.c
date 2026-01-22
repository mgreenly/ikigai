/**
 * @file message_insert_test.c
 * @brief Integration tests for message persistence - all event kinds
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

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

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Per-test teardown
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

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while(0)

// ========== Tests ==========

// Test 1: Clear event (kind='clear', content=NULL, data={})
START_TEST(test_clear_event_insert)
{
    SKIP_IF_NO_DB();

    // Insert clear event
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ck_assert(is_ok(&insert_res));

    // Query to verify event was inserted
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT kind, content, data FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // Verify kind
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "clear");
    // Verify content is NULL
    ck_assert(PQgetisnull(result, 0, 1));
    // Verify data is {} (empty JSON object)
    ck_assert_str_eq(PQgetvalue(result, 0, 2), "{}");

    PQclear(result);
}
END_TEST

// Test 2: System event (kind='system', content='prompt text', data={})
START_TEST(test_system_event_insert)
{
    SKIP_IF_NO_DB();

    // Insert system event
    const char *prompt = "You are a helpful assistant";
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "system", prompt, "{}");
    ck_assert(is_ok(&insert_res));

    // Query and verify
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT kind, content, data FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    ck_assert_str_eq(PQgetvalue(result, 0, 0), "system");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), prompt);
    ck_assert_str_eq(PQgetvalue(result, 0, 2), "{}");

    PQclear(result);
}
END_TEST

// Test 3: User event with model data
START_TEST(test_user_event_insert)
{
    SKIP_IF_NO_DB();

    // Insert user event with JSONB data
    const char *message = "What is the meaning of life?";
    const char *data = "{\"model\":\"gpt-4\",\"temperature\":1.0,\"max_tokens\":2000}";
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "user", message, data);
    ck_assert(is_ok(&insert_res));

    // Query and verify
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT kind, content, data FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    ck_assert_str_eq(PQgetvalue(result, 0, 0), "user");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), message);

    // Verify JSONB data round-trip
    const char *retrieved_data = PQgetvalue(result, 0, 2);
    // PostgreSQL may reformat JSON, but keys should be present
    ck_assert(strstr(retrieved_data, "gpt-4") != NULL);
    ck_assert(strstr(retrieved_data, "temperature") != NULL);

    PQclear(result);
}
END_TEST

// Test 4: Assistant event with model and token data
START_TEST(test_assistant_event_insert)
{
    SKIP_IF_NO_DB();

    // Insert assistant event with metadata
    const char *response = "42 is the answer to everything";
    const char *data = "{\"model\":\"gpt-4\",\"tokens\":150,\"finish_reason\":\"stop\"}";
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "assistant", response, data);
    ck_assert(is_ok(&insert_res));

    // Query and verify
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT kind, content, data FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    ck_assert_str_eq(PQgetvalue(result, 0, 0), "assistant");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), response);

    // Verify JSONB metadata
    const char *retrieved_data = PQgetvalue(result, 0, 2);
    ck_assert(strstr(retrieved_data, "gpt-4") != NULL);
    ck_assert(strstr(retrieved_data, "tokens") != NULL);
    ck_assert(strstr(retrieved_data, "150") != NULL);

    PQclear(result);
}
END_TEST

// Test 5: Mark event with label
START_TEST(test_mark_event_insert)
{
    SKIP_IF_NO_DB();

    // Insert mark event (content=NULL, label in data_json)
    const char *data = "{\"label\":\"approach-a\"}";
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, data);
    ck_assert(is_ok(&insert_res));

    // Query and verify
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT kind, content, data FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    ck_assert_str_eq(PQgetvalue(result, 0, 0), "mark");
    ck_assert(PQgetisnull(result, 0, 1) || strlen(PQgetvalue(result, 0, 1)) == 0);

    // Verify label in JSONB
    const char *retrieved_data = PQgetvalue(result, 0, 2);
    ck_assert(strstr(retrieved_data, "approach-a") != NULL);

    PQclear(result);
}
END_TEST

// Test 6: Rewind event with target_message_id
START_TEST(test_rewind_event_insert)
{
    SKIP_IF_NO_DB();

    // Insert rewind event
    const char *label = "approach-a";
    const char *data = "{\"target_message_id\":42,\"label\":\"approach-a\"}";
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "rewind", label, data);
    ck_assert(is_ok(&insert_res));

    // Query and verify
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT kind, content, data FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    ck_assert_str_eq(PQgetvalue(result, 0, 0), "rewind");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), label);

    // Verify target_message_id in JSONB
    const char *retrieved_data = PQgetvalue(result, 0, 2);
    ck_assert(strstr(retrieved_data, "target_message_id") != NULL);
    ck_assert(strstr(retrieved_data, "42") != NULL);

    PQclear(result);
}
END_TEST

// Test 7: Verify created_at timestamp is set
START_TEST(test_message_has_created_at)
{
    SKIP_IF_NO_DB();

    // Insert a message
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "user", "test", "{}");
    ck_assert(is_ok(&insert_res));

    // Query created_at
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT created_at FROM messages "
                                  "WHERE session_id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // Verify created_at is not NULL
    ck_assert(!PQgetisnull(result, 0, 0));

    PQclear(result);
}
END_TEST

// Test 8: Verify foreign key constraint (session_id references sessions.id)
START_TEST(test_message_foreign_key_constraint)
{
    SKIP_IF_NO_DB();

    // Try to insert message with non-existent session_id
    int64_t invalid_session_id = 999999;
    res_t insert_res = ik_db_message_insert(db, invalid_session_id, NULL, "user", "test", "{}");

    // Should return ERR due to foreign key violation
    ck_assert(is_err(&insert_res));
}
END_TEST

// Test 9: Multiple message insertion preserves order
START_TEST(test_multiple_messages_preserve_order)
{
    SKIP_IF_NO_DB();

    // Insert multiple messages
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "first", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "response1", "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "second", "{}");

    // Query messages ordered by created_at
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT content FROM messages "
                                  "WHERE session_id = %lld "
                                  "ORDER BY created_at",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 4);

    // Verify order (clear has NULL content)
    ck_assert(PQgetisnull(result, 0, 0));
    ck_assert_str_eq(PQgetvalue(result, 1, 0), "first");
    ck_assert_str_eq(PQgetvalue(result, 2, 0), "response1");
    ck_assert_str_eq(PQgetvalue(result, 3, 0), "second");

    PQclear(result);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *message_insert_suite(void)
{
    Suite *s = suite_create("Message Insert");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_clear_event_insert);
    tcase_add_test(tc_core, test_system_event_insert);
    tcase_add_test(tc_core, test_user_event_insert);
    tcase_add_test(tc_core, test_assistant_event_insert);
    tcase_add_test(tc_core, test_mark_event_insert);
    tcase_add_test(tc_core, test_rewind_event_insert);
    tcase_add_test(tc_core, test_message_has_created_at);
    tcase_add_test(tc_core, test_message_foreign_key_constraint);
    tcase_add_test(tc_core, test_multiple_messages_preserve_order);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_insert_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/db/message_insert_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
