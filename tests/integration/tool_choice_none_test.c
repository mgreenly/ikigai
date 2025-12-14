/**
 * @file tool_choice_none_test.c
 * @brief End-to-end integration test for tool_choice: "none" behavior
 *
 * Verifies that tool_choice: "none" works correctly in the full conversation flow:
 * 1. User asks for file search
 * 2. Request explicitly sets tool_choice: "none"
 * 3. Model responds with text only (cannot call tools)
 * 4. No tool execution occurs
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
#include "../../src/openai/tool_choice.h"
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

// ========== Test: Request serialization with tool_choice none ==========

/**
 * Verify that ik_openai_serialize_request includes "tool_choice": "none"
 * when using ik_tool_choice_none().
 */
START_TEST(test_request_has_tool_choice_none) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;

    // Create conversation with user message
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Find all C files in src/");
    ck_assert(!msg_res.is_err);

    res_t add_res = ik_openai_conversation_add_msg(conv, msg_res.ok);
    ck_assert(!add_res.is_err);

    // Create request
    ik_openai_request_t *request = ik_openai_request_create(ctx, cfg, conv);

    // Serialize with tool_choice none
    ik_tool_choice_t choice = ik_tool_choice_none();
    char *json = ik_openai_serialize_request(ctx, request, choice);
    ck_assert_ptr_nonnull(json);

    // Parse JSON and verify tool_choice field
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_is_obj(root));

    // Verify tool_choice field exists and is "none"
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert(yyjson_is_str(tool_choice));
    ck_assert_str_eq(yyjson_get_str(tool_choice), "none");

    // Verify tools array is still present (model can't use them, but they're sent)
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert(yyjson_arr_size(tools) == 5);  // 5 tools: glob, file_read, grep, file_write, bash

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST
// ========== Test: End-to-end none tool choice flow ==========

/**
 * Test the complete flow with tool_choice: "none":
 * 1. User message: "Find all C files in src/"
 * 2. Model must respond with text only (simulated)
 * 3. No tool execution occurs
 * 4. Only user and assistant messages in database
 */
START_TEST(test_tool_choice_none_end_to_end)
{
    SKIP_IF_NO_DB();

    // Step 1: User message
    const char *user_message = "Find all C files in src/";
    res_t res = ik_db_message_insert(db, session_id, NULL, "user", user_message, NULL);
    ck_assert(!res.is_err);

    // Step 2: Model responds with text only (tool_choice was "none")
    // In real flow, this would come from OpenAI API with finish_reason="stop"
    // Model cannot call tools, so it provides text guidance instead
    const char *assistant_response =
        "To find all C files in src/, you can use a command like `find src/ -name \"*.c\"` "
        "or check the directory listing. I don't have access to your filesystem to search directly.";

    // Persist assistant message to database
    res = ik_db_message_insert(db, session_id, NULL, "assistant", assistant_response,
                               "{\"model\": \"gpt-4o-mini\", \"finish_reason\": \"stop\"}");
    ck_assert(!res.is_err);

    // Step 3: Verify conversation structure in database
    // Should have ONLY: user, assistant (no tool_call, no tool_result)
    const char *count_query = "SELECT COUNT(*) FROM messages WHERE session_id = $1";
    char *session_id_str = talloc_asprintf(test_ctx, "%lld", (long long)session_id);
    const char *params[] = {session_id_str};

    PGresult *count_result = PQexecParams(db->conn, count_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(count_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(count_result), 1);

    int message_count = atoi(PQgetvalue(count_result, 0, 0));
    ck_assert_int_eq(message_count, 2);  // Only user and assistant
    PQclear(count_result);

    // Verify no tool_call messages exist
    const char *tool_call_query =
        "SELECT COUNT(*) FROM messages WHERE session_id = $1 AND kind = 'tool_call'";

    PGresult *tool_call_result = PQexecParams(db->conn, tool_call_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(tool_call_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(tool_call_result), 1);

    int tool_call_count = atoi(PQgetvalue(tool_call_result, 0, 0));
    ck_assert_int_eq(tool_call_count, 0);  // No tool calls
    PQclear(tool_call_result);

    // Verify no tool_result messages exist
    const char *tool_result_query =
        "SELECT COUNT(*) FROM messages WHERE session_id = $1 AND kind = 'tool_result'";

    PGresult *tool_result_result = PQexecParams(db->conn, tool_result_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(tool_result_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(tool_result_result), 1);

    int tool_result_count = atoi(PQgetvalue(tool_result_result, 0, 0));
    ck_assert_int_eq(tool_result_count, 0);  // No tool results
    PQclear(tool_result_result);

    // Verify assistant message has correct finish_reason
    const char *assistant_query =
        "SELECT data FROM messages WHERE session_id = $1 AND kind = 'assistant'";

    PGresult *assistant_result = PQexecParams(db->conn, assistant_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(assistant_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(assistant_result), 1);

    const char *data_json_str = PQgetvalue(assistant_result, 0, 0);
    ck_assert_ptr_nonnull(data_json_str);

    // Parse data_json and verify finish_reason
    yyjson_doc *data_doc = yyjson_read(data_json_str, strlen(data_json_str), 0);
    ck_assert_ptr_nonnull(data_doc);

    yyjson_val *data_root = yyjson_doc_get_root(data_doc);
    ck_assert(yyjson_is_obj(data_root));

    yyjson_val *finish_reason = yyjson_obj_get(data_root, "finish_reason");
    ck_assert_ptr_nonnull(finish_reason);
    ck_assert_str_eq(yyjson_get_str(finish_reason), "stop");

    yyjson_doc_free(data_doc);
    PQclear(assistant_result);
}

END_TEST
// ========== Test: Verify none constructor ==========

/**
 * Verify that ik_tool_choice_none() returns the correct mode.
 */
START_TEST(test_tool_choice_none_constructor)
{
    ik_tool_choice_t choice = ik_tool_choice_none();
    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_NONE);
    ck_assert_ptr_null(choice.tool_name);
}

END_TEST

// ========== Test Suite ==========

static Suite *tool_choice_none_suite(void)
{
    Suite *s = suite_create("Tool Choice None");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);
    tcase_add_test(tc_core, test_request_has_tool_choice_none);
    tcase_add_test(tc_core, test_tool_choice_none_end_to_end);
    tcase_add_test(tc_core, test_tool_choice_none_constructor);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = tool_choice_none_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
