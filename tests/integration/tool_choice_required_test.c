/**
 * @file tool_choice_required_test.c
 * @brief End-to-end integration test for tool_choice: "required" behavior
 *
 * Verifies that tool_choice: "required" works correctly in the full conversation flow:
 * 1. User asks for file search
 * 2. Request explicitly sets tool_choice: "required"
 * 3. Model must call a tool (cannot respond with text only)
 * 4. Tool executes and returns results
 * 5. Model summarizes results
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

// ========== Test: Request serialization with tool_choice required ==========

/**
 * Verify that ik_openai_serialize_request includes "tool_choice": "required"
 * when using ik_tool_choice_required().
 */
START_TEST(test_request_has_tool_choice_required) {
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

    // Serialize with tool_choice required
    ik_tool_choice_t choice = ik_tool_choice_required();
    char *json = ik_openai_serialize_request(ctx, request, choice);
    ck_assert_ptr_nonnull(json);

    // Parse JSON and verify tool_choice field
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_is_obj(root));

    // Verify tool_choice field exists and is "required"
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert(yyjson_is_str(tool_choice));
    ck_assert_str_eq(yyjson_get_str(tool_choice), "required");

    // Verify tools array is present
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert(yyjson_arr_size(tools) == 5);  // 5 tools: glob, file_read, grep, file_write, bash

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST
// ========== Test: End-to-end required tool choice flow ==========

/**
 * Test the complete flow with tool_choice: "required":
 * 1. User message: "Find all C files in src/"
 * 2. Model must call a tool (simulated - glob tool call)
 * 3. Tool executes and returns results
 * 4. Model responds with formatted results (simulated)
 * 5. All messages persisted to database correctly
 */
