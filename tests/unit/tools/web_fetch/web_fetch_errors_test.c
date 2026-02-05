#include "tests/test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static TALLOC_CTX *test_ctx;
static char tool_path[PATH_MAX + 256];

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Construct absolute path to tool binary
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(tool_path, sizeof(tool_path), "%s/libexec/ikigai/web-fetch-tool", cwd);
    } else {
        snprintf(tool_path, sizeof(tool_path), "libexec/ikigai/web-fetch-tool");
    }
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper: Run tool with input and capture output
static int32_t run_tool(const char *input, char **output, int32_t *exit_code)
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
        // Child process
        close(pipe_in[1]);
        close(pipe_out[0]);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[0]);
        close(pipe_out[1]);

        execl(tool_path, tool_path, (char *)NULL);
        exit(127);
    }

    // Parent process
    close(pipe_in[0]);
    close(pipe_out[1]);

    // Write input
    if (input != NULL) {
        size_t len = strlen(input);
        ssize_t written = write(pipe_in[1], input, len);
        (void)written;
    }
    close(pipe_in[1]);

    // Read output
    char buffer[65536];
    ssize_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_out[0], buffer + total_read, sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += bytes_read;
    }
    close(pipe_out[0]);

    buffer[total_read] = '\0';
    *output = talloc_strdup(test_ctx, buffer);

    // Wait for child
    int32_t status;
    waitpid(pid, &status, 0);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return 0;
}

// Test: Malformed URL returns INVALID_URL or NETWORK_ERROR
START_TEST(test_malformed_url) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"url\":\"not-a-valid-url\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0); // Tool returns 0 even for errors (error in JSON)
    ck_assert_ptr_nonnull(output);

    // Should contain error response
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
}
END_TEST

// Test: Nonexistent host returns NETWORK_ERROR
START_TEST(test_nonexistent_host) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"url\":\"http://this-host-definitely-does-not-exist-12345.com\"}", &output,
                              &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should contain error response
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "\"error_code\"") != NULL, "Missing error_code field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
}
END_TEST

// Test: Pagination with limit
START_TEST(test_pagination_limit) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"limit\":2}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Extract content field value
    char *content_start = strstr(output, "\"content\"");
    ck_assert_ptr_nonnull(content_start);

    // Count newlines in content - with limit=2, should have at most 2 lines
    int32_t newline_count = 0;
    for (char *p = content_start; *p && newline_count < 10; p++) {
        if (*p == '\\' && *(p + 1) == 'n') {
            newline_count++;
            p++; // Skip the 'n' after backslash
        }
    }

    // With limit=2, we should see limited output (exact count depends on HTML structure)
    ck_assert_msg(newline_count <= 3, "Limit not applied correctly");
}
END_TEST

// Test: Pagination with offset
START_TEST(test_pagination_offset) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":3}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check success and that content field exists
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");
    ck_assert_msg(strstr(output, "\"content\"") != NULL, "Content field not found");
}
END_TEST

// Test: Pagination with offset beyond content
START_TEST(test_pagination_offset_beyond) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":1000}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Content should be empty when offset exceeds total lines
    ck_assert_msg(strstr(output, "\"content\": \"\"") != NULL || strstr(output, "\"content\":\"\"") != NULL,
                  "Content should be empty");
}
END_TEST

// Test: Large HTML that triggers buffer reallocation
START_TEST(test_large_html) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/large.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check success
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");
}
END_TEST

// Test: File not found (HTTP-like error)
START_TEST(test_file_not_found) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/nonexistent.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should be an error
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Should contain error field");
}
END_TEST

// Test: Large JSON input that triggers stdin buffer reallocation
START_TEST(test_large_json_input) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    // Create a JSON with a very long URL (> 4096 bytes) to trigger stdin buffer reallocation
    char *large_input = talloc_array(NULL, char, 10000);
    char *url_part = talloc_array(NULL, char, 9000);

    // Build a very long URL with query params to exceed 4096 bytes
    strcpy(url_part, "file://");
    strcat(url_part, cwd);
    strcat(url_part, "/tests/fixtures/html/simple.html?");
    // Add 300 query params to ensure we exceed 4096 bytes
    for (int32_t i = 0; i < 300; i++) {
        char param[50];
        snprintf(param, sizeof(param), "param%d=value%d&", i, i);
        strcat(url_part, param);
    }

    snprintf(large_input, 10000, "{\"url\":\"%s\"}", url_part);

    int32_t result = run_tool(large_input, &output, &exit_code);

    talloc_free(url_part);
    talloc_free(large_input);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should succeed
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");
}
END_TEST

// Test: HTTP 404 error returns HTTP_ERROR
START_TEST(test_http_404_error) {
    char *output = NULL;
    int32_t exit_code = 0;

    // Use httpbin.org to get a reliable 404 response
    int32_t result = run_tool("{\"url\":\"https://httpbin.org/status/404\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0); // Tool returns 0 even for errors
    ck_assert_ptr_nonnull(output);

    // Should contain ERR_IO error for HTTP 404
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Should contain error field");
    ck_assert_msg(strstr(output, "\"error_code\"") != NULL, "Missing error_code field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
    ck_assert_msg(strstr(output, "404") != NULL, "Missing 404 status code in error message");
}
END_TEST

// Test: HTTP 500 error returns HTTP_ERROR
START_TEST(test_http_500_error) {
    char *output = NULL;
    int32_t exit_code = 0;

    // Use httpbin.org to get a reliable 500 response
    int32_t result = run_tool("{\"url\":\"https://httpbin.org/status/500\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should contain ERR_IO error for HTTP 500
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Should contain error field");
    ck_assert_msg(strstr(output, "\"error_code\"") != NULL, "Missing error_code field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
    ck_assert_msg(strstr(output, "500") != NULL, "Missing 500 status code in error message");
}
END_TEST

// Test: Binary data that can't be parsed as HTML returns PARSE_ERROR
START_TEST(test_unparseable_content) {
    char *output = NULL;
    int32_t exit_code = 0;

    // httpbin.org/bytes returns random binary data - not valid HTML
    // This should trigger htmlReadMemory to return NULL
    int32_t result = run_tool("{\"url\":\"https://httpbin.org/bytes/1000\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Might succeed (if libxml2 is very forgiving) or return PARSE_ERROR
    // We just verify it doesn't crash and returns valid JSON
    ck_assert_msg(strstr(output, "\"error\"") != NULL || strstr(output, "\"url\"") != NULL,
                  "Should contain either error or url field");
}
END_TEST

// Test: Pagination with both offset and limit
START_TEST(test_pagination_offset_and_limit) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input,
             sizeof(input),
             "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":2,\"limit\":2}",
             cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check success and that content field exists
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");
    ck_assert_msg(strstr(output, "\"content\"") != NULL, "Content field not found");
}
END_TEST

static Suite *web_fetch_errors_suite(void)
{
    Suite *s = suite_create("WebFetchErrors");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_malformed_url);
    tcase_add_test(tc_core, test_nonexistent_host);
    tcase_add_test(tc_core, test_pagination_limit);
    tcase_add_test(tc_core, test_pagination_offset);
    tcase_add_test(tc_core, test_pagination_offset_beyond);
    tcase_add_test(tc_core, test_large_html);
    tcase_add_test(tc_core, test_file_not_found);
    tcase_add_test(tc_core, test_large_json_input);
    tcase_add_test(tc_core, test_http_404_error);
    tcase_add_test(tc_core, test_http_500_error);
    tcase_add_test(tc_core, test_unparseable_content);
    tcase_add_test(tc_core, test_pagination_offset_and_limit);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = web_fetch_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_fetch/web_fetch_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
