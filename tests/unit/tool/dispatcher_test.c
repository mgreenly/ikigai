#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"

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

// ============================================================================
// ik_tool_dispatch tests
// ============================================================================

START_TEST(test_dispatch_glob_with_valid_json) {
    // Dispatch to glob tool with valid pattern argument
    const char *arguments = "{\"pattern\": \"*.c\", \"path\": \"/tmp\"}";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    // Should succeed
    ck_assert(!res.is_err);

    // Result should be JSON string
    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result to verify it's valid JSON from glob
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    // Should have success field from glob result
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_is_bool(success));

    yyjson_doc_free(doc);
}
END_TEST START_TEST(test_dispatch_glob_returns_exec_result)
{
    // Verify that dispatch returns the exact result from ik_tool_exec_glob
    const char *arguments = "{\"pattern\": \"*.json\"}";
    res_t dispatch_res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!dispatch_res.is_err);

    char *dispatch_json = dispatch_res.ok;
    ck_assert_ptr_nonnull(dispatch_json);

    // Result should be valid JSON with success field from glob
    yyjson_doc *doc = yyjson_read(dispatch_json, strlen(dispatch_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_invalid_json_arguments)
{
    // Invalid JSON should return error JSON
    const char *arguments = "{invalid json";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    // Result should be error JSON with single "error" field
    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert(yyjson_is_str(error));

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Invalid JSON arguments");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_glob_missing_required_pattern)
{
    // Missing required "pattern" parameter should return error JSON
    const char *arguments = "{\"path\": \"/tmp\"}";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    // Result should be error JSON
    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: pattern");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_unknown_tool)
{
    // Unknown tool name should return error JSON
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, "unknown_tool", arguments);

    ck_assert(!res.is_err);

    // Result should be error JSON
    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Unknown tool: unknown_tool");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_null_tool_name)
{
    // NULL tool_name should return error JSON
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, NULL, arguments);

    ck_assert(!res.is_err);

    // Result should be error JSON
    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    // Should indicate NULL or empty tool name
    ck_assert(strstr(error_msg, "Unknown tool") != NULL || strstr(error_msg, "tool") != NULL);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_empty_tool_name)
{
    // Empty tool_name should return error JSON
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, "", arguments);

    ck_assert(!res.is_err);

    // Result should be error JSON
    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_file_read_not_found)
{
    // file_read tool should return error when file doesn't exist
    const char *arguments = "{\"path\": \"/nonexistent/file/that/does/not/exist\"}";
    res_t res = ik_tool_dispatch(ctx, "file_read", arguments);

    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert(strstr(error_msg, "not found") != NULL || strstr(error_msg, "No such file") != NULL);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_grep_with_matches)
{
    // grep tool should work and find matches
    const char *arguments = "{\"pattern\": \"test\"}";
    res_t res = ik_tool_dispatch(ctx, "grep", arguments);

    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_unimplemented_file_write)
{
    // file_write tool should return "not implemented" error
    const char *arguments = "{\"path\": \"/tmp/test\", \"content\": \"test\"}";
    res_t res = ik_tool_dispatch(ctx, "file_write", arguments);

    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Tool not implemented: file_write");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_unimplemented_bash)
{
    // bash tool should return "not implemented" error
    const char *arguments = "{\"command\": \"ls\"}";
    res_t res = ik_tool_dispatch(ctx, "bash", arguments);

    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Tool not implemented: bash");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_error_format_single_field)
{
    // Verify error JSON has only "error" field (as per spec)
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, "nonexistent", arguments);

    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify only "error" field exists
    yyjson_obj_iter iter = yyjson_obj_iter_with(root);
    yyjson_val *key = NULL;
    int field_count = 0;
    while ((key = yyjson_obj_iter_next(&iter))) {
        field_count++;
        const char *key_str = yyjson_get_str(key);
        ck_assert_str_eq(key_str, "error");
    }
    ck_assert_int_eq(field_count, 1);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_glob_with_null_path)
{
    // Glob with NULL path should still work (passed to exec_glob)
    const char *arguments = "{\"pattern\": \"Makefile\"}";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Should return valid glob result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_null_arguments)
{
    // NULL arguments should be handled gracefully
    res_t res = ik_tool_dispatch(ctx, "glob", NULL);

    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Should return error JSON (missing required pattern parameter)
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert(yyjson_is_str(error));

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: pattern");

    yyjson_doc_free(doc);
}

END_TEST

// Test suite
static Suite *dispatcher_suite(void)
{
    Suite *s = suite_create("Tool Dispatcher");

    TCase *tc_dispatch = tcase_create("Dispatcher");
    tcase_add_checked_fixture(tc_dispatch, setup, teardown);

    // Glob tests
    tcase_add_test(tc_dispatch, test_dispatch_glob_with_valid_json);
    tcase_add_test(tc_dispatch, test_dispatch_glob_returns_exec_result);
    tcase_add_test(tc_dispatch, test_dispatch_glob_with_null_path);

    // Error handling tests
    tcase_add_test(tc_dispatch, test_dispatch_null_arguments);
    tcase_add_test(tc_dispatch, test_dispatch_invalid_json_arguments);
    tcase_add_test(tc_dispatch, test_dispatch_glob_missing_required_pattern);
    tcase_add_test(tc_dispatch, test_dispatch_unknown_tool);
    tcase_add_test(tc_dispatch, test_dispatch_null_tool_name);
    tcase_add_test(tc_dispatch, test_dispatch_empty_tool_name);

    // Implemented tools tests
    tcase_add_test(tc_dispatch, test_dispatch_file_read_not_found);
    tcase_add_test(tc_dispatch, test_dispatch_grep_with_matches);

    // Unimplemented tools tests
    tcase_add_test(tc_dispatch, test_dispatch_unimplemented_file_write);
    tcase_add_test(tc_dispatch, test_dispatch_unimplemented_bash);

    // Error format tests
    tcase_add_test(tc_dispatch, test_dispatch_error_format_single_field);

    suite_add_tcase(s, tc_dispatch);

    return s;
}

int main(void)
{
    Suite *s = dispatcher_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