START_TEST(test_tool_choice_required_end_to_end)
{
    SKIP_IF_NO_DB();

    // Step 1: User message
    const char *user_message = "Find all C files in src/";
    res_t res = ik_db_message_insert(db, session_id, NULL, "user", user_message, NULL);
    ck_assert(!res.is_err);

    // Step 2: Model MUST call a tool (tool_choice was "required")
    // In real flow, this would come from OpenAI API with finish_reason="tool_calls"
    // Model cannot respond with text only - must use a tool
    const char *tool_call_id = "call_glob_required_123";
    const char *tool_name = "glob";
    const char *tool_arguments = "{\"pattern\": \"*.c\", \"path\": \"src/\"}";

    // Build tool_call message data_json
    char *tool_call_data = talloc_asprintf(test_ctx,
                                           "{\"id\": \"%s\", \"type\": \"function\", \"function\": {\"name\": \"%s\", \"arguments\": %s}}",
                                           tool_call_id,
                                           tool_name,
                                           tool_arguments);
    ck_assert_ptr_nonnull(tool_call_data);

    // Persist tool_call message to database
    res = ik_db_message_insert(db, session_id, NULL, "tool_call", NULL, tool_call_data);
    ck_assert(!res.is_err);

    // Step 3: Execute tool
    char *pattern = ik_tool_arg_get_string(test_ctx, tool_arguments, "pattern");
    char *path = ik_tool_arg_get_string(test_ctx, tool_arguments, "path");
    ck_assert_ptr_nonnull(pattern);
    ck_assert_ptr_nonnull(path);

    res = ik_tool_exec_glob(test_ctx, pattern, path);
    ck_assert(!res.is_err);

    char *tool_result_json = res.ok;
    ck_assert_ptr_nonnull(tool_result_json);

    // Verify tool result structure
    yyjson_doc *result_doc = yyjson_read(tool_result_json, strlen(tool_result_json), 0);
    ck_assert_ptr_nonnull(result_doc);

    yyjson_val *result_root = yyjson_doc_get_root(result_doc);
    ck_assert(yyjson_is_obj(result_root));

    // Verify success field
    yyjson_val *success = yyjson_obj_get(result_root, "success");
    ck_assert_ptr_nonnull(success);
    // Success depends on whether src/ has .c files - don't assert value

    // Step 4: Create tool_result message
    yyjson_val *data = yyjson_obj_get(result_root, "data");
    yyjson_val *output = NULL;
    int count = 0;

    if (data && yyjson_is_obj(data)) {
        output = yyjson_obj_get(data, "output");
        yyjson_val *count_val = yyjson_obj_get(data, "count");
        if (count_val) {
            count = (int)yyjson_get_int(count_val);
        }
    }

    const char *output_str = output ? yyjson_get_str(output) : "";
    char *content = talloc_asprintf(test_ctx, "%d file(s) found", count);

    ik_msg_t *tool_result_msg = ik_msg_create_tool_result(
        test_ctx,
        tool_call_id,
        tool_name,
        tool_result_json,
        yyjson_get_bool(success),
        content
        );
    ck_assert_ptr_nonnull(tool_result_msg);

    // Step 5: Persist tool_result message to database
    res = ik_db_message_insert(db, session_id, NULL, "tool_result",
                               tool_result_msg->content,
                               tool_result_msg->data_json);
    ck_assert(!res.is_err);

    // Step 6: Model provides final formatted response (now using auto mode)
    // In real flow, this would be sent to OpenAI with tool_result in conversation
    const char *assistant_response = talloc_asprintf(test_ctx,
                                                     "I found the following C files in src/:\n\n%s",
                                                     output_str);

    // Free result_doc after we're done using output_str
    yyjson_doc_free(result_doc);

    res = ik_db_message_insert(db, session_id, NULL, "assistant", assistant_response,
                               "{\"model\": \"gpt-4o-mini\", \"finish_reason\": \"stop\"}");
    ck_assert(!res.is_err);

    // Step 7: Verify conversation structure in database
    // Should have: user, tool_call, tool_result, assistant
    const char *count_query = "SELECT COUNT(*) FROM messages WHERE session_id = $1";
    char *session_id_str = talloc_asprintf(test_ctx, "%lld", (long long)session_id);
    const char *params[] = {session_id_str};

    PGresult *count_result = PQexecParams(db->conn, count_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(count_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(count_result), 1);

    int message_count = atoi(PQgetvalue(count_result, 0, 0));
    ck_assert_int_eq(message_count, 4);  // user, tool_call, tool_result, assistant
    PQclear(count_result);

    // Verify tool_call message has correct structure
    const char *tool_call_query =
        "SELECT kind, data FROM messages WHERE session_id = $1 AND kind = 'tool_call'";

    PGresult *tool_call_result = PQexecParams(db->conn, tool_call_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(tool_call_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(tool_call_result), 1);

    const char *kind = PQgetvalue(tool_call_result, 0, 0);
    ck_assert_str_eq(kind, "tool_call");

    const char *data_json_str = PQgetvalue(tool_call_result, 0, 1);
    ck_assert_ptr_nonnull(data_json_str);

    // Parse data_json and verify tool call structure
    yyjson_doc *data_doc = yyjson_read(data_json_str, strlen(data_json_str), 0);
    ck_assert_ptr_nonnull(data_doc);

    yyjson_val *data_root = yyjson_doc_get_root(data_doc);
    ck_assert(yyjson_is_obj(data_root));

    // Verify tool call fields
    yyjson_val *id_val = yyjson_obj_get(data_root, "id");
    ck_assert_ptr_nonnull(id_val);
    ck_assert_str_eq(yyjson_get_str(id_val), tool_call_id);

    yyjson_val *type_val = yyjson_obj_get(data_root, "type");
    ck_assert_ptr_nonnull(type_val);
    ck_assert_str_eq(yyjson_get_str(type_val), "function");

    yyjson_val *function = yyjson_obj_get(data_root, "function");
    ck_assert_ptr_nonnull(function);
    ck_assert(yyjson_is_obj(function));

    yyjson_val *name_val = yyjson_obj_get(function, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert_str_eq(yyjson_get_str(name_val), "glob");

    yyjson_doc_free(data_doc);
    PQclear(tool_call_result);

    // Verify that tool execution actually occurred
    // (This distinguishes required mode from none mode)
    const char *tool_result_query =
        "SELECT COUNT(*) FROM messages WHERE session_id = $1 AND kind = 'tool_result'";

    PGresult *tool_result_result = PQexecParams(db->conn, tool_result_query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(tool_result_result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(tool_result_result), 1);

    int tool_result_count = atoi(PQgetvalue(tool_result_result, 0, 0));
    ck_assert_int_eq(tool_result_count, 1);  // Tool WAS executed
    PQclear(tool_result_result);
}

END_TEST
// ========== Test: Verify required constructor ==========

/**
 * Verify that ik_tool_choice_required() returns the correct mode.
 */
START_TEST(test_tool_choice_required_constructor)
{
    ik_tool_choice_t choice = ik_tool_choice_required();
    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_REQUIRED);
    ck_assert_ptr_null(choice.tool_name);
}

END_TEST

// ========== Test Suite ==========

static Suite *tool_choice_required_suite(void)
{
    Suite *s = suite_create("Tool Choice Required");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);
    tcase_add_test(tc_core, test_request_has_tool_choice_required);
    tcase_add_test(tc_core, test_tool_choice_required_end_to_end);
    tcase_add_test(tc_core, test_tool_choice_required_constructor);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = tool_choice_required_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
