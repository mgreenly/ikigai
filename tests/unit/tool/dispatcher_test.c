#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"
#include "../../test_utils.h"

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

START_TEST(test_dispatch_glob_with_valid_json) {
    const char *arguments = "{\"pattern\": \"*.c\", \"path\": \"/tmp\"}";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_ptr_nonnull(data);

    yyjson_doc_free(doc);
}
END_TEST START_TEST(test_dispatch_glob_returns_exec_result)
{
    const char *arguments = "{\"pattern\": \"*.json\"}";
    res_t dispatch_res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!dispatch_res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(dispatch_res.ok, &doc);
    ck_assert_ptr_nonnull(data);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_invalid_json_arguments)
{
    const char *arguments = "{invalid json";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    // Dispatcher errors have only {"error": "..."}, not {"success": false, "error": "..."}
    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Invalid JSON arguments");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_glob_missing_required_pattern)
{
    const char *arguments = "{\"path\": \"/tmp\"}";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
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
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, "unknown_tool", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
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
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, NULL, arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert(strstr(error_msg, "Unknown tool") != NULL || strstr(error_msg, "tool") != NULL);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_empty_tool_name)
{
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, "", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_msg);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_file_read_missing_path)
{
    const char *arguments = "{}";
    res_t res = ik_tool_dispatch(ctx, "file_read", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: path");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_file_read_not_found)
{
    const char *arguments = "{\"path\": \"/nonexistent/file/that/does/not/exist\"}";
    res_t res = ik_tool_dispatch(ctx, "file_read", arguments);

    ck_assert(!res.is_err);

    // file_read tool returns full success/error envelope
    yyjson_doc *doc = NULL;
    const char *error_msg = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error_msg, "not found") != NULL || strstr(error_msg, "No such file") != NULL);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_grep_missing_pattern)
{
    const char *arguments = "{}";
    res_t res = ik_tool_dispatch(ctx, "grep", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: pattern");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_grep_with_matches)
{
    const char *arguments = "{\"pattern\": \"test\", \"glob\": \"*.c\", \"path\": \"src\"}";
    res_t res = ik_tool_dispatch(ctx, "grep", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_ptr_nonnull(data);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_bash_success)
{
    const char *arguments = "{\"command\": \"echo test\"}";
    res_t res = ik_tool_dispatch(ctx, "bash", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_ptr_nonnull(data);

    const char *output_str = ik_test_tool_get_output(data);
    ck_assert(strstr(output_str, "test") != NULL);

    int64_t exit_code = ik_test_tool_get_exit_code(data);
    ck_assert_int_eq(exit_code, 0);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_bash_missing_command)
{
    const char *arguments = "{}";
    res_t res = ik_tool_dispatch(ctx, "bash", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: command");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_error_format_single_field)
{
    const char *arguments = "{\"pattern\": \"*.c\"}";
    res_t res = ik_tool_dispatch(ctx, "nonexistent", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_msg);

    // Verify only one field exists
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
    const char *arguments = "{\"pattern\": \"Makefile\"}";
    res_t res = ik_tool_dispatch(ctx, "glob", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_ptr_nonnull(data);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_null_arguments)
{
    res_t res = ik_tool_dispatch(ctx, "glob", NULL);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: pattern");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_file_write_missing_path)
{
    const char *arguments = "{\"content\": \"test\"}";
    res_t res = ik_tool_dispatch(ctx, "file_write", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: path");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_file_write_missing_content)
{
    const char *arguments = "{\"path\": \"/tmp/test\"}";
    res_t res = ik_tool_dispatch(ctx, "file_write", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = yyjson_read(res.ok, strlen(res.ok), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    const char *error_msg = yyjson_get_str(error);
    ck_assert_str_eq(error_msg, "Missing required parameter: content");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_dispatch_file_write_success)
{
    char test_file[] = "/tmp/ikigai-dispatcher-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);
    unlink(test_file);

    char arguments[512];
    snprintf(arguments, sizeof(arguments),
             "{\"path\": \"%s\", \"content\": \"test content\"}",
             test_file);

    res_t res = ik_tool_dispatch(ctx, "file_write", arguments);

    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_ptr_nonnull(data);

    yyjson_doc_free(doc);

    unlink(test_file);
}

END_TEST

static Suite *dispatcher_suite(void)
{
    Suite *s = suite_create("Tool Dispatcher");

    TCase *tc_dispatch = tcase_create("Dispatcher");
    tcase_add_checked_fixture(tc_dispatch, setup, teardown);
    tcase_set_timeout(tc_dispatch, 30);

    tcase_add_test(tc_dispatch, test_dispatch_glob_with_valid_json);
    tcase_add_test(tc_dispatch, test_dispatch_glob_returns_exec_result);
    tcase_add_test(tc_dispatch, test_dispatch_glob_with_null_path);

    tcase_add_test(tc_dispatch, test_dispatch_null_arguments);
    tcase_add_test(tc_dispatch, test_dispatch_invalid_json_arguments);
    tcase_add_test(tc_dispatch, test_dispatch_glob_missing_required_pattern);
    tcase_add_test(tc_dispatch, test_dispatch_unknown_tool);
    tcase_add_test(tc_dispatch, test_dispatch_null_tool_name);
    tcase_add_test(tc_dispatch, test_dispatch_empty_tool_name);

    tcase_add_test(tc_dispatch, test_dispatch_file_read_missing_path);
    tcase_add_test(tc_dispatch, test_dispatch_file_read_not_found);
    tcase_add_test(tc_dispatch, test_dispatch_grep_missing_pattern);
    tcase_add_test(tc_dispatch, test_dispatch_grep_with_matches);

    tcase_add_test(tc_dispatch, test_dispatch_file_write_missing_path);
    tcase_add_test(tc_dispatch, test_dispatch_file_write_missing_content);
    tcase_add_test(tc_dispatch, test_dispatch_file_write_success);

    tcase_add_test(tc_dispatch, test_dispatch_bash_success);
    tcase_add_test(tc_dispatch, test_dispatch_bash_missing_command);

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
