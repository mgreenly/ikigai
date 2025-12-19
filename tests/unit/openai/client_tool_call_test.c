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
 * Tool call message creation tests
 */

START_TEST(test_tool_call_message_create) {
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_abc123",
        "function",
        "glob",
        "{\"pattern\": \"*.c\", \"path\": \"src/\"}",
        "glob(pattern=\"*.c\", path=\"src/\")"
        );

    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->kind, "tool_call");
    ck_assert_str_eq(msg->content, "glob(pattern=\"*.c\", path=\"src/\")");
    ck_assert_ptr_nonnull(msg->data_json);
}
END_TEST START_TEST(test_tool_call_message_data_json_structure)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_xyz789",
        "function",
        "file_read",
        "{\"path\": \"/etc/passwd\"}",
        "file_read(path=\"/etc/passwd\")"
        );

    ck_assert_ptr_nonnull(msg->data_json);

    /* Parse data_json and verify structure */
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_is_obj(root));

    /* Check id field */
    const char *id = yyjson_get_str(yyjson_obj_get(root, "id"));
    ck_assert_str_eq(id, "call_xyz789");

    /* Check type field */
    const char *type = yyjson_get_str(yyjson_obj_get(root, "type"));
    ck_assert_str_eq(type, "function");

    /* Check function object */
    yyjson_val *func = yyjson_obj_get(root, "function");
    ck_assert_ptr_nonnull(func);
    ck_assert(yyjson_is_obj(func));

    /* Check function name and arguments */
    const char *name = yyjson_get_str(yyjson_obj_get(func, "name"));
    const char *args = yyjson_get_str(yyjson_obj_get(func, "arguments"));
    ck_assert_str_eq(name, "file_read");
    ck_assert_str_eq(args, "{\"path\": \"/etc/passwd\"}");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_tool_call_message_talloc_hierarchy)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_test",
        "function",
        "test_func",
        "{}",
        "test()"
        );

    /* Message should be child of ctx */
    ck_assert_ptr_eq(talloc_parent(msg), ctx);

    /* Role, content, and data_json should be children of message */
    ck_assert_ptr_eq(talloc_parent(msg->kind), msg);
    ck_assert_ptr_eq(talloc_parent(msg->content), msg);
    ck_assert_ptr_eq(talloc_parent(msg->data_json), msg);
}

END_TEST START_TEST(test_tool_call_message_empty_arguments)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_empty",
        "function",
        "no_args_func",
        "{}",
        "no_args_func()"
        );

    ck_assert_ptr_nonnull(msg->data_json);

    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *func = yyjson_obj_get(root, "function");
    const char *args = yyjson_get_str(yyjson_obj_get(func, "arguments"));
    ck_assert_str_eq(args, "{}");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_tool_call_message_complex_arguments)
{
    const char *complex_args = "{\"nested\": {\"key\": \"value\"}, \"array\": [1, 2, 3]}";
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_complex",
        "function",
        "complex_func",
        complex_args,
        "complex_func(nested={key=value}, array=[1, 2, 3])"
        );

    ck_assert_ptr_nonnull(msg->data_json);

    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *func = yyjson_obj_get(root, "function");
    const char *args = yyjson_get_str(yyjson_obj_get(func, "arguments"));
    ck_assert_str_eq(args, complex_args);

    yyjson_doc_free(doc);
}

END_TEST
/*
 * Serialization tests for tool_call messages
 */

