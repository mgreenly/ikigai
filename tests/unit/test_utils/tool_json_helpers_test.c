#include <check.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/vendor/yyjson/yyjson.h"
#include "../../test_utils.h"

// Test fixtures
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

// Test: ik_test_tool_parse_success with valid success response
START_TEST(test_parse_success_valid) {
    const char *json = "{\"success\": true, \"data\": {\"output\": \"test\"}}";
    yyjson_doc *doc = NULL;

    yyjson_val *data = ik_test_tool_parse_success(json, &doc);

    // Verify data object returned
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(doc);

    // Verify we can extract output field
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_str_eq(output_str, "test");

    yyjson_doc_free(doc);
}
END_TEST
// Test: ik_test_tool_parse_error with valid error response
START_TEST(test_parse_error_valid)
{
    const char *json = "{\"success\": false, \"error\": \"File not found\"}";
    yyjson_doc *doc = NULL;

    const char *error = ik_test_tool_parse_error(json, &doc);

    // Verify error string returned
    ck_assert_ptr_nonnull(error);
    ck_assert_ptr_nonnull(doc);
    ck_assert_str_eq(error, "File not found");

    yyjson_doc_free(doc);
}

END_TEST
// Test: ik_test_tool_get_output extracts output field
START_TEST(test_get_output)
{
    const char *json = "{\"success\": true, \"data\": {\"output\": \"hello world\", \"exit_code\": 0}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *data = yyjson_obj_get(root, "data");

    const char *output = ik_test_tool_get_output(data);

    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "hello world");

    yyjson_doc_free(doc);
}

END_TEST
// Test: ik_test_tool_get_exit_code extracts exit_code field
START_TEST(test_get_exit_code)
{
    const char *json = "{\"success\": true, \"data\": {\"output\": \"test\", \"exit_code\": 42}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *data = yyjson_obj_get(root, "data");

    int64_t exit_code = ik_test_tool_get_exit_code(data);

    ck_assert_int_eq(exit_code, 42);

    yyjson_doc_free(doc);
}

END_TEST

// Create test suite
static Suite *tool_json_helpers_suite(void)
{
    Suite *s = suite_create("Tool JSON Helpers");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_parse_success_valid);
    tcase_add_test(tc_core, test_parse_error_valid);
    tcase_add_test(tc_core, test_get_output);
    tcase_add_test(tc_core, test_get_exit_code);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = tool_json_helpers_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
