#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/client.h"
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

/* Helper to verify tool_call serialization */
static void verify_tool_call_serialization(yyjson_mut_val *msg_obj,
                                           const char *expected_id,
                                           const char *expected_type,
                                           const char *expected_name,
                                           const char *expected_args)
{
    const char *role = yyjson_mut_get_str(yyjson_mut_obj_get(msg_obj, "role"));
    ck_assert_str_eq(role, "assistant");

    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(msg_obj, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert(yyjson_mut_is_arr(tool_calls));
    ck_assert_uint_eq(yyjson_mut_arr_size(tool_calls), 1);

    yyjson_mut_val *tool_call = yyjson_mut_arr_get(tool_calls, 0);
    const char *id = yyjson_mut_get_str(yyjson_mut_obj_get(tool_call, "id"));
    const char *type = yyjson_mut_get_str(yyjson_mut_obj_get(tool_call, "type"));
    ck_assert_str_eq(id, expected_id);
    ck_assert_str_eq(type, expected_type);

    yyjson_mut_val *func = yyjson_mut_obj_get(tool_call, "function");
    ck_assert_ptr_nonnull(func);
    const char *name = yyjson_mut_get_str(yyjson_mut_obj_get(func, "name"));
    const char *args = yyjson_mut_get_str(yyjson_mut_obj_get(func, "arguments"));
    ck_assert_str_eq(name, expected_name);
    ck_assert_str_eq(args, expected_args);
}

/* Helper to verify tool_result serialization */
static void verify_tool_result_serialization(yyjson_mut_val *msg_obj,
                                             const char *expected_id,
                                             const char *expected_content)
{
    const char *role = yyjson_mut_get_str(yyjson_mut_obj_get(msg_obj, "role"));
    ck_assert_str_eq(role, "tool");

    const char *id = yyjson_mut_get_str(yyjson_mut_obj_get(msg_obj, "tool_call_id"));
    const char *content = yyjson_mut_get_str(yyjson_mut_obj_get(msg_obj, "content"));
    ck_assert_str_eq(id, expected_id);
    ck_assert_str_eq(content, expected_content);
}

/*
 * Tool call serialization tests
 */

START_TEST(test_serialize_tool_call_basic) {
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx, "call_123", "function", "glob", "{\"pattern\": \"*.c\"}", "glob(pattern=\"*.c\")"
        );

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_call_msg(doc, msg_obj, msg, ctx);

    verify_tool_call_serialization(msg_obj, "call_123", "function", "glob", "{\"pattern\": \"*.c\"}");
    yyjson_mut_doc_free(doc);
}
END_TEST START_TEST(test_serialize_tool_call_complex)
{
    const char *args = "{\"nested\": {\"key\": \"value\"}, \"array\": [1, 2, 3]}";
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        ctx, "call_complex", "function", "func", args, "func(...)"
        );

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_call_msg(doc, msg_obj, msg, ctx);

    verify_tool_call_serialization(msg_obj, "call_complex", "function", "func", args);
    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_call_null_parent)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_call(
        NULL, "call_null", "function", "test", "{}", "test()"
        );

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_call_msg(doc, msg_obj, msg, NULL);

    verify_tool_call_serialization(msg_obj, "call_null", "function", "test", "{}");
    yyjson_mut_doc_free(doc);
    talloc_free(msg);
}

END_TEST
/*
 * Tool result serialization tests
 */

START_TEST(test_serialize_tool_result_basic)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_result(
        ctx, "call_123", "{\"success\": true, \"data\": \"result\"}"
        );

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_result_msg(doc, msg_obj, msg, ctx);

    verify_tool_result_serialization(msg_obj, "call_123", "{\"success\": true, \"data\": \"result\"}");
    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_result_complex)
{
    const char *content = "{\"nested\": {\"deep\": {\"value\": 42}}, \"array\": [\"a\", \"b\"]}";
    ik_msg_t *msg = ik_openai_msg_create_tool_result(ctx, "call_complex", content);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_result_msg(doc, msg_obj, msg, ctx);

    verify_tool_result_serialization(msg_obj, "call_complex", content);
    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_result_null_parent)
{
    ik_msg_t *msg = ik_openai_msg_create_tool_result(NULL, "call_null", "{}");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_result_msg(doc, msg_obj, msg, NULL);

    verify_tool_result_serialization(msg_obj, "call_null", "{}");
    yyjson_mut_doc_free(doc);
    talloc_free(msg);
}

END_TEST
/*
 * Combined tests
 */

START_TEST(test_serialize_call_and_result_sequence)
{
    ik_msg_t *call_msg = ik_openai_msg_create_tool_call(
        ctx, "call_seq", "function", "func", "{\"a\": 1}", "func(a=1)"
        );
    ik_msg_t *result_msg = ik_openai_msg_create_tool_result(
        ctx, "call_seq", "{\"output\": \"success\"}"
        );

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

    yyjson_mut_val *call_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_call_msg(doc, call_obj, call_msg, ctx);
    verify_tool_call_serialization(call_obj, "call_seq", "function", "func", "{\"a\": 1}");

    yyjson_mut_val *result_obj = yyjson_mut_obj(doc);
    ik_openai_serialize_tool_result_msg(doc, result_obj, result_msg, ctx);
    verify_tool_result_serialization(result_obj, "call_seq", "{\"output\": \"success\"}");

    yyjson_mut_doc_free(doc);
}

END_TEST
/*
 * Metadata filtering tests
 */