START_TEST(test_serialize_tool_call_message)
{
    /* Create conversation with tool_call message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_123",
        "function",
        "glob",
        "{\"pattern\": \"*.c\"}",
        "glob(pattern=\"*.c\")"
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

    /* Tool call should be serialized as role="assistant" with tool_calls array */
    const char *role = yyjson_get_str(yyjson_obj_get(first_msg, "role"));
    ck_assert_str_eq(role, "assistant");

    /* Check tool_calls array */
    yyjson_val *tool_calls = yyjson_obj_get(first_msg, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert(yyjson_is_arr(tool_calls));

    /* Check first tool call */
    yyjson_val *tool_call = yyjson_arr_get(tool_calls, 0);
    ck_assert_ptr_nonnull(tool_call);

    const char *tool_id = yyjson_get_str(yyjson_obj_get(tool_call, "id"));
    const char *tool_type = yyjson_get_str(yyjson_obj_get(tool_call, "type"));
    yyjson_val *tool_func = yyjson_obj_get(tool_call, "function");

    ck_assert_str_eq(tool_id, "call_123");
    ck_assert_str_eq(tool_type, "function");
    ck_assert_ptr_nonnull(tool_func);

    const char *func_name = yyjson_get_str(yyjson_obj_get(tool_func, "name"));
    const char *func_args = yyjson_get_str(yyjson_obj_get(tool_func, "arguments"));
    ck_assert_str_eq(func_name, "glob");
    ck_assert_str_eq(func_args, "{\"pattern\": \"*.c\"}");

    /* Tool call message should not have content field */
    yyjson_val *content = yyjson_obj_get(first_msg, "content");
    ck_assert_ptr_null(content);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_mixed_messages)
{
    /* Create conversation with both user and tool_call messages */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    /* Add user message */
    res_t user_msg_res = ik_openai_msg_create(ctx, "user", "Find all C files");
    ck_assert(!user_msg_res.is_err);
    res_t add_user = ik_openai_conversation_add_msg(conv, user_msg_res.ok);
    ck_assert(!add_user.is_err);

    /* Add tool_call message */
    ik_msg_t *tool_msg = ik_openai_msg_create_tool_call(
        ctx,
        "call_456",
        "function",
        "glob",
        "{\"pattern\": \"*.c\", \"path\": \"src/\"}",
        "glob(pattern=\"*.c\", path=\"src/\")"
        );
    res_t add_tool = ik_openai_conversation_add_msg(conv, tool_msg);
    ck_assert(!add_tool.is_err);

    /* Add assistant message */
    res_t asst_msg_res = ik_openai_msg_create(ctx, "assistant", "I found the files");
    ck_assert(!asst_msg_res.is_err);
    res_t add_asst = ik_openai_conversation_add_msg(conv, asst_msg_res.ok);
    ck_assert(!add_asst.is_err);

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
    ck_assert_ptr_nonnull(messages);

    /* Verify we have 3 messages */
    ck_assert_uint_eq(yyjson_arr_size(messages), 3);

    /* First message should be user */
    yyjson_val *msg1 = yyjson_arr_get(messages, 0);
    const char *role1 = yyjson_get_str(yyjson_obj_get(msg1, "role"));
    ck_assert_str_eq(role1, "user");

    /* Second message should be assistant (tool_call serialized) */
    yyjson_val *msg2 = yyjson_arr_get(messages, 1);
    const char *role2 = yyjson_get_str(yyjson_obj_get(msg2, "role"));
    ck_assert_str_eq(role2, "assistant");

    /* Verify tool_calls in second message */
    yyjson_val *tool_calls = yyjson_obj_get(msg2, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert(yyjson_is_arr(tool_calls));

    /* Third message should be regular assistant message with content */
    yyjson_val *msg3 = yyjson_arr_get(messages, 2);
    const char *role3 = yyjson_get_str(yyjson_obj_get(msg3, "role"));
    const char *content3 = yyjson_get_str(yyjson_obj_get(msg3, "content"));
    ck_assert_str_eq(role3, "assistant");
    ck_assert_str_eq(content3, "I found the files");

    yyjson_doc_free(doc);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_tool_call_suite(void)
{
    Suite *s = suite_create("OpenAI Tool Call Messages");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    /* Tool call message creation tests */
    tcase_add_test(tc_core, test_tool_call_message_create);
    tcase_add_test(tc_core, test_tool_call_message_data_json_structure);
    tcase_add_test(tc_core, test_tool_call_message_talloc_hierarchy);
    tcase_add_test(tc_core, test_tool_call_message_empty_arguments);
    tcase_add_test(tc_core, test_tool_call_message_complex_arguments);

    /* Serialization tests */
    tcase_add_test(tc_core, test_serialize_tool_call_message);
    tcase_add_test(tc_core, test_serialize_mixed_messages);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = client_tool_call_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
