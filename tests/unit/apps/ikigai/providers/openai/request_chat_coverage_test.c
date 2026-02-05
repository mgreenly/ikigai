#include "tests/test_constants.h"
/**
 * @file request_chat_coverage_test.c
 * @brief Coverage tests for request_chat.c
 *
 * Tests coverage gaps in ik_openai_serialize_chat_request and helpers.
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

/* Test: Serialize request with tools */
START_TEST(test_serialize_with_tools) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_tool(ctx, req, "test_tool", "A test tool",
                     "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);

    /* Verify tools are present */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert_uint_eq(yyjson_arr_size(tools), 1);

    yyjson_doc_free(doc);
}

END_TEST

/* Test: tool_choice_mode = 1 (IK_TOOL_NONE) */
START_TEST(test_tool_choice_none) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_tool(ctx, req, "test_tool", "A test tool",
                     "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 1; // IK_TOOL_NONE

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "none");
    yyjson_doc_free(doc);
}

END_TEST

/* Test: tool_choice_mode = 2 (IK_TOOL_REQUIRED) */
START_TEST(test_tool_choice_required) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_tool(ctx, req, "test_tool", "A test tool",
                     "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "required");
    yyjson_doc_free(doc);
}

END_TEST

/* Test: Invalid tool_choice_mode (default case) */
START_TEST(test_tool_choice_invalid) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_tool(ctx, req, "test_tool", "A test tool",
                     "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 999; /* Invalid value to trigger default case */

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "auto");
    yyjson_doc_free(doc);
}

END_TEST

/* Test: Serialize with system_prompt */
START_TEST(test_serialize_with_system_prompt) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    req->system_prompt = talloc_strdup(ctx, "You are a helpful assistant.");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *messages = yyjson_obj_get(yyjson_doc_get_root(doc), "messages");
    ck_assert_uint_ge(yyjson_arr_size(messages), 1);
    yyjson_val *first_msg = yyjson_arr_get_first(messages);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(first_msg, "role")), "system");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(first_msg, "content")), "You are a helpful assistant.");
    yyjson_doc_free(doc);
}

END_TEST

/* Test: Serialize with streaming=true */
START_TEST(test_serialize_with_streaming) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, true, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(root, "stream")));
    ck_assert(yyjson_get_bool(yyjson_obj_get(yyjson_obj_get(root, "stream_options"), "include_usage")));
    yyjson_doc_free(doc);
}

END_TEST

/* Test: Invalid tool parameters JSON */
START_TEST(test_tool_invalid_json_parameters) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_tool(ctx, req, "test_tool", "A test tool", "{invalid json}"); /* Invalid JSON */

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    /* Should fail due to invalid JSON parameters */
    ck_assert(!is_ok(&result));
}

END_TEST

/**
 * Test: Serialize with messages to cover lines 185-191
 */
START_TEST(test_serialize_with_messages) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    ik_test_add_message(ctx, req, IK_ROLE_USER, "Hello");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *messages = yyjson_obj_get(yyjson_doc_get_root(doc), "messages");
    ck_assert_uint_eq(yyjson_arr_size(messages), 1);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Serialize with max_output_tokens > 0 to cover lines 200-205
 */
START_TEST(test_serialize_with_max_output_tokens) {
    ik_request_t *req = ik_test_create_minimal_request(ctx);
    req->max_output_tokens = 2048;

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *max_tokens = yyjson_obj_get(yyjson_doc_get_root(doc), "max_completion_tokens");
    ck_assert_int_eq(yyjson_get_int(max_tokens), 2048);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: ik_openai_build_chat_url to cover lines 284-295
 */
START_TEST(test_build_chat_url) {
    const char *base_url = "https://api.openai.com";
    char *url = NULL;
    res_t result = ik_openai_build_chat_url(ctx, base_url, &url);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(url);
    ck_assert_str_eq(url, "https://api.openai.com/v1/chat/completions");
}

END_TEST

/**
 * Test: ik_openai_build_headers to cover lines 297-320
 */
START_TEST(test_build_headers) {
    const char *api_key = "sk-test-12345";
    char **headers = NULL;
    res_t result = ik_openai_build_headers(ctx, api_key, &headers);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(headers);

    /* Verify headers array has expected values */
    ck_assert_ptr_nonnull(headers[0]); /* Authorization header */
    ck_assert_ptr_nonnull(headers[1]); /* Content-Type header */
    ck_assert_ptr_null(headers[2]);    /* NULL terminator */

    /* Check Authorization header format */
    ck_assert(strstr(headers[0], "Authorization: Bearer") != NULL);
    ck_assert(strstr(headers[0], "sk-test-12345") != NULL);

    /* Check Content-Type header */
    ck_assert_str_eq(headers[1], "Content-Type: application/json");
}

END_TEST

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

static Suite *request_chat_coverage_suite(void)
{
    Suite *s = suite_create("request_chat_coverage");

    TCase *tc_tools = tcase_create("tool_serialization");
    tcase_set_timeout(tc_tools, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_with_tools);
    tcase_add_test(tc_tools, test_tool_choice_none);
    tcase_add_test(tc_tools, test_tool_choice_required);
    tcase_add_test(tc_tools, test_tool_choice_invalid);
    tcase_add_test(tc_tools, test_tool_invalid_json_parameters);
    tcase_add_test(tc_tools, test_tool_properties_as_array);
    suite_add_tcase(s, tc_tools);

    TCase *tc_basic = tcase_create("basic_serialization");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_with_system_prompt);
    tcase_add_test(tc_basic, test_serialize_with_streaming);
    tcase_add_test(tc_basic, test_serialize_with_messages);
    tcase_add_test(tc_basic, test_serialize_with_max_output_tokens);
    tcase_add_test(tc_basic, test_serialize_with_empty_system_prompt);
    tcase_add_test(tc_basic, test_serialize_null_model);
    tcase_add_test(tc_basic, test_serialize_with_multiple_tools);
    tcase_add_test(tc_basic, test_serialize_with_multiple_messages);
    tcase_add_test(tc_basic, test_serialize_full_featured_request);
    suite_add_tcase(s, tc_basic);

    TCase *tc_api = tcase_create("api_functions");
    tcase_set_timeout(tc_api, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_api, setup, teardown);
    tcase_add_test(tc_api, test_build_chat_url);
    tcase_add_test(tc_api, test_build_headers);
    suite_add_tcase(s, tc_api);

    return s;
}

int32_t main(void)
{
    Suite *s = request_chat_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/request_chat_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
