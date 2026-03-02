#include "apps/ikigai/tool_executor.h"

#include "shared/error.h"
#include "shared/json_allocator.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/tool_external.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/wrapper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_tool_registry_t *registry;
static ik_paths_t *paths;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    registry = ik_tool_registry_create(test_ctx);

    // Create test directories
    mkdir("/tmp/bin", 0755);
    mkdir("/tmp/state", 0755);
    mkdir("/tmp/cache", 0755);

    setenv("IKIGAI_BIN_DIR", "/tmp/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/tmp/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/tmp/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/tmp/libexec/ikigai", 1);
    setenv("IKIGAI_CACHE_DIR", "/tmp/cache", 1);
    setenv("IKIGAI_STATE_DIR", "/tmp/state", 1);
    setenv("IKIGAI_RUNTIME_DIR", "/run/user/1000", 1);

    res_t paths_result = ik_paths_init(test_ctx, &paths);
    ck_assert(!is_err(&paths_result));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Test: NULL registry
START_TEST(test_null_registry) {
    char *result = ik_tool_execute_from_registry(test_ctx, NULL, paths, "agent1", "test_tool", "{}", NULL);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}

END_TEST

// Test: Tool not found
START_TEST(test_tool_not_found) {
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "nonexistent", "{}", NULL);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}

END_TEST

// Test: Translation error when translating arguments
START_TEST(test_translate_args_error) {
    // Create a minimal schema for the test tool using talloc allocator
    char schema_json[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *schema = yyjson_read_opts(schema_json, strlen(schema_json), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema);

    // Register a test tool
    res_t add_res = ik_tool_registry_add(registry, "test_tool", "/tmp/test_tool.sh", schema);
    ck_assert(!is_err(&add_res));

    // Call with NULL paths to trigger translation error
    char *result = ik_tool_execute_from_registry(test_ctx, registry, NULL, "agent1", "test_tool", "{}", NULL);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_str_eq(yyjson_get_str(error_code), "translation_failed");
    yyjson_doc_free(doc);
}

END_TEST

// Test: Successful tool execution
START_TEST(test_successful_execution) {
    // Create a simple test tool that echoes stdin
    const char *script_path = "/tmp/test_executor_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\ncat\n");
    fclose(f);
    chmod(script_path, 0755);

    // Create a minimal schema for the test tool using talloc allocator
    char schema_json[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *schema = yyjson_read_opts(schema_json, strlen(schema_json), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema);

    // Register the test tool
    res_t add_res = ik_tool_registry_add(registry, "test_tool", script_path, schema);
    ck_assert(!is_err(&add_res));

    // Execute the tool
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "test_tool", "{\"test\":\"data\"}", NULL);
    ck_assert_ptr_nonnull(result);

    // Verify success
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(yyjson_get_bool(success));
    yyjson_doc_free(doc);

    unlink(script_path);
}

END_TEST

// Mock for tool execution failure or long result
static int mock_tool_exec_fail = 0;
static int mock_tool_long_result = 0;
res_t ik_tool_external_exec_(TALLOC_CTX *ctx, const char *tool_path, const char *agent_id, const char *arguments_json, pid_t *child_pid_out, char **out_result)
{
    (void)tool_path;
    (void)agent_id;
    (void)arguments_json;
    (void)child_pid_out;
    if (mock_tool_exec_fail) {
        return ERR(ctx, IO, "Tool execution failed");
    }
    if (mock_tool_long_result) {
        // Return valid JSON with >512 bytes to trigger the truncated DEBUG_LOG branch
        // Format: {"data":"AAAA...500 A chars..."}
        // This creates ~515 bytes of JSON output
        const size_t data_len = 500;
        const size_t json_len = 10 + data_len + 2; // {"data":"..."}
        char *long_json = talloc_array(ctx, char, (unsigned int)(json_len + 1));
        if (long_json == NULL) return ERR(ctx, IO, "OOM");
        snprintf(long_json, 10, "{\"data\":\"");
        memset(long_json + 9, 'A', data_len);
        long_json[9 + data_len] = '"';
        long_json[9 + data_len + 1] = '}';
        long_json[9 + data_len + 2] = '\0';
        *out_result = long_json;
        return OK(NULL);
    }
    return ik_tool_external_exec(ctx, tool_path, agent_id, arguments_json, child_pid_out, out_result);
}

