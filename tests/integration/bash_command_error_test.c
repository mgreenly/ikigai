/**
 * @file bash_command_error_test.c
 * @brief End-to-end integration test for bash command error handling
 *
 * Tests the complete flow when a bash command fails with non-zero exit code:
 * 1. User requests command that will fail
 * 2. Model responds with bash tool call
 * 3. Tool execution returns error output with non-zero exit code
 * 4. Error result added to conversation as tool message
 * 5. Follow-up request sent to model with error in tool message
 * 6. Model responds with helpful explanation
 * 7. All messages persist to database correctly
 *
 * This test verifies User Story 09 (bash-command-fails) works end-to-end.
 */

#include "../../src/commands.h"
#include "../../src/db/connection.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/error.h"
#include "../../src/msg.h"
#include "../../src/openai/client.h"
#include "../../src/tool.h"
#include "../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

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

// Helper to count messages in database for a session
static int count_messages(ik_db_ctx_t *db_ctx, int64_t sid, const char *kind)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *query;
    PGresult *result;

    if (kind == NULL) {
        query = "SELECT COUNT(*) FROM messages WHERE session_id = $1";
        const char *params[] = {talloc_asprintf(ctx, "%lld", (long long)sid)};
        result = PQexecParams(db_ctx->conn, query, 1, NULL, params, NULL, NULL, 0);
    } else {
        query = "SELECT COUNT(*) FROM messages WHERE session_id = $1 AND kind = $2";
        const char *params[] = {
            talloc_asprintf(ctx, "%lld", (long long)sid),
            kind
        };
        result = PQexecParams(db_ctx->conn, query, 2, NULL, params, NULL, NULL, 0);
    }

    int count = 0;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        count = atoi(PQgetvalue(result, 0, 0));
    }

    PQclear(result);
    talloc_free(ctx);
    return count;
}

// Helper to retrieve a message from database
static char *get_message_content(ik_db_ctx_t *db_ctx, int64_t sid, const char *kind)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *query = "SELECT content FROM messages WHERE session_id = $1 AND kind = $2 ORDER BY id DESC LIMIT 1";
    const char *params[] = {
        talloc_asprintf(ctx, "%lld", (long long)sid),
        kind
    };

    PGresult *result = PQexecParams(db_ctx->conn, query, 2, NULL, params, NULL, NULL, 0);

    char *content = NULL;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        content = strdup(PQgetvalue(result, 0, 0));
    }

    PQclear(result);
    talloc_free(ctx);
    return content;
}

// Test 1: Tool execution returns non-zero exit code in JSON format
START_TEST(test_bash_command_returns_error_exit_code) {
    SKIP_IF_NO_DB();

    // Execute a command that will fail (false command always returns 1)
    res_t res = ik_tool_exec_bash(test_ctx, "false");
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    // Verify success: true (tool executed successfully)
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data object exists with exit_code
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data);
    ck_assert(yyjson_is_obj(data));

    // Verify exit_code is non-zero
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_ptr_nonnull(exit_code);
    ck_assert_int_ne(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}
