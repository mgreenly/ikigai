#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/client.h"
#include "openai/tool_choice.h"
#include "vendor/yyjson/yyjson.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

/*
 * Tool result message creation tests
 */

START_TEST(test_tool_result_message_create) {
    ik_msg_t *msg = ik_openai_msg_create_tool_result(
        ctx,
        "call_abc123",
        "{\"success\": true, \"data\": {\"count\": 3}}"
        );

    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->kind, "tool_result");
    ck_assert_str_eq(msg->content, "{\"success\": true, \"data\": {\"count\": 3}}");
    ck_assert_ptr_nonnull(msg->data_json);
}
END_TEST START_TEST(test_tool_result_message_data_json_structure)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_result(
        ctx,
        "call_xyz789",
        "{\"output\": \"file.c\"}"
        );

    ck_assert_ptr_nonnull(msg->data_json);

    /* Parse data_json and verify structure */
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_is_obj(root));

    /* Check tool_call_id field */
    const char *tool_call_id = yyjson_get_str(yyjson_obj_get(root, "tool_call_id"));
    ck_assert_str_eq(tool_call_id, "call_xyz789");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_tool_result_message_talloc_hierarchy)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_result(
        ctx,
        "call_test",
        "{}"
        );

    /* Message should be child of ctx */
    ck_assert_ptr_eq(talloc_parent(msg), ctx);

    /* Role, content, and data_json should be children of message */
    ck_assert_ptr_eq(talloc_parent(msg->kind), msg);
    ck_assert_ptr_eq(talloc_parent(msg->content), msg);
    ck_assert_ptr_eq(talloc_parent(msg->data_json), msg);
}

END_TEST START_TEST(test_serialize_tool_result_message)
{
    /* Create conversation with tool_result message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create_tool_result(
        ctx,
        "call_123",
        "{\"success\": true, \"count\": 5}"
        );

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Create minimal request for serialization */
    ik_openai_request_t *req = talloc_zero(ctx, ik_openai_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->conv = conv;
    req->temperature = 0.7;
    req->max_completion_tokens = 100;
    req->stream = false;

    /* Serialize request with tool_choice auto */
    ik_tool_choice_t choice = ik_tool_choice_auto();
    char *json = ik_openai_serialize_request(ctx, req, choice);
    ck_assert_ptr_nonnull(json);

    /* Parse serialized JSON */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    /* Check first message */
    yyjson_val *first_msg = yyjson_arr_get(messages, 0);
    ck_assert_ptr_nonnull(first_msg);

    /* Tool result should be serialized as role="tool" with tool_call_id */
    const char *role = yyjson_get_str(yyjson_obj_get(first_msg, "role"));
    ck_assert_str_eq(role, "tool");

    /* Check tool_call_id */
    const char *tool_call_id = yyjson_get_str(yyjson_obj_get(first_msg, "tool_call_id"));
    ck_assert_str_eq(tool_call_id, "call_123");

    /* Check content */
    const char *content = yyjson_get_str(yyjson_obj_get(first_msg, "content"));
    ck_assert_str_eq(content, "{\"success\": true, \"count\": 5}");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_call_and_result_sequence)
{
    /* Create conversation with user, tool_call, tool_result messages */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    /* Add user message */
    res_t user_msg_res = ik_openai_msg_create(ctx, "user", "Find all C files");
    ck_assert(!user_msg_res.is_err);
    res_t add_user = ik_openai_conversation_add_msg(conv, user_msg_res.ok);
    ck_assert(!add_user.is_err);

    /* Add tool_call message */
    ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_456",
        "function",
        "glob",
        "{\"pattern\": \"*.c\"}",
        "glob(pattern=\"*.c\")"
        );
    res_t add_call = ik_openai_conversation_add_msg(conv, tool_call_msg);
    ck_assert(!add_call.is_err);

    /* Add tool_result message */
    ik_msg_t *tool_result_msg = ik_openai_msg_create_tool_result(
        ctx,
        "call_456",
        "{\"output\": \"main.c\\ntest.c\", \"count\": 2}"
        );
    res_t add_result = ik_openai_conversation_add_msg(conv, tool_result_msg);
    ck_assert(!add_result.is_err);

    /* Create and serialize request */
    ik_openai_request_t *req = talloc_zero(ctx, ik_openai_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->conv = conv;
    req->temperature = 0.7;
    req->max_completion_tokens = 100;
    req->stream = false;

    ik_tool_choice_t choice = ik_tool_choice_auto();
    char *json = ik_openai_serialize_request(ctx, req, choice);
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");

    /* Verify we have 3 messages */
    ck_assert_uint_eq(yyjson_arr_size(messages), 3);

    /* First: user */
    yyjson_val *msg1 = yyjson_arr_get(messages, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg1, "role")), "user");

    /* Second: assistant with tool_calls */
    yyjson_val *msg2 = yyjson_arr_get(messages, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg2, "role")), "assistant");
    ck_assert_ptr_nonnull(yyjson_obj_get(msg2, "tool_calls"));

    /* Third: tool result */
    yyjson_val *msg3 = yyjson_arr_get(messages, 2);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg3, "role")), "tool");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg3, "tool_call_id")), "call_456");

    yyjson_doc_free(doc);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_tool_result_suite(void)
{
    Suite *s = suite_create("OpenAI Tool Result Messages");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    /* Tool result message creation tests */
    tcase_add_test(tc_core, test_tool_result_message_create);
    tcase_add_test(tc_core, test_tool_result_message_data_json_structure);
    tcase_add_test(tc_core, test_tool_result_message_talloc_hierarchy);
    tcase_add_test(tc_core, test_serialize_tool_result_message);
    tcase_add_test(tc_core, test_serialize_tool_call_and_result_sequence);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = client_tool_result_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
