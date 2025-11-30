/**
 * @file file_read_error_test.c
 * @brief End-to-end integration test for file not found error handling
 *
 * This test verifies that file not found errors flow correctly through the entire system:
 * 1. User requests a non-existent file
 * 2. Model responds with file_read tool call
 * 3. Tool execution returns error: {"error": "File not found: missing.txt"}
 * 4. Error result is added to conversation as tool message
 * 5. Follow-up request is sent to model with error in tool message
 * 6. Model responds with helpful error explanation
 * 7. All messages persist to database correctly
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "../../src/config.h"
#include "../../src/db/connection.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/error.h"
#include "../../src/msg.h"
#include "../../src/openai/client.h"
#include "../../src/tool.h"
#include "../test_utils.h"

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

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Test: End-to-end file read error flow ==========

/**
 * Test the complete flow of file not found error handling.
 * This test simulates what happens when a user asks to read a non-existent file.
 */
START_TEST(test_file_read_error_end_to_end) {
    SKIP_IF_NO_DB();

    // Step 1: User message asking to read a non-existent file
    const char *user_message = "Show me missing.txt";
    res_t res = ik_db_message_insert(db, session_id, "user", user_message, NULL);
    ck_assert(!res.is_err);

    // Step 2: Model responds with tool call (simulated by creating the message directly)
    // In real flow, this would come from OpenAI API response with finish_reason="tool_calls"
    const char *tool_call_id = "call_test123";
    const char *tool_name = "file_read";
    const char *tool_arguments = "{\"path\": \"missing.txt\"}";

    // Build tool_call message data_json
    char *tool_call_data = talloc_asprintf(test_ctx,
                                           "{\"id\": \"%s\", \"type\": \"function\", \"function\": {\"name\": \"%s\", \"arguments\": %s}}",
                                           tool_call_id,
                                           tool_name,
                                           tool_arguments);
    ck_assert_ptr_nonnull(tool_call_data);

    // Persist tool_call message to database
    res = ik_db_message_insert(db, session_id, "tool_call", NULL, tool_call_data);
    ck_assert(!res.is_err);

    // Step 3: Execute tool and get error result
    res = ik_tool_dispatch(test_ctx, tool_name, tool_arguments);
    ck_assert(!res.is_err);

    char *tool_result_json = res.ok;
    ck_assert_ptr_nonnull(tool_result_json);

    // Verify the tool result contains error
    yyjson_doc *result_doc = yyjson_read(tool_result_json, strlen(tool_result_json), 0);
    ck_assert_ptr_nonnull(result_doc);

    yyjson_val *result_root = yyjson_doc_get_root(result_doc);
    ck_assert(yyjson_is_obj(result_root));

    // Verify success: false
    yyjson_val *success = yyjson_obj_get(result_root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    // Verify error message mentions the file
    yyjson_val *error = yyjson_obj_get(result_root, "error");
    ck_assert_ptr_nonnull(error);
    const char *error_str = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_str);
    ck_assert(strstr(error_str, "File not found") != NULL);
    ck_assert(strstr(error_str, "missing.txt") != NULL);

    // Step 4: Create tool_result message
    ik_message_t *tool_result_msg = ik_msg_create_tool_result(
        test_ctx,
        tool_call_id,
        tool_name,
        tool_result_json,
        false,  // success = false
        "File not found: missing.txt"
        );
    ck_assert_ptr_nonnull(tool_result_msg);

    // Step 5: Persist tool_result message to database
    res = ik_db_message_insert(db, session_id, "tool_result",
                               tool_result_msg->content,
                               tool_result_msg->data_json);
    ck_assert(!res.is_err);

    // Step 6: Verify conversation structure in database
    // We should have: user message, tool_call message, tool_result message
    const char *count_query = "SELECT COUNT(*) FROM messages WHERE session_id = $1";
    char *session_id_str = talloc_asprintf(test_ctx, "%lld", (long long)session_id);
    const char *params[] = {session_id_str};

    PGresult *count_result = PQexecParams(db->conn, count_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(count_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(count_result), 1);

    int message_count = atoi(PQgetvalue(count_result, 0, 0));
    ck_assert_int_eq(message_count, 3);  // user, tool_call, tool_result
    PQclear(count_result);

    // Verify tool_result message persisted correctly
    const char *tool_result_query =
        "SELECT kind, content, data FROM messages WHERE session_id = $1 AND kind = 'tool_result'";

    PGresult *tool_result_query_result = PQexecParams(db->conn, tool_result_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(tool_result_query_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(tool_result_query_result), 1);

    const char *kind = PQgetvalue(tool_result_query_result, 0, 0);
    ck_assert_str_eq(kind, "tool_result");

    const char *content = PQgetvalue(tool_result_query_result, 0, 1);
    ck_assert_str_eq(content, "File not found: missing.txt");

    const char *data = PQgetvalue(tool_result_query_result, 0, 2);
    ck_assert_ptr_nonnull(data);

    // Parse data_json and verify structure
    yyjson_doc *data_doc = yyjson_read(data, strlen(data), 0);
    ck_assert_ptr_nonnull(data_doc);

    yyjson_val *data_root = yyjson_doc_get_root(data_doc);
    ck_assert(yyjson_is_obj(data_root));

    // Verify tool_call_id
    yyjson_val *stored_tool_call_id = yyjson_obj_get(data_root, "tool_call_id");
    ck_assert_ptr_nonnull(stored_tool_call_id);
    ck_assert_str_eq(yyjson_get_str(stored_tool_call_id), tool_call_id);

    // Verify name
    yyjson_val *stored_name = yyjson_obj_get(data_root, "name");
    ck_assert_ptr_nonnull(stored_name);
    ck_assert_str_eq(yyjson_get_str(stored_name), tool_name);

    // Verify output contains error JSON
    yyjson_val *stored_output = yyjson_obj_get(data_root, "output");
    ck_assert_ptr_nonnull(stored_output);
    const char *stored_output_str = yyjson_get_str(stored_output);
    ck_assert(strstr(stored_output_str, "File not found") != NULL);

    // Verify success field is false
    yyjson_val *stored_success = yyjson_obj_get(data_root, "success");
    ck_assert_ptr_nonnull(stored_success);
    ck_assert(yyjson_get_bool(stored_success) == false);

    yyjson_doc_free(data_doc);
    PQclear(tool_result_query_result);
    yyjson_doc_free(result_doc);

    // Step 7: Simulate model's follow-up response explaining the error
    // In real flow, this would be sent to OpenAI with the tool_result in the conversation,
    // and OpenAI would respond with a helpful message.
    const char *assistant_followup =
        "I couldn't find that file. `missing.txt` doesn't exist in the current directory. "
        "Would you like me to search for it elsewhere?";

    res = ik_db_message_insert(db, session_id, "assistant", assistant_followup,
                               "{\"model\": \"gpt-4o-mini\", \"finish_reason\": \"stop\"}");
    ck_assert(!res.is_err);

    // Final verification: We should now have 4 messages in the correct sequence
    count_result = PQexecParams(db->conn, count_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(count_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(count_result), 1);

    message_count = atoi(PQgetvalue(count_result, 0, 0));
    ck_assert_int_eq(message_count, 4);  // user, tool_call, tool_result, assistant
    PQclear(count_result);
}
END_TEST
/**
 * Test that tool execution correctly handles file not found without crashing.
 * This is a simpler unit-style test within the integration suite.
 */
START_TEST(test_tool_exec_file_read_handles_missing_file)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Execute tool on non-existent file
    const char *nonexistent_path = "/tmp/ikigai-test-missing-file-xyz123.txt";
    res_t res = ik_tool_exec_file_read(ctx, nonexistent_path);

    // Should return successfully (not an error), with error JSON in the result
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse and verify error structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "File not found") != NULL);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}

END_TEST

// ========== Test Suite ==========

static Suite *file_read_error_suite(void)
{
    Suite *s = suite_create("File Read Error Integration");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);
    tcase_add_test(tc_core, test_file_read_error_end_to_end);
    tcase_add_test(tc_core, test_tool_exec_file_read_handles_missing_file);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = file_read_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