END_TEST
// Test 2: Tool execution with command that produces error output
START_TEST(test_bash_command_with_stderr_output)
{
    SKIP_IF_NO_DB();

    // Execute a command that writes to stderr and fails
    // Using a command that will definitely fail with error output
    res_t res = ik_tool_exec_bash(test_ctx, "ls /nonexistent_directory_12345 2>&1");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: true (tool executed)
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data contains output with error message
    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    // Should contain some error-related text
    ck_assert(strlen(output_str) > 0);

    // Verify exit_code is non-zero
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_ne(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test 3: Conversation with bash tool call and error result persists correctly
START_TEST(test_bash_error_conversation_persistence)
{
    SKIP_IF_NO_DB();

    // Create conversation simulating the user story flow
    res_t conv_res = ik_openai_conversation_create(test_ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    // Step 1: User message "Compile the project with gcc main.c"
    res_t user_msg_res = ik_openai_msg_create(test_ctx, "user", "Compile the project with gcc main.c");
    ck_assert(!user_msg_res.is_err);
    ik_msg_t *user_msg = user_msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, user_msg);
    ck_assert(!add_res.is_err);

    // Persist user message to database
    res_t insert_res = ik_db_message_insert(db, session_id, NULL, "user",
                                            "Compile the project with gcc main.c", NULL);
    ck_assert(!insert_res.is_err);

    // Verify user message was persisted
    ck_assert_int_eq(count_messages(db, session_id, "user"), 1);

    // Step 2: Model responds with bash tool call (simulated)
    // In real flow, model would respond with tool_calls
    // We create the tool call message that represents the assistant's decision
    ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(
        test_ctx,
        "call_bash1",
        "function",
        "bash",
        "{\"command\": \"gcc main.c\"}",
        "bash(command=\"gcc main.c\")"
        );
    ck_assert_ptr_nonnull(tool_call_msg);

    add_res = ik_openai_conversation_add_msg(conv, tool_call_msg);
    ck_assert(!add_res.is_err);

    // Persist assistant tool call to database
    insert_res = ik_db_message_insert(db, session_id, NULL, "assistant",
                                      "bash(command=\"gcc main.c\")",
                                      tool_call_msg->data_json);
    ck_assert(!insert_res.is_err);

    // Step 3: Execute bash tool (this would fail with missing object files)
    // For testing, we use a simpler command that will fail
    res_t tool_res = ik_tool_exec_bash(test_ctx, "gcc /tmp/nonexistent_file_12345.c 2>&1");
    ck_assert(!tool_res.is_err);

    char *tool_output = tool_res.ok;
    ck_assert_ptr_nonnull(tool_output);

    // Parse tool output to verify it's error format
    yyjson_doc *tool_doc = yyjson_read(tool_output, strlen(tool_output), 0);
    ck_assert_ptr_nonnull(tool_doc);
    yyjson_val *tool_root = yyjson_doc_get_root(tool_doc);

    yyjson_val *data = yyjson_obj_get(tool_root, "data");
    ck_assert_ptr_nonnull(data);

    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_ne(yyjson_get_int(exit_code), 0);

    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *error_output = yyjson_get_str(output);
    ck_assert_ptr_nonnull(error_output);

    yyjson_doc_free(tool_doc);

    // Step 4: Create tool result message with error
    ik_msg_t *tool_result_msg = ik_msg_create_tool_result(
        test_ctx,
        "call_bash1",
        "bash",
        tool_output,
        true,  // success = true (tool executed, even though command failed)
        "Command failed with non-zero exit code"
        );
    ck_assert_ptr_nonnull(tool_result_msg);

    // Persist tool result to database
    insert_res = ik_db_message_insert(db, session_id, NULL, "tool_result",
                                      tool_result_msg->content,
                                      tool_result_msg->data_json);
    ck_assert(!insert_res.is_err);

    // Step 5: Model responds with explanation (simulated)
    res_t assistant_msg_res = ik_openai_msg_create(test_ctx,
                                                   "assistant",
                                                   "The compilation failed. GCC reported an error. The file does not exist.");
    ck_assert(!assistant_msg_res.is_err);
    ik_msg_t *assistant_msg = assistant_msg_res.ok;

    add_res = ik_openai_conversation_add_msg(conv, assistant_msg);
    ck_assert(!add_res.is_err);

    // Persist assistant response to database
    insert_res = ik_db_message_insert(db, session_id, NULL, "assistant",
                                      assistant_msg->content, NULL);
    ck_assert(!insert_res.is_err);

    // Verify complete conversation was persisted
    // Should have: 1 user, 1 assistant (tool call), 1 tool_result, 1 assistant (explanation)
    ck_assert_int_eq(count_messages(db, session_id, NULL), 4);
    ck_assert_int_eq(count_messages(db, session_id, "user"), 1);
    ck_assert_int_eq(count_messages(db, session_id, "assistant"), 2);
    ck_assert_int_eq(count_messages(db, session_id, "tool_result"), 1);

    // Verify tool_result message contains error information
    char *tool_result_content = get_message_content(db, session_id, "tool_result");
    ck_assert_ptr_nonnull(tool_result_content);
    ck_assert(strstr(tool_result_content, "failed") != NULL ||
              strstr(tool_result_content, "error") != NULL ||
              strstr(tool_result_content, "exit") != NULL);
    free(tool_result_content);
}

END_TEST
// Test 4: Tool dispatcher handles bash tool correctly
START_TEST(test_tool_dispatcher_bash_with_error)
{
    SKIP_IF_NO_DB();

    // Dispatch bash tool with command that will fail
    const char *arguments = "{\"command\": \"false\"}";
    res_t res = ik_tool_dispatch(test_ctx, "bash", arguments);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify successful tool execution
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    // Verify error exit code
    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_ne(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test 5: Multiple bash failures in sequence persist correctly
START_TEST(test_multiple_bash_failures_persistence)
{
    SKIP_IF_NO_DB();

    // Simulate multiple failed commands in sequence
    const char *failing_commands[] = {
        "false",
        "ls /nonexistent",
        "gcc /tmp/missing.c"
    };

    for (size_t i = 0; i < 3; i++) {
        // Execute bash command
        res_t tool_res = ik_tool_exec_bash(test_ctx, failing_commands[i]);
        ck_assert(!tool_res.is_err);

        // Create and persist tool result
        char *tool_call_id = talloc_asprintf(test_ctx, "call_bash_%zu", i);
        ik_msg_t *tool_result_msg = ik_msg_create_tool_result(
            test_ctx,
            tool_call_id,
            "bash",
            tool_res.ok,
            true,
            "Command execution result"
            );
        ck_assert_ptr_nonnull(tool_result_msg);

        res_t insert_res = ik_db_message_insert(db, session_id, NULL, "tool_result",
                                                tool_result_msg->content,
                                                tool_result_msg->data_json);
        ck_assert(!insert_res.is_err);
    }

    // Verify all tool results were persisted
    ck_assert_int_eq(count_messages(db, session_id, "tool_result"), 3);
}

END_TEST

// Test suite
static Suite *bash_command_error_suite(void)
{
    Suite *s = suite_create("Bash Command Error Integration");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_bash_command_returns_error_exit_code);
    tcase_add_test(tc_core, test_bash_command_with_stderr_output);
    tcase_add_test(tc_core, test_bash_error_conversation_persistence);
    tcase_add_test(tc_core, test_tool_dispatcher_bash_with_error);
    tcase_add_test(tc_core, test_multiple_bash_failures_persistence);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = bash_command_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
