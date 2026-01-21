#include "../../test_constants.h"
#include "../../../src/tool_external.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Test: Execute simple echo tool
START_TEST(test_execute_echo_tool) {
    // Create a simple shell script that echoes stdin
    const char *script_path = "/tmp/test_echo_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\ncat\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{\"test\":\"value\"}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "{\"test\":\"value\"}");

    unlink(script_path);
}

END_TEST

// Test: Execute tool with output
START_TEST(test_execute_tool_with_output) {
    // Create a script that produces JSON output regardless of input
    const char *script_path = "/tmp/test_output_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nprintf '{\"output\":\"hello\"}'\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "{\"output\":\"hello\"}");

    unlink(script_path);
}

END_TEST

// Test: Tool exits with non-zero status
START_TEST(test_tool_nonzero_exit) {
    // Create a script that exits with error
    const char *script_path = "/tmp/test_error_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nexit 1\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_uint_eq(res.err->code, ERR_IO);

    unlink(script_path);
}

END_TEST

// Test: Tool produces no output
START_TEST(test_tool_no_output) {
    // Create a script that produces no output
    const char *script_path = "/tmp/test_silent_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\n# Silent\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_uint_eq(res.err->code, ERR_IO);

    unlink(script_path);
}

END_TEST

// Test: Tool not found
START_TEST(test_tool_not_found) {
    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, "/nonexistent/tool", input_json, &result);

    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_uint_eq(res.err->code, ERR_IO);
}

END_TEST

// Test: Tool with multiline output
START_TEST(test_tool_multiline_output) {
    // Create a script that produces multiline output
    const char *script_path = "/tmp/test_multiline_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nprintf 'line1\\nline2\\nline3'\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "line1\nline2\nline3");

    unlink(script_path);
}

END_TEST

// Test: Tool reads stdin correctly
START_TEST(test_tool_reads_stdin) {
    // Create a script that processes stdin
    const char *script_path = "/tmp/test_stdin_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nread line\nprintf \"Got: $line\"\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{\"key\":\"value\"}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Got: {\"key\":\"value\"}");

    unlink(script_path);
}

END_TEST

// Test: Large output handling
START_TEST(test_tool_large_output) {
    // Create a script that produces large output (but within buffer)
    const char *script_path = "/tmp/test_large_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nfor i in $(seq 1 100); do echo '{\"line\":'$i'}'; done\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert(strlen(result) > 100);

    unlink(script_path);
}

END_TEST

// Test: Very large output (buffer overflow case - 70KB output)
START_TEST(test_tool_very_large_output) {
    // Create a script that produces > 65535 bytes (exceeds buffer size)
    const char *script_path = "/tmp/test_overflow_tool.sh";
    FILE *f = fopen(script_path, "w");
    ck_assert_ptr_nonnull(f);
    // Use dd to generate exactly 70000 bytes, ignoring SIGPIPE
    fprintf(f, "#!/bin/sh\ndd if=/dev/zero bs=70000 count=1 2>/dev/null | tr '\\0' 'x'; exit 0\n");
    fclose(f);
    chmod(script_path, 0755);

    char *result = NULL;
    const char *input_json = "{}";
    res_t res = ik_tool_external_exec(test_ctx, script_path, input_json, &result);

    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(result);
    // Output should be truncated at buffer size - 1 (65535 chars + null terminator)
    ck_assert_uint_eq(strlen(result), 65535);

    unlink(script_path);
}

END_TEST

static Suite *tool_external_suite(void)
{
    Suite *s = suite_create("ToolExternal");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_execute_echo_tool);
    tcase_add_test(tc_core, test_execute_tool_with_output);
    tcase_add_test(tc_core, test_tool_nonzero_exit);
    tcase_add_test(tc_core, test_tool_no_output);
    tcase_add_test(tc_core, test_tool_not_found);
    tcase_add_test(tc_core, test_tool_multiline_output);
    tcase_add_test(tc_core, test_tool_reads_stdin);
    tcase_add_test(tc_core, test_tool_large_output);
    tcase_add_test(tc_core, test_tool_very_large_output);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_external_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/tool_external_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
