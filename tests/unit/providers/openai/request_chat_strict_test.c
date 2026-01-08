#include "../../../test_constants.h"
/**
 * @file request_chat_strict_test.c
 * @brief Tests for OpenAI strict mode tool serialization
 *
 * OpenAI's strict mode (strict:true on function tools) requires ALL properties
 * to be listed in the required[] array. These tests verify that behavior.
 */

#include "providers/openai/request.h"
#include "providers/request.h"
#include "message.h"
#include "tool.h"
#include "error.h"
#include "vendor/yyjson/yyjson.h"

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

/* Helper: Create a minimal request */
static ik_request_t *create_minimal_request(void)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "gpt-4");
    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->max_output_tokens = 0;
    req->tool_count = 0;
    return req;
}

/* Helper: Add a tool to a request */
static void add_tool(ik_request_t *req, const char *name, const char *desc, const char *params)
{
    size_t idx = req->tool_count++;
    req->tools = talloc_realloc(ctx, req->tools, ik_tool_def_t, (unsigned int)req->tool_count);
    req->tools[idx].name = talloc_strdup(ctx, name);
    req->tools[idx].description = talloc_strdup(ctx, desc);
    req->tools[idx].parameters = talloc_strdup(ctx, params);
}

/**
 * Test: Verify all properties are added to required array for OpenAI strict mode
 * OpenAI requires ALL properties in required[] when strict:true is set.
 * This was the root cause of the "Missing 'path'" error for glob tool.
 */
START_TEST(test_strict_mode_all_properties_required) {
    ik_request_t *req = create_minimal_request();
    // Tool with optional parameter (path is NOT in required[])
    add_tool(req,
             "glob",
             "Find files",
             "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"}},\"required\":[\"pattern\"],\"additionalProperties\":false}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *func = yyjson_obj_get(tool, "function");
    yyjson_val *params = yyjson_obj_get(func, "parameters");
    yyjson_val *required = yyjson_obj_get(params, "required");

    // Both pattern AND path must be in required array
    ck_assert_ptr_nonnull(required);
    ck_assert(yyjson_is_arr(required));
    ck_assert_uint_eq(yyjson_arr_size(required), 2);

    // Check both properties are present (order may vary)
    bool found_pattern = false;
    bool found_path = false;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(required, &iter);
    yyjson_val *val;
    while ((val = yyjson_arr_iter_next(&iter)) != NULL) {
        const char *str = yyjson_get_str(val);
        if (strcmp(str, "pattern") == 0) found_pattern = true;
        if (strcmp(str, "path") == 0) found_path = true;
    }
    ck_assert(found_pattern);
    ck_assert(found_path);

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Verify strict:true is set on tool functions
 */
START_TEST(test_strict_mode_flag_set) {
    ik_request_t *req = create_minimal_request();
    add_tool(req,
             "test_tool",
             "A test tool",
             "{\"type\":\"object\",\"properties\":{\"arg\":{\"type\":\"string\"}},\"required\":[\"arg\"],\"additionalProperties\":false}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *func = yyjson_obj_get(tool, "function");
    yyjson_val *strict = yyjson_obj_get(func, "strict");

    ck_assert_ptr_nonnull(strict);
    ck_assert(yyjson_get_bool(strict));

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Tool with no properties should still serialize correctly
 */
START_TEST(test_strict_mode_empty_properties) {
    ik_request_t *req = create_minimal_request();
    add_tool(req, "no_args", "Tool with no arguments",
             "{\"type\":\"object\",\"properties\":{},\"required\":[],\"additionalProperties\":false}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *func = yyjson_obj_get(tool, "function");
    yyjson_val *params = yyjson_obj_get(func, "parameters");
    yyjson_val *required = yyjson_obj_get(params, "required");

    ck_assert_ptr_nonnull(required);
    ck_assert(yyjson_is_arr(required));
    ck_assert_uint_eq(yyjson_arr_size(required), 0);

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Tool with missing properties object should not crash
 */
START_TEST(test_strict_mode_no_properties_object) {
    ik_request_t *req = create_minimal_request();
    // Malformed schema without properties object
    add_tool(req, "weird", "Weird tool", "{\"type\":\"object\",\"additionalProperties\":false}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    // Should succeed without crashing
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Multiple tools with mixed required/optional params
 */
START_TEST(test_strict_mode_multiple_tools) {
    ik_request_t *req = create_minimal_request();

    // glob tool: pattern required, path optional
    add_tool(req,
             "glob",
             "Find files",
             "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"}},\"required\":[\"pattern\"],\"additionalProperties\":false}");

    // grep tool: pattern required, path and glob optional
    add_tool(req,
             "grep",
             "Search files",
             "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"glob\":{\"type\":\"string\"}},\"required\":[\"pattern\"],\"additionalProperties\":false}");

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    ck_assert_uint_eq(yyjson_arr_size(tools), 2);

    // Check first tool (glob) has 2 required params
    yyjson_val *tool1 = yyjson_arr_get(tools, 0);
    yyjson_val *req1 = yyjson_obj_get(yyjson_obj_get(yyjson_obj_get(tool1, "function"), "parameters"), "required");
    ck_assert_uint_eq(yyjson_arr_size(req1), 2);

    // Check second tool (grep) has 3 required params
    yyjson_val *tool2 = yyjson_arr_get(tools, 1);
    yyjson_val *req2 = yyjson_obj_get(yyjson_obj_get(yyjson_obj_get(tool2, "function"), "parameters"), "required");
    ck_assert_uint_eq(yyjson_arr_size(req2), 3);

    yyjson_doc_free(doc);
}

END_TEST

static Suite *request_chat_strict_suite(void)
{
    Suite *s = suite_create("request_chat_strict");

    TCase *tc = tcase_create("strict_mode");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_strict_mode_all_properties_required);
    tcase_add_test(tc, test_strict_mode_flag_set);
    tcase_add_test(tc, test_strict_mode_empty_properties);
    tcase_add_test(tc, test_strict_mode_no_properties_object);
    tcase_add_test(tc, test_strict_mode_multiple_tools);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = request_chat_strict_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
