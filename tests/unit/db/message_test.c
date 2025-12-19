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

    // Insert clear event
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Verify message was inserted
    const char *query = "SELECT kind, content, data FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "clear");
    ck_assert(PQgetisnull(result, 0, 1)); // content should be NULL

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

    // Verify message was inserted
    const char *query = "SELECT kind, content, data FROM messages WHERE session_id = $1";
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

    // Verify JSONB data round-trip
    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "user");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), user_msg);

    // Verify JSONB contains expected keys
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

    // Verify message was inserted
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

    // Verify message was inserted
    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
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

    // Verify message was inserted
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

    // Verify empty string was inserted (not NULL)
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

    // Verify NULL data was inserted
    const char *query = "SELECT data FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(PQgetisnull(result, 0, 0));

    PQclear(result);
}

END_TEST
// Test: Insert tool_call event with tool details
START_TEST(test_db_message_insert_tool_call_event)
{
    SKIP_IF_NO_DB();

    const char *tool_call_content = "glob(pattern='*.c', path='src/')";
    const char *data_json =
        "{\"id\":\"call_abc123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"*.c\\\",\\\"path\\\":\\\"src/\\\"}\"}}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "tool_call", tool_call_content, data_json);
    ck_assert(is_ok(&res));

    // Verify message was inserted
    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "tool_call");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), tool_call_content);

    // Verify JSONB contains tool call details
    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "call_abc123") != NULL);
    ck_assert(strstr(json_result, "glob") != NULL);

    PQclear(result);
}

END_TEST
// Test: Insert tool_result event with execution result
START_TEST(test_db_message_insert_tool_result_event)
{
    SKIP_IF_NO_DB();

    const char *tool_result_content = "3 files found";
    const char *data_json =
        "{\"tool_call_id\":\"call_abc123\",\"name\":\"glob\",\"output\":\"src/main.c\\nsrc/config.c\\nsrc/repl.c\",\"success\":true}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "tool_result", tool_result_content, data_json);
    ck_assert(is_ok(&res));

    // Verify message was inserted
    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "tool_result");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), tool_result_content);

    // Verify JSONB contains tool result details
    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "call_abc123") != NULL);
    ck_assert(strstr(json_result, "glob") != NULL);
    ck_assert(strstr(json_result, "success") != NULL);

    PQclear(result);
}

END_TEST
// Test: Insert with non-existent session_id triggers foreign key constraint
// Bug 9 regression test: error message must be accessible without crash
START_TEST(test_db_message_insert_fk_constraint)
{
    SKIP_IF_NO_DB();

    // Try to insert message with non-existent session_id
    res_t res = ik_db_message_insert(db, 99999, NULL, "user", "test", NULL);
    ck_assert(is_err(&res));

    // Bug 9: Accessing error message must not crash
    // The old code allocated error on tmp context then freed tmp,
    // leaving a dangling pointer that crashed when accessed
    const char *msg = error_message(res.err);
    ck_assert(msg != NULL);
    ck_assert(strlen(msg) > 0);

    // Error must be properly allocated on db context so we can free it
    talloc_free(res.err);
}

END_TEST
// Test: Multiple inserts maintain chronological order
START_TEST(test_db_message_insert_multiple_events)
{
    SKIP_IF_NO_DB();

    // Insert multiple events
    res_t res1 = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res1));

    res_t res2 = ik_db_message_insert(db, session_id, NULL, "system", "System prompt", "{}");
    ck_assert(is_ok(&res2));

    res_t res3 = ik_db_message_insert(db, session_id, NULL, "user", "Hello", "{\"model\":\"gpt-4\"}");
    ck_assert(is_ok(&res3));

    // Verify all messages were inserted in order
    const char *query = "SELECT kind FROM messages WHERE session_id = $1 ORDER BY created_at";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQntuples(result), 3);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "clear");
    ck_assert_str_eq(PQgetvalue(result, 1, 0), "system");
    ck_assert_str_eq(PQgetvalue(result, 2, 0), "user");

    PQclear(result);
}

END_TEST
// Test: Insert command event for slash command persistence
START_TEST(test_db_message_insert_command_event)
{
    SKIP_IF_NO_DB();

    const char *command_content = "/help";
    const char *data_json = "{\"command\":\"/help\",\"output\":\"Available commands...\"}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "command", command_content, data_json);
    ck_assert(is_ok(&res));

    // Verify message was inserted
    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1 AND kind = 'command'";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "command");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), command_content);

    // Verify JSONB contains command details
    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "/help") != NULL);

    PQclear(result);
}

END_TEST
// Test: Insert fork event for recording fork operations
START_TEST(test_db_message_insert_fork_event)
{
    SKIP_IF_NO_DB();

    const char *fork_content = "Forked to agent_uuid_123";
    const char *data_json = "{\"parent_uuid\":\"uuid_parent\",\"child_uuid\":\"uuid_child\"}";

    res_t res = ik_db_message_insert(db, session_id, NULL, "fork", fork_content, data_json);
    ck_assert(is_ok(&res));

    // Verify message was inserted
    const char *query = "SELECT kind, content, data::text FROM messages WHERE session_id = $1 AND kind = 'fork'";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "fork");
    ck_assert_str_eq(PQgetvalue(result, 0, 1), fork_content);

    // Verify JSONB contains fork details
    const char *json_result = PQgetvalue(result, 0, 2);
    ck_assert(strstr(json_result, "parent_uuid") != NULL);
    ck_assert(strstr(json_result, "child_uuid") != NULL);

    PQclear(result);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *message_suite(void)
{
    Suite *s = suite_create("Message");

    TCase *tc_insert = tcase_create("Insert");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_insert, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_insert, test_setup, test_teardown);

    tcase_add_test(tc_insert, test_db_message_insert_clear_event);
    tcase_add_test(tc_insert, test_db_message_insert_system_event);
    tcase_add_test(tc_insert, test_db_message_insert_user_event);
    tcase_add_test(tc_insert, test_db_message_insert_assistant_event);
    tcase_add_test(tc_insert, test_db_message_insert_mark_event);
    tcase_add_test(tc_insert, test_db_message_insert_rewind_event);
    tcase_add_test(tc_insert, test_db_message_insert_tool_call_event);
    tcase_add_test(tc_insert, test_db_message_insert_tool_result_event);
    tcase_add_test(tc_insert, test_db_message_insert_empty_content);
    tcase_add_test(tc_insert, test_db_message_insert_null_data);
    tcase_add_test(tc_insert, test_db_message_insert_multiple_events);
    tcase_add_test(tc_insert, test_db_message_insert_command_event);
    tcase_add_test(tc_insert, test_db_message_insert_fork_event);
    suite_add_tcase(s, tc_insert);

    TCase *tc_errors = tcase_create("Errors");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_errors, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_errors, test_setup, test_teardown);

    tcase_add_test(tc_errors, test_db_message_insert_fk_constraint);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
