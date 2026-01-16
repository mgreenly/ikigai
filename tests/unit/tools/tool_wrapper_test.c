#include "../../../src/tool_wrapper.h"

#include "../../../src/vendor/yyjson/yyjson.h"

#include <check.h>
#include <inttypes.h>
#include <string.h>
#include <talloc.h>

// Test wrapping successful tool output
START_TEST(test_wrap_success_basic) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tool_json[] = "{\"output\": \"test result\"}";

    char *wrapped = ik_tool_wrap_success(ctx, tool_json);

    ck_assert_ptr_nonnull(wrapped);

    // Parse and verify structure
    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *result = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result);
    yyjson_val *output = yyjson_obj_get(result, "output");
    ck_assert_str_eq(yyjson_get_str(output), "test result");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Test wrapping nested JSON objects
START_TEST(test_wrap_success_nested) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tool_json[] = "{\"data\": {\"key\": \"value\", \"count\": 42}}";

    char *wrapped = ik_tool_wrap_success(ctx, tool_json);

    ck_assert_ptr_nonnull(wrapped);

    // Parse and verify nested structure preserved
    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *result = yyjson_obj_get(root, "result");
    yyjson_val *data = yyjson_obj_get(result, "data");
    ck_assert_ptr_nonnull(data);

    yyjson_val *key = yyjson_obj_get(data, "key");
    ck_assert_str_eq(yyjson_get_str(key), "value");

    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 42);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Test wrapping empty JSON object
START_TEST(test_wrap_success_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tool_json[] = "{}";

    char *wrapped = ik_tool_wrap_success(ctx, tool_json);

    ck_assert_ptr_nonnull(wrapped);

    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *result = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result);
    ck_assert(yyjson_is_obj(result));

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Test handling of invalid JSON - should return failure wrapper
START_TEST(test_wrap_success_invalid_json) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tool_json[] = "not valid json {";

    char *wrapped = ik_tool_wrap_success(ctx, tool_json);

    ck_assert_ptr_nonnull(wrapped);

    // Should return failure wrapper
    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_str_eq(yyjson_get_str(error), "Tool returned invalid JSON");

    yyjson_val *code = yyjson_obj_get(root, "error_code");
    ck_assert_str_eq(yyjson_get_str(code), "INVALID_OUTPUT");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Test wrapping failure with error message
START_TEST(test_wrap_failure_basic) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char *wrapped = ik_tool_wrap_failure(ctx, "Execution failed", "EXEC_ERROR");

    ck_assert_ptr_nonnull(wrapped);

    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_str_eq(yyjson_get_str(error), "Execution failed");

    yyjson_val *code = yyjson_obj_get(root, "error_code");
    ck_assert_str_eq(yyjson_get_str(code), "EXEC_ERROR");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Test wrapping timeout error
START_TEST(test_wrap_failure_timeout) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char *wrapped = ik_tool_wrap_failure(ctx, "Tool execution timed out after 30 seconds", "TIMEOUT");

    ck_assert_ptr_nonnull(wrapped);

    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_str_eq(yyjson_get_str(error), "Tool execution timed out after 30 seconds");

    yyjson_val *code = yyjson_obj_get(root, "error_code");
    ck_assert_str_eq(yyjson_get_str(code), "TIMEOUT");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Test wrapping with empty error message
START_TEST(test_wrap_failure_empty_message) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char *wrapped = ik_tool_wrap_failure(ctx, "", "UNKNOWN");

    ck_assert_ptr_nonnull(wrapped);

    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_str_eq(yyjson_get_str(error), "");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

static Suite *tool_wrapper_suite(void)
{
    Suite *s = suite_create("tool_wrapper");

    TCase *tc_success = tcase_create("wrap_success");
    tcase_add_test(tc_success, test_wrap_success_basic);
    tcase_add_test(tc_success, test_wrap_success_nested);
    tcase_add_test(tc_success, test_wrap_success_empty);
    tcase_add_test(tc_success, test_wrap_success_invalid_json);
    suite_add_tcase(s, tc_success);

    TCase *tc_failure = tcase_create("wrap_failure");
    tcase_add_test(tc_failure, test_wrap_failure_basic);
    tcase_add_test(tc_failure, test_wrap_failure_timeout);
    tcase_add_test(tc_failure, test_wrap_failure_empty_message);
    suite_add_tcase(s, tc_failure);

    return s;
}

int32_t main(void)
{
    int32_t number_failed = 0;
    Suite *s = tool_wrapper_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
