/**
 * @file replay_tool_e2e_test.c
 * @brief End-to-end integration test for session replay with tool messages
 *
 * Verifies the complete flow of Story 12: Session Replay With Tools
 * - Persists a conversation with tool calls to database
 * - Simulates application restart (new replay context)
 * - Verifies session restoration with correct message order
 * - Validates API request serialization matches OpenAI format
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/openai/tool_choice.h"
#include "../../../src/config.h"
#include "../../test_utils.h"
#include "../../../src/vendor/yyjson/yyjson.h"
#include <check.h>
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

/**
 * Test: Complete tool conversation E2E flow
 *
 * This test simulates the complete Story 12 walkthrough:
 * 1. Persist a conversation with tool calls
 * 2. Simulate app restart (new replay context)
 * 3. Load session and verify message order
 * 4. Verify API serialization produces correct OpenAI format
 */
START_TEST(test_tool_conversation_e2e)
{
    SKIP_IF_NO_DB();

    // ========== Phase 1: Persist tool conversation ==========

    // Event 1: User message - "Show me config.json"
    res_t res = ik_db_message_insert(db, session_id, NULL, "user",
                                      "Show me config.json", "{}");
    ck_assert(is_ok(&res));

    // Event 2: Tool call - glob with pattern "*.c"
    // Using the canonical format with data_json containing tool call structure
    const char *tool_call_data = "{"
        "\"id\":\"call_xyz\","
        "\"type\":\"function\","
        "\"function\":{"
            "\"name\":\"file_read\","
            "\"arguments\":\"{\\\"path\\\":\\\"config.json\\\"}\""
        "}"
    "}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_call",
                               "file_read(path=\"config.json\")",
                               tool_call_data);
    ck_assert(is_ok(&res));

    // Event 3: Tool result - file contents
    const char *tool_result_data = "{"
        "\"tool_call_id\":\"call_xyz\","
        "\"name\":\"file_read\","
        "\"output\":\"{\\\"success\\\":true,\\\"data\\\":{\\\"output\\\":\\\"{\\\\\\\"debug\\\\\\\":true,\\\\\\\"port\\\\\\\":8080}\\\"}}\","
        "\"success\":true"
    "}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_result",
                               "File read successfully",
                               tool_result_data);
    ck_assert(is_ok(&res));

    // Event 4: Assistant message - summary response
    res = ik_db_message_insert(db, session_id, NULL, "assistant",
                               "Here's config.json with your debug and port settings.",
                               "{}");
    ck_assert(is_ok(&res));

    // ========== Phase 2: Simulate app restart - new replay context ==========

    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert(context != NULL);

    // ========== Phase 3: Verify message order and structure ==========

    ck_assert_int_eq((int)context->count, 4);

    // Message 0: User message
    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Show me config.json");

    // Message 1: Tool call
    ck_assert_str_eq(context->messages[1]->kind, "tool_call");
    ck_assert_str_eq(context->messages[1]->content, "file_read(path=\"config.json\")");
    ck_assert(context->messages[1]->data_json != NULL);

    // Verify tool call data structure
    yyjson_doc *tc_doc = yyjson_read(context->messages[1]->data_json,
                                      strlen(context->messages[1]->data_json), 0);
    ck_assert(tc_doc != NULL);
    yyjson_val *tc_root = yyjson_doc_get_root(tc_doc);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc_root, "id")), "call_xyz");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc_root, "type")), "function");
    yyjson_val *tc_func = yyjson_obj_get(tc_root, "function");
    ck_assert(tc_func != NULL);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc_func, "name")), "file_read");
    yyjson_doc_free(tc_doc);

    // Message 2: Tool result
    ck_assert_str_eq(context->messages[2]->kind, "tool_result");
    ck_assert_str_eq(context->messages[2]->content, "File read successfully");
    ck_assert(context->messages[2]->data_json != NULL);

    // Verify tool result data structure
    yyjson_doc *tr_doc = yyjson_read(context->messages[2]->data_json,
                                      strlen(context->messages[2]->data_json), 0);
    ck_assert(tr_doc != NULL);
    yyjson_val *tr_root = yyjson_doc_get_root(tr_doc);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tr_root, "tool_call_id")), "call_xyz");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tr_root, "name")), "file_read");
    ck_assert(yyjson_get_bool(yyjson_obj_get(tr_root, "success")));
    yyjson_doc_free(tr_doc);

    // Message 3: Assistant message
    ck_assert_str_eq(context->messages[3]->kind, "assistant");
    ck_assert_str_eq(context->messages[3]->content,
                     "Here's config.json with your debug and port settings.");

    // ========== Phase 4: Verify API serialization format ==========

    // Build conversation for API request using replayed messages
    ik_openai_conversation_t *conv = ik_openai_conversation_create(replay_ctx);

    // Add user message
    ik_msg_t *msg_tmp = ik_openai_msg_create(replay_ctx, "user",
                                          context->messages[0]->content);
    res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(is_ok(&res));

    // Add tool_call message (canonical format)
    ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(
        replay_ctx,
        "call_xyz",
        "function",
        "file_read",
        "{\"path\":\"config.json\"}",
        "file_read(path=\"config.json\")"
    );
    res = ik_openai_conversation_add_msg(conv, tool_call_msg);
    ck_assert(is_ok(&res));

    // Add tool result message (role="tool" for OpenAI API)
    // Note: OpenAI expects role="tool" with tool_call_id and content
    ik_msg_t *tool_result_msg = ik_openai_msg_create(replay_ctx, "tool",
        "{\"success\":true,\"data\":{\"output\":\"{\\\"debug\\\":true,\\\"port\\\":8080}\"}}");
    // Set data_json to include tool_call_id for serialization
    tool_result_msg->data_json = talloc_strdup(tool_result_msg,
        "{\"tool_call_id\":\"call_xyz\"}");
    res = ik_openai_conversation_add_msg(conv, tool_result_msg);
    ck_assert(is_ok(&res));

    // Add assistant message
    msg_tmp = ik_openai_msg_create(replay_ctx, "assistant",
                                    context->messages[3]->content);
    res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(is_ok(&res));

    // Create config for request serialization
    ik_config_t *cfg = talloc_zero(replay_ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 2000;

    // Create request and serialize to JSON
    ik_openai_request_t *request = ik_openai_request_create(replay_ctx, cfg, conv);
    ik_tool_choice_t choice = ik_tool_choice_auto();
    char *json_str = ik_openai_serialize_request(replay_ctx, request, choice);
    ck_assert(json_str != NULL);

    // Parse serialized JSON and verify structure
    yyjson_doc *json_doc = yyjson_read(json_str, strlen(json_str), 0);
    ck_assert(json_doc != NULL);
    yyjson_val *root = yyjson_doc_get_root(json_doc);

    // Verify model
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(root, "model")), "gpt-4o-mini");

    // Verify messages array
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert(yyjson_is_arr(messages));
    ck_assert_int_eq((int)yyjson_arr_size(messages), 4);

    // Verify message 0: user message
    yyjson_val *msg0 = yyjson_arr_get(messages, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg0, "role")), "user");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg0, "content")), "Show me config.json");

    // Verify message 1: tool_call transformed to role="assistant" with tool_calls array
    yyjson_val *msg1 = yyjson_arr_get(messages, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg1, "role")), "assistant");
    yyjson_val *tool_calls = yyjson_obj_get(msg1, "tool_calls");
    ck_assert(yyjson_is_arr(tool_calls));
    ck_assert_int_eq((int)yyjson_arr_size(tool_calls), 1);

    yyjson_val *tc = yyjson_arr_get(tool_calls, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc, "id")), "call_xyz");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc, "type")), "function");
    yyjson_val *func = yyjson_obj_get(tc, "function");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(func, "name")), "file_read");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(func, "arguments")),
                     "{\"path\":\"config.json\"}");

    // Verify message 2: tool result (role="tool")
    yyjson_val *msg2 = yyjson_arr_get(messages, 2);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg2, "role")), "tool");

    // Verify message 3: assistant message
    yyjson_val *msg3 = yyjson_arr_get(messages, 3);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg3, "role")), "assistant");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg3, "content")),
                     "Here's config.json with your debug and port settings.");

    yyjson_doc_free(json_doc);
    talloc_free(replay_ctx);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *replay_tool_e2e_suite(void)
{
    Suite *s = suite_create("Replay Tool E2E");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_tool_conversation_e2e);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_tool_e2e_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
