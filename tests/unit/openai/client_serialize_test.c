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
 * Test suite
 */

static Suite *client_serialize_suite(void)
{
    Suite *s = suite_create("OpenAI Client Serialize");
    TCase *tc_call = tcase_create("ToolCall");
    TCase *tc_result = tcase_create("ToolResult");
    TCase *tc_combined = tcase_create("Combined");

    tcase_add_checked_fixture(tc_call, setup, teardown);
    tcase_add_checked_fixture(tc_result, setup, teardown);
    tcase_add_checked_fixture(tc_combined, setup, teardown);

    tcase_add_test(tc_call, test_serialize_tool_call_basic);
    tcase_add_test(tc_call, test_serialize_tool_call_complex);
    tcase_add_test(tc_call, test_serialize_tool_call_null_parent);

    tcase_add_test(tc_result, test_serialize_tool_result_basic);
    tcase_add_test(tc_result, test_serialize_tool_result_complex);
    tcase_add_test(tc_result, test_serialize_tool_result_null_parent);

    tcase_add_test(tc_combined, test_serialize_call_and_result_sequence);

    suite_add_tcase(s, tc_call);
    suite_add_tcase(s, tc_result);
    suite_add_tcase(s, tc_combined);

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
