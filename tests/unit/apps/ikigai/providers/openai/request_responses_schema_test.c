#include "tests/test_constants.h"
/**
 * @file request_responses_schema_test.c
 * @brief Tests for schema format validation in OpenAI Responses API
 *
 * Tests the remove_format_validators() function indirectly through tool serialization.
 */

#include "request_responses_test_helper.h"

/* ================================================================
 * Schema Format Validator Tests
 * ================================================================ */

START_TEST(test_schema_with_array_items) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Tool with array items that have format validators
    const char *schema = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"urls\":{"
                "\"type\":\"array\","
                "\"items\":{"
                    "\"type\":\"string\","
                    "\"format\":\"uri\""
                "}"
            "}"
        "}"
    "}";

    ik_request_add_tool(req, "test_tool", "Test with array items", schema, true);

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    // Verify format was removed from items
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    ck_assert(yyjson_is_arr(tools));
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *params = yyjson_obj_get(tool, "parameters");
    yyjson_val *props = yyjson_obj_get(params, "properties");
    yyjson_val *urls = yyjson_obj_get(props, "urls");
    yyjson_val *items = yyjson_obj_get(urls, "items");
    ck_assert_ptr_null(yyjson_obj_get(items, "format"));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_schema_with_oneof_combinator) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Tool with oneOf combinator containing format validators
    const char *schema = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"value\":{"
                "\"oneOf\":["
                    "{\"type\":\"string\",\"format\":\"uri\"},"
                    "{\"type\":\"integer\"}"
                "]"
            "}"
        "}"
    "}";

    ik_request_add_tool(req, "test_tool", "Test with oneOf", schema, true);

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    // Verify format was removed from oneOf schemas
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *params = yyjson_obj_get(tool, "parameters");
    yyjson_val *props = yyjson_obj_get(params, "properties");
    yyjson_val *value = yyjson_obj_get(props, "value");
    yyjson_val *oneof = yyjson_obj_get(value, "oneOf");
    ck_assert(yyjson_is_arr(oneof));
    yyjson_val *first_option = yyjson_arr_get_first(oneof);
    ck_assert_ptr_null(yyjson_obj_get(first_option, "format"));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_schema_with_anyof_combinator) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Tool with anyOf combinator
    const char *schema = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"value\":{"
                "\"anyOf\":["
                    "{\"type\":\"string\",\"format\":\"email\"},"
                    "{\"type\":\"null\"}"
                "]"
            "}"
        "}"
    "}";

    ik_request_add_tool(req, "test_tool", "Test with anyOf", schema, true);

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    // Verify format was removed
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *params = yyjson_obj_get(tool, "parameters");
    yyjson_val *props = yyjson_obj_get(params, "properties");
    yyjson_val *value = yyjson_obj_get(props, "value");
    yyjson_val *anyof = yyjson_obj_get(value, "anyOf");
    ck_assert(yyjson_is_arr(anyof));
    yyjson_val *first_option = yyjson_arr_get_first(anyof);
    ck_assert_ptr_null(yyjson_obj_get(first_option, "format"));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_schema_with_allof_combinator) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Tool with allOf combinator
    const char *schema = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"value\":{"
                "\"allOf\":["
                    "{\"type\":\"string\"},"
                    "{\"format\":\"date-time\"}"
                "]"
            "}"
        "}"
    "}";

    ik_request_add_tool(req, "test_tool", "Test with allOf", schema, true);

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    // Verify format was removed from allOf schemas
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *params = yyjson_obj_get(tool, "parameters");
    yyjson_val *props = yyjson_obj_get(params, "properties");
    yyjson_val *value = yyjson_obj_get(props, "value");
    yyjson_val *allof = yyjson_obj_get(value, "allOf");
    ck_assert(yyjson_is_arr(allof));
    // Check second element which had the format field
    yyjson_val *second_option = yyjson_arr_get(allof, 1);
    ck_assert_ptr_null(yyjson_obj_get(second_option, "format"));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_schema_without_properties) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Tool with no properties (just type)
    const char *schema = "{\"type\":\"object\"}";

    ik_request_add_tool(req, "test_tool", "Test without properties", schema, true);

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    // Should succeed even without properties
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    ck_assert(yyjson_is_arr(tools));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_schema_without_items) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Array property without items definition
    const char *schema = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"values\":{\"type\":\"array\"}"
        "}"
    "}";

    ik_request_add_tool(req, "test_tool", "Test without items", schema, true);

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tools = yyjson_obj_get(yyjson_doc_get_root(doc), "tools");
    ck_assert(yyjson_is_arr(tools));
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_schema_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Schema Tests");

    TCase *tc_schema = tcase_create("Schema Format Validators");
    tcase_set_timeout(tc_schema, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_schema, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_schema, test_schema_with_array_items);
    tcase_add_test(tc_schema, test_schema_with_oneof_combinator);
    tcase_add_test(tc_schema, test_schema_with_anyof_combinator);
    tcase_add_test(tc_schema, test_schema_with_allof_combinator);
    tcase_add_test(tc_schema, test_schema_without_properties);
    tcase_add_test(tc_schema, test_schema_without_items);
    suite_add_tcase(s, tc_schema);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_schema_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/request_responses_schema_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