START_TEST(test_serialize_request_filters_metadata_events)
{
    /* Create conversation with mixed message types */
    ik_openai_conversation_t *conv = talloc_zero(ctx, ik_openai_conversation_t);
    conv->messages = talloc_array(ctx, ik_msg_t *, 7);
    conv->message_count = 7;

    /* Create messages: system, clear, user, agent_killed, assistant, mark, user */
    ik_msg_t *msg0 = talloc_zero(conv->messages, ik_msg_t);
    msg0->kind = talloc_strdup(msg0, "system");
    msg0->content = talloc_strdup(msg0, "You are a helpful assistant");
    conv->messages[0] = msg0;

    ik_msg_t *msg1 = talloc_zero(conv->messages, ik_msg_t);
    msg1->kind = talloc_strdup(msg1, "clear");
    msg1->content = NULL;  /* Metadata events have NULL content */
    conv->messages[1] = msg1;

    ik_msg_t *msg2 = talloc_zero(conv->messages, ik_msg_t);
    msg2->kind = talloc_strdup(msg2, "user");
    msg2->content = talloc_strdup(msg2, "Hello");
    conv->messages[2] = msg2;

    ik_msg_t *msg3 = talloc_zero(conv->messages, ik_msg_t);
    msg3->kind = talloc_strdup(msg3, "agent_killed");
    msg3->content = NULL;
    conv->messages[3] = msg3;

    ik_msg_t *msg4 = talloc_zero(conv->messages, ik_msg_t);
    msg4->kind = talloc_strdup(msg4, "assistant");
    msg4->content = talloc_strdup(msg4, "Hi there");
    conv->messages[4] = msg4;

    ik_msg_t *msg5 = talloc_zero(conv->messages, ik_msg_t);
    msg5->kind = talloc_strdup(msg5, "mark");
    msg5->content = NULL;
    conv->messages[5] = msg5;

    ik_msg_t *msg6 = talloc_zero(conv->messages, ik_msg_t);
    msg6->kind = talloc_strdup(msg6, "user");
    msg6->content = talloc_strdup(msg6, "What is 2+2?");
    conv->messages[6] = msg6;

    /* Create request */
    ik_openai_request_t *request = talloc_zero(ctx, ik_openai_request_t);
    request->model = talloc_strdup(request, "gpt-5-mini");
    request->conv = conv;

    /* Serialize the request */
    ik_tool_choice_t choice = ik_tool_choice_auto();
    char *json_str = ik_openai_serialize_request(ctx, request, choice);
    ck_assert_ptr_nonnull(json_str);

    /* Parse the JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    /* Should have only 4 messages (system, user, assistant, user) - metadata filtered out */
    size_t msg_count = yyjson_arr_size(messages);
    ck_assert_uint_eq(msg_count, 4);

    /* Verify first message is system */
    yyjson_val *json_msg0 = yyjson_arr_get(messages, 0);
    const char *role0 = yyjson_get_str(yyjson_obj_get(json_msg0, "role"));
    const char *content0 = yyjson_get_str(yyjson_obj_get(json_msg0, "content"));
    ck_assert_str_eq(role0, "system");
    ck_assert_str_eq(content0, "You are a helpful assistant");

    /* Verify second message is user (clear was filtered) */
    yyjson_val *json_msg1 = yyjson_arr_get(messages, 1);
    const char *role1 = yyjson_get_str(yyjson_obj_get(json_msg1, "role"));
    const char *content1 = yyjson_get_str(yyjson_obj_get(json_msg1, "content"));
    ck_assert_str_eq(role1, "user");
    ck_assert_str_eq(content1, "Hello");

    /* Verify third message is assistant (agent_killed was filtered) */
    yyjson_val *json_msg2 = yyjson_arr_get(messages, 2);
    const char *role2 = yyjson_get_str(yyjson_obj_get(json_msg2, "role"));
    const char *content2 = yyjson_get_str(yyjson_obj_get(json_msg2, "content"));
    ck_assert_str_eq(role2, "assistant");
    ck_assert_str_eq(content2, "Hi there");

    /* Verify fourth message is user (mark was filtered) */
    yyjson_val *json_msg3 = yyjson_arr_get(messages, 3);
    const char *role3 = yyjson_get_str(yyjson_obj_get(json_msg3, "role"));
    const char *content3 = yyjson_get_str(yyjson_obj_get(json_msg3, "content"));
    ck_assert_str_eq(role3, "user");
    ck_assert_str_eq(content3, "What is 2+2?");

    yyjson_doc_free(doc);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_serialize_suite(void)
{
    Suite *s = suite_create("OpenAI Client Serialize");
    TCase *tc_call = tcase_create("ToolCall");
    TCase *tc_result = tcase_create("ToolResult");
    TCase *tc_combined = tcase_create("Combined");
    TCase *tc_filter = tcase_create("MetadataFilter");

    tcase_add_checked_fixture(tc_call, setup, teardown);
    tcase_add_checked_fixture(tc_result, setup, teardown);
    tcase_add_checked_fixture(tc_combined, setup, teardown);
    tcase_add_checked_fixture(tc_filter, setup, teardown);

    tcase_add_test(tc_call, test_serialize_tool_call_basic);
    tcase_add_test(tc_call, test_serialize_tool_call_complex);
    tcase_add_test(tc_call, test_serialize_tool_call_null_parent);

    tcase_add_test(tc_result, test_serialize_tool_result_basic);
    tcase_add_test(tc_result, test_serialize_tool_result_complex);
    tcase_add_test(tc_result, test_serialize_tool_result_null_parent);

    tcase_add_test(tc_combined, test_serialize_call_and_result_sequence);

    tcase_add_test(tc_filter, test_serialize_request_filters_metadata_events);

    suite_add_tcase(s, tc_call);
    suite_add_tcase(s, tc_result);
    suite_add_tcase(s, tc_combined);
    suite_add_tcase(s, tc_filter);

    return s;
}

int main(void)
{
    Suite *s = client_serialize_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
