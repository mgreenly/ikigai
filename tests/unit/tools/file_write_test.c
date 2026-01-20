/**
 * @file file_write_test.c
 * @brief Unit tests for file_write tool
 */
#include "../../test_constants.h"

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static const char *tool_path = "libexec/ikigai/file-write-tool";

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper to run tool with optional --schema flag and get exit code + output
static int32_t run_tool_with_args(const char *arg1, const char *input, char *output, size_t output_size)
{
    int32_t pipe_in[2];
    int32_t pipe_out[2];

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]);
        close(pipe_out[1]);
        if (arg1 != NULL) {
            execl(tool_path, tool_path, arg1, (char *)NULL);
        } else {
            execl(tool_path, tool_path, (char *)NULL);
        }
        exit(127);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    if (input != NULL) {
        size_t len = strlen(input);
        ssize_t written = write(pipe_in[1], input, len);
        (void)written;
    }
    close(pipe_in[1]);

    if (output != NULL && output_size > 0) {
        size_t total = 0;
        ssize_t n;
        while ((n = read(pipe_out[0], output + total, output_size - total - 1)) > 0) {
            total += (size_t)n;
            if (total >= output_size - 1) {
                break;
            }
        }
        output[total] = '\0';
    }
    close(pipe_out[0]);

    int32_t status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Helper to run tool and get exit code + output
static int32_t run_tool(const char *input, char *output, size_t output_size)
{
    return run_tool_with_args(NULL, input, output, output_size);
}

// Helper to run tool and get exit code only
static int32_t run_tool_basic(const char *input)
{
    return run_tool(input, NULL, 0);
}