// Test: Tool execution failure
START_TEST(test_tool_execution_failure) {
    char schema_json[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *schema = yyjson_read_opts(schema_json, strlen(schema_json), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema);

    res_t add_res = ik_tool_registry_add(registry, "test_tool", "/tmp/dummy.sh", schema);
    ck_assert(!is_err(&add_res));

    mock_tool_exec_fail = 1;
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "test_tool", "{}", NULL);
    mock_tool_exec_fail = 0;

    ck_assert_ptr_nonnull(result);
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_str_eq(yyjson_get_str(error_code), "execution_failed");
    yyjson_doc_free(doc);
}

END_TEST

// Mock for translation back failure
static int mock_translate_back_fail = 0;
res_t ik_paths_translate_path_to_ik_uri_(TALLOC_CTX *ctx, void *paths_arg, const char *input, char **out)
{
    if (mock_translate_back_fail) {
        return ERR(ctx, INVALID_ARG, "Translation back failed");
    }
    return ik_paths_translate_path_to_ik_uri(ctx, (ik_paths_t *)paths_arg, input, out);
}

// Test: Translation back failure
START_TEST(test_translate_back_failure) {
    // Create a test tool that outputs plain text
    const char *script_path = "/tmp/test_translate_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nprintf 'result'\n");
    fclose(f);
    chmod(script_path, 0755);

    char schema_json[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *schema = yyjson_read_opts(schema_json, strlen(schema_json), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema);

    res_t add_res = ik_tool_registry_add(registry, "test_tool", script_path, schema);
    ck_assert(!is_err(&add_res));

    mock_translate_back_fail = 1;
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "test_tool", "{}", NULL);
    mock_translate_back_fail = 0;

    ck_assert_ptr_nonnull(result);
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_str_eq(yyjson_get_str(error_code), "translation_failed");
    yyjson_doc_free(doc);

    unlink(script_path);
}

END_TEST

// Test: NULL arguments (covers the arguments ? arguments : "(null)" branch in DEBUG_LOG)
START_TEST(test_null_arguments) {
    const char *script_path = "/tmp/test_null_args_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\necho '{}'\n");
    fclose(f);
    chmod(script_path, 0755);

    char schema_json[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *schema = yyjson_read_opts(schema_json, strlen(schema_json), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema);

    res_t add_res = ik_tool_registry_add(registry, "test_tool", script_path, schema);
    ck_assert(!is_err(&add_res));

    // Pass NULL arguments — hits the "(null)" branch in the DEBUG_LOG ternary
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "test_tool", NULL, NULL);
    // Result is non-null regardless of success or error
    ck_assert_ptr_nonnull(result);

    unlink(script_path);
}

END_TEST

// Test: Long result (covers result_len > TOOL_RESULT_LOG_MAX branch)
START_TEST(test_long_result) {
    char schema_json[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *schema = yyjson_read_opts(schema_json, strlen(schema_json), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema);

    res_t add_res = ik_tool_registry_add(registry, "test_tool", "/tmp/dummy_long.sh", schema);
    ck_assert(!is_err(&add_res));

    // Use mock to return 600 chars, triggering the truncated DEBUG_LOG branch
    mock_tool_long_result = 1;
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "test_tool", "{}", NULL);
    mock_tool_long_result = 0;
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(yyjson_get_bool(success));
    yyjson_doc_free(doc);
}

END_TEST

static Suite *tool_executor_suite(void)
{
    Suite *s = suite_create("ToolExecutor");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_null_registry);
    tcase_add_test(tc_core, test_tool_not_found);
    tcase_add_test(tc_core, test_translate_args_error);
    tcase_add_test(tc_core, test_successful_execution);
    tcase_add_test(tc_core, test_tool_execution_failure);
    tcase_add_test(tc_core, test_translate_back_failure);
    tcase_add_test(tc_core, test_null_arguments);
    tcase_add_test(tc_core, test_long_result);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_executor_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/tool_executor_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
