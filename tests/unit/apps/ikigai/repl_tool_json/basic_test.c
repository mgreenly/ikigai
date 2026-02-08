#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/repl_tool_json.h"
#include "apps/ikigai/tool.h"
#include "shared/error.h"
#include "shared/wrapper_json.h"
#include "tests/helpers/test_utils_helper.h"
#include "vendor/yyjson/yyjson.h"

START_TEST(test_build_tool_result_data_json_with_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Build result_json with tool_success field
    const char *result_json = "{\"tool_success\": true, \"data\": \"example\"}";
    char *data_json = ik_build_tool_result_data_json(ctx, "call-123", "test_tool", result_json);

    ck_assert_ptr_nonnull(data_json);

    // Parse result to verify structure
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Verify required fields
    yyjson_val *tool_call_id = yyjson_obj_get(root, "tool_call_id");
    ck_assert_ptr_nonnull(tool_call_id);
    ck_assert_str_eq(yyjson_get_str(tool_call_id), "call-123");

    yyjson_val *name = yyjson_obj_get(root, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_get_str(name), "test_tool");

    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(yyjson_get_str(output), result_json);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_build_tool_result_data_json_invalid_json) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Test with invalid JSON - should handle gracefully and default success to false
    const char *result_json = "{this is not valid JSON}";
    char *data_json = ik_build_tool_result_data_json(ctx, "call-456", "broken_tool", result_json);

    ck_assert_ptr_nonnull(data_json);

    // Parse result to verify structure
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Verify output is preserved as-is
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(yyjson_get_str(output), result_json);

    // Verify success defaults to false when JSON parsing fails
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_build_tool_result_data_json_missing_tool_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Test with valid JSON but no tool_success field - should default to false
    const char *result_json = "{\"data\": \"example\", \"other_field\": 123}";
    char *data_json = ik_build_tool_result_data_json(ctx, "call-789", "incomplete_tool", result_json);

    ck_assert_ptr_nonnull(data_json);

    // Parse result to verify structure
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Verify output is preserved
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(yyjson_get_str(output), result_json);

    // Verify success defaults to false when tool_success field is missing
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_build_tool_call_data_json_basic) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a simple tool call
    ik_tool_call_t *tc = ik_tool_call_create(ctx, "call-abc", "example_tool", "{\"arg1\": \"value1\"}");
    ck_assert_ptr_nonnull(tc);

    char *data_json = ik_build_tool_call_data_json(ctx, tc, NULL, NULL, NULL);

    ck_assert_ptr_nonnull(data_json);

    // Parse result to verify structure
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Verify required fields
    yyjson_val *tool_call_id = yyjson_obj_get(root, "tool_call_id");
    ck_assert_ptr_nonnull(tool_call_id);
    ck_assert_str_eq(yyjson_get_str(tool_call_id), "call-abc");

    yyjson_val *tool_name = yyjson_obj_get(root, "tool_name");
    ck_assert_ptr_nonnull(tool_name);
    ck_assert_str_eq(yyjson_get_str(tool_name), "example_tool");

    yyjson_val *tool_args = yyjson_obj_get(root, "tool_args");
    ck_assert_ptr_nonnull(tool_args);
    ck_assert_str_eq(yyjson_get_str(tool_args), "{\"arg1\": \"value1\"}");

    // Verify optional fields are not present
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_null(thinking);

    yyjson_val *redacted = yyjson_obj_get(root, "redacted_thinking");
    ck_assert_ptr_null(redacted);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_build_tool_call_data_json_with_thinking) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_tool_call_t *tc = ik_tool_call_create(ctx, "call-def", "thinking_tool", "{}");
    ck_assert_ptr_nonnull(tc);

    const char *thinking_text = "This is my thought process";
    const char *thinking_sig = "signature-xyz";

    char *data_json = ik_build_tool_call_data_json(ctx, tc, thinking_text, thinking_sig, NULL);

    ck_assert_ptr_nonnull(data_json);

    // Parse result to verify structure
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Verify thinking object is present
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *text = yyjson_obj_get(thinking, "text");
    ck_assert_ptr_nonnull(text);
    ck_assert_str_eq(yyjson_get_str(text), thinking_text);

    yyjson_val *sig = yyjson_obj_get(thinking, "signature");
    ck_assert_ptr_nonnull(sig);
    ck_assert_str_eq(yyjson_get_str(sig), thinking_sig);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

static Suite *repl_tool_json_suite(void)
{
    Suite *s = suite_create("REPL Tool JSON");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_build_tool_result_data_json_with_success);
    tcase_add_test(tc_core, test_build_tool_result_data_json_invalid_json);
    tcase_add_test(tc_core, test_build_tool_result_data_json_missing_tool_success);
    tcase_add_test(tc_core, test_build_tool_call_data_json_basic);
    tcase_add_test(tc_core, test_build_tool_call_data_json_with_thinking);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_tool_json_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl_tool_json/basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