START_TEST(test_schema_output) {
    char output[4096] = {0};
    int32_t exit_code = run_tool_with_args("--schema", NULL, output, sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert_str_eq(output,
                     "{\n  \"name\": \"file_write\",\n  \"description\": \"Write content to a file (creates or overwrites)\",\n  \"parameters\": {\n    \"type\": \"object\",\n    \"properties\": {\n      \"file_path\": {\n        \"type\": \"string\",\n        \"description\": \"Absolute or relative path to file\"\n      },\n      \"content\": {\n        \"type\": \"string\",\n        \"description\": \"Content to write to file\"\n      }\n    },\n    \"required\": [\"file_path\", \"content\"]\n  }\n}\n");
}
END_TEST

START_TEST(test_empty_input) {
    int32_t exit_code = run_tool_basic("");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_json) {
    int32_t exit_code = run_tool_basic("{invalid json}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_missing_file_path) {
    int32_t exit_code = run_tool_basic("{\"content\":\"test\"}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_file_path) {
    int32_t exit_code = run_tool_basic("{\"file_path\":123,\"content\":\"test\"}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_missing_content) {
    int32_t exit_code = run_tool_basic("{\"file_path\":\"/tmp/test\"}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_content) {
    int32_t exit_code = run_tool_basic("{\"file_path\":\"/tmp/test\",\"content\":123}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_simple_write) {
    char output[4096] = {0};
    const char *test_file = "/tmp/file_write_test_simple.txt";

    // Clean up any previous test file
    unlink(test_file);

    int32_t exit_code = run_tool("{\"file_path\":\"/tmp/file_write_test_simple.txt\",\"content\":\"Hello, World!\"}",
                                 output,
                                 sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert_str_eq(output, "{\"output\":\"Wrote 13 bytes to file_write_test_simple.txt\",\"bytes\":13}\n");

    // Verify file contents
    FILE *fp = fopen(test_file, "r");
    ck_assert_ptr_nonnull(fp);
    char content[128] = {0};
    size_t read = fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);
    content[read] = '\0';
    ck_assert_str_eq(content, "Hello, World!");

    // Clean up
    unlink(test_file);
}
END_TEST

START_TEST(test_write_special_characters) {
    char output[4096] = {0};
    const char *test_file = "/tmp/file_write_test_special.txt";

    // Clean up any previous test file
    unlink(test_file);

    int32_t exit_code =
        run_tool(
            "{\"file_path\":\"/tmp/file_write_test_special.txt\",\"content\":\"Line 1\\nLine 2\\tTabbed\\r\\nCRLF\"}",
            output,
            sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output,
                     "\"bytes\":26") != NULL);

    // Verify file contents
    FILE *fp = fopen(test_file, "r");
    ck_assert_ptr_nonnull(fp);
    char content[128] = {0};
    size_t read = fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);
    content[read] = '\0';
    ck_assert_str_eq(content, "Line 1\nLine 2\tTabbed\r\nCRLF");

    // Clean up
    unlink(test_file);
}
END_TEST

START_TEST(test_write_empty_content) {
    char output[4096] = {0};
    const char *test_file = "/tmp/file_write_test_empty.txt";

    // Clean up any previous test file
    unlink(test_file);

    int32_t exit_code = run_tool("{\"file_path\":\"/tmp/file_write_test_empty.txt\",\"content\":\"\"}", output,
                                 sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert_str_eq(output, "{\"output\":\"Wrote 0 bytes to file_write_test_empty.txt\",\"bytes\":0}\n");

    // Verify file exists and is empty
    FILE *fp = fopen(test_file, "r");
    ck_assert_ptr_nonnull(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    ck_assert_int_eq(size, 0);

    // Clean up
    unlink(test_file);
}
END_TEST

START_TEST(test_overwrite_existing_file) {
    char output[4096] = {0};
    const char *test_file = "/tmp/file_write_test_overwrite.txt";

    // Create initial file
    FILE *fp = fopen(test_file, "w");
    ck_assert_ptr_nonnull(fp);
    fprintf(fp, "Original content that should be overwritten");
    fclose(fp);

    // Overwrite with new content
    int32_t exit_code = run_tool("{\"file_path\":\"/tmp/file_write_test_overwrite.txt\",\"content\":\"New content\"}",
                                 output,
                                 sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert_str_eq(output, "{\"output\":\"Wrote 11 bytes to file_write_test_overwrite.txt\",\"bytes\":11}\n");

    // Verify new content
    fp = fopen(test_file, "r");
    ck_assert_ptr_nonnull(fp);
    char content[128] = {0};
    size_t read = fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);
    content[read] = '\0';
    ck_assert_str_eq(content, "New content");

    // Clean up
    unlink(test_file);
}
END_TEST

START_TEST(test_large_content) {
    char output[4096] = {0};
    const char *test_file = "/tmp/file_write_test_large.txt";

    // Clean up any previous test file
    unlink(test_file);

    // Create large content (10KB)
    char *large_content = talloc_array(test_ctx, char, 10240 + 100);
    memset(large_content, 'A', 10240);
    large_content[10240] = '\0';

    char *input = talloc_asprintf(test_ctx,
                                  "{\"file_path\":\"/tmp/file_write_test_large.txt\",\"content\":\"%s\"}",
                                  large_content);

    int32_t exit_code = run_tool(input, output, sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"bytes\":10240") != NULL);

    // Verify file size
    FILE *fp = fopen(test_file, "r");
    ck_assert_ptr_nonnull(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    ck_assert_int_eq(size, 10240);

    // Clean up
    unlink(test_file);
}
END_TEST

START_TEST(test_nested_directory) {
    char output[4096] = {0};
    const char *test_dir = "/tmp/file_write_test_nested_dir";
    const char *test_file = "/tmp/file_write_test_nested_dir/nested.txt";

    // Create directory
    mkdir(test_dir, 0755);

    int32_t exit_code =
        run_tool("{\"file_path\":\"/tmp/file_write_test_nested_dir/nested.txt\",\"content\":\"Nested content\"}",
                 output,
                 sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert_str_eq(output, "{\"output\":\"Wrote 14 bytes to nested.txt\",\"bytes\":14}\n");

    // Verify file contents
    FILE *fp = fopen(test_file, "r");
    ck_assert_ptr_nonnull(fp);
    char content[128] = {0};
    size_t read = fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);
    content[read] = '\0';
    ck_assert_str_eq(content, "Nested content");

    // Clean up
    unlink(test_file);
    rmdir(test_dir);
}
END_TEST

START_TEST(test_permission_denied) {
    char output[4096] = {0};
    const char *test_dir = "/tmp/file_write_test_readonly_dir";
    const char *test_file = "/tmp/file_write_test_readonly_dir/readonly.txt";

    // Clean up any previous test
    unlink(test_file);
    rmdir(test_dir);

    // Create directory with no write permissions
    mkdir(test_dir, 0555);

    int32_t exit_code =
        run_tool("{\"file_path\":\"/tmp/file_write_test_readonly_dir/readonly.txt\",\"content\":\"Should fail\"}",
                 output,
                 sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"error_code\":\"PERMISSION_DENIED\"") != NULL);
    ck_assert(strstr(output, "Permission denied") != NULL);

    // Clean up (restore write permission first)
    chmod(test_dir, 0755);
    rmdir(test_dir);
}
END_TEST

START_TEST(test_nonexistent_directory) {
    char output[4096] = {0};

    int32_t exit_code =
        run_tool("{\"file_path\":\"/nonexistent/path/that/does/not/exist/file.txt\",\"content\":\"Should fail\"}",
                 output,
                 sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"error_code\":\"OPEN_FAILED\"") != NULL);
    ck_assert(strstr(output, "Cannot open file") != NULL);
}
END_TEST

START_TEST(test_buffer_growth) {
    char output[8192] = {0};
    const char *test_file = "/tmp/file_write_test_buffer_growth.txt";

    // Clean up any previous test file
    unlink(test_file);

    // Create input that exceeds 4KB buffer (needs buffer growth during stdin read)
    char *large_content = talloc_array(test_ctx, char, 5000);
    memset(large_content, 'B', 4999);
    large_content[4999] = '\0';

    char *input = talloc_asprintf(test_ctx,
                                  "{\"file_path\":\"/tmp/file_write_test_buffer_growth.txt\",\"content\":\"%s\"}",
                                  large_content);

    int32_t exit_code = run_tool(input, output, sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"bytes\":4999") != NULL);

    // Clean up
    unlink(test_file);
}
END_TEST

START_TEST(test_exact_buffer_boundary) {
    char output[4096] = {0};
    const char *test_file = "/tmp/file_write_test_exact_boundary.txt";

    // Clean up any previous test file
    unlink(test_file);

    // Create content that's exactly 4096 bytes to hit buffer boundary
    // The JSON overhead reduces the available space for content
    // JSON format: {"file_path":"...","content":"..."}
    // Overhead: ~60 bytes, so content should be ~4036 bytes to make total ~4096
    char *exact_content = talloc_array(test_ctx, char, 4037);
    memset(exact_content, 'B', 4036);
    exact_content[4036] = '\0';

    char *input = talloc_asprintf(test_ctx,
                                  "{\"file_path\":\"/tmp/file_write_test_exact_boundary.txt\",\"content\":\"%s\"}",
                                  exact_content);

    int32_t exit_code = run_tool(input, output, sizeof(output));

    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"bytes\":4036") != NULL);

    // Clean up
    unlink(test_file);
}
END_TEST

static Suite *file_write_tool_suite(void)
{
    Suite *s = suite_create("FileWriteTool");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_schema_output);
    tcase_add_test(tc_core, test_empty_input);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_file_path);
    tcase_add_test(tc_core, test_invalid_file_path);
    tcase_add_test(tc_core, test_missing_content);
    tcase_add_test(tc_core, test_invalid_content);
    tcase_add_test(tc_core, test_simple_write);
    tcase_add_test(tc_core, test_write_special_characters);
    tcase_add_test(tc_core, test_write_empty_content);
    tcase_add_test(tc_core, test_overwrite_existing_file);
    tcase_add_test(tc_core, test_large_content);
    tcase_add_test(tc_core, test_nested_directory);
    tcase_add_test(tc_core, test_permission_denied);
    tcase_add_test(tc_core, test_nonexistent_directory);
    tcase_add_test(tc_core, test_buffer_growth);
    tcase_add_test(tc_core, test_exact_buffer_boundary);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = file_write_tool_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
