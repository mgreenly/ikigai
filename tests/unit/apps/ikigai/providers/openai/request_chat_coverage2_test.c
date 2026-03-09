#include "tests/test_constants.h"
/**
 * @file request_chat_coverage2_test.c
 * @brief Additional coverage tests for request_chat.c (schema, multi-tool, full request)
 */

#include "apps/ikigai/providers/openai/request.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/tool.h"
#include "shared/error.h"
#include "shared/wrapper.h"
#include "vendor/yyjson/yyjson.h"
#include "request_chat_coverage_helper.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

static void *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
}

/**
 * Test: Serialize with empty system_prompt string (should not add system message)
 */
START_TEST(test_serialize_with_empty_system_prompt) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    req->system_prompt = talloc_strdup(ctx, ""); /* Empty string */

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *messages = yyjson_obj_get(yyjson_doc_get_root(doc), "messages");
    ck_assert_uint_eq(yyjson_arr_size(messages), 0); /* No system message */
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Serialize with NULL model to cover error path (line 133)
 */
START_TEST(test_serialize_null_model) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    req->model = NULL; /* NULL model */

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(!is_ok(&result));
}

END_TEST

/**
 * Test: Serialize with multiple tools to cover loop iteration branches
 */
START_TEST(test_serialize_with_multiple_tools) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_tool(ctx, req, "tool_one", "First tool",
                     "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    ik_test_add_tool(ctx,
                     req,
                     "tool_two",
                     "Second tool",
                     "{\"type\":\"object\",\"properties\":{\"arg1\":{\"type\":\"string\"}},\"required\":[\"arg1\"],\"additionalProperties\":false}");
    ik_test_add_tool(ctx, req, "tool_three", "Third tool",
                     "{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}},\"additionalProperties\":false}");
    req->tool_choice_mode = 0;

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    ck_assert_uint_eq(yyjson_arr_size(tools), 3);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Serialize with multiple messages to cover loop iteration branches
 */
START_TEST(test_serialize_with_multiple_messages) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_message(ctx, req, IK_ROLE_USER, "Hello");
    ik_test_add_message(ctx, req, IK_ROLE_ASSISTANT, "Hi there!");
    ik_test_add_message(ctx, req, IK_ROLE_USER, "How are you?");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *messages = yyjson_obj_get(yyjson_doc_get_root(doc), "messages");
    ck_assert_uint_eq(yyjson_arr_size(messages), 3);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Full-featured request with all options enabled
 * This exercises all code paths together to improve branch coverage
 */
START_TEST(test_serialize_full_featured_request) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    req->system_prompt = talloc_strdup(ctx, "You are a helpful assistant.");
    req->max_output_tokens = 4096;
    ik_test_add_message(ctx, req, IK_ROLE_USER, "Hello");
    ik_test_add_message(ctx, req, IK_ROLE_ASSISTANT, "Hi!");
    ik_test_add_tool(ctx,
                     req,
                     "get_weather",
                     "Get weather info",
                     "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"}},\"required\":[\"city\"],\"additionalProperties\":false}");
    ik_test_add_tool(ctx,
                     req,
                     "search",
                     "Search the web",
                     "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"additionalProperties\":false}");
    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, true, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Verify all fields are present */
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "model"));
    ck_assert_uint_eq(yyjson_arr_size(yyjson_obj_get(root, "messages")), 3); /* system + 2 */
    ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(root, "max_completion_tokens")), 4096);
    ck_assert(yyjson_get_bool(yyjson_obj_get(root, "stream")));
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "stream_options"));
    ck_assert_uint_eq(yyjson_arr_size(yyjson_obj_get(root, "tools")), 2);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(root, "tool_choice")), "required");

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Tool with schema containing 'items' object to cover line 56 branch
 * (items != NULL && yyjson_mut_is_obj(items))
 */
START_TEST(test_tool_schema_with_items) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    /* Schema with an array property whose items is an object with a format field */
    ik_test_add_tool(ctx, req, "list_tool", "Tool with array items",
                     "{\"type\":\"object\","
                     "\"properties\":{"
                     "  \"tags\":{"
                     "    \"type\":\"array\","
                     "    \"items\":{\"type\":\"string\",\"format\":\"uri\"}"
                     "  }"
                     "}}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Tool with schema containing 'oneOf' array to cover lines 63-64 branches
 * (combinator != NULL && yyjson_mut_is_arr(combinator))
 */
START_TEST(test_tool_schema_with_oneof) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    /* Schema with a oneOf combinator containing format fields to strip */
    ik_test_add_tool(ctx, req, "combo_tool", "Tool with oneOf schema",
                     "{\"type\":\"object\","
                     "\"properties\":{"
                     "  \"value\":{"
                     "    \"oneOf\":["
                     "      {\"type\":\"string\",\"format\":\"uri\"},"
                     "      {\"type\":\"number\"}"
                     "    ]"
                     "  }"
                     "}}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Tool with properties as array (malformed) to cover !yyjson_mut_is_obj() branch
 */
START_TEST(test_tool_properties_as_array) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    /* Properties is an array instead of object - malformed but shouldn't crash */
    ik_test_add_tool(ctx, req, "bad_tool", "Tool with array properties",
                     "{\"type\":\"object\",\"properties\":[],\"additionalProperties\":false}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    /* Should succeed - ensure_all_properties_required returns true early */
    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_doc_free(doc);
}

END_TEST

static Suite *request_chat_coverage2_suite(void)
{
    Suite *s = suite_create("request_chat_coverage2");

    TCase *tc_basic = tcase_create("basic_serialization");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_with_empty_system_prompt);
    tcase_add_test(tc_basic, test_serialize_null_model);
    tcase_add_test(tc_basic, test_serialize_with_multiple_tools);
    tcase_add_test(tc_basic, test_serialize_with_multiple_messages);
    tcase_add_test(tc_basic, test_serialize_full_featured_request);
    suite_add_tcase(s, tc_basic);

    TCase *tc_schema = tcase_create("schema_features");
    tcase_set_timeout(tc_schema, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_schema, setup, teardown);
    tcase_add_test(tc_schema, test_tool_schema_with_items);
    tcase_add_test(tc_schema, test_tool_schema_with_oneof);
    tcase_add_test(tc_schema, test_tool_properties_as_array);
    suite_add_tcase(s, tc_schema);

    return s;
}

int32_t main(void)
{
    Suite *s = request_chat_coverage2_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
                    "reports/check/unit/apps/ikigai/providers/openai/request_chat_coverage2_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
