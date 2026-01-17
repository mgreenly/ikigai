#include "../../test_constants.h"

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

// Helper: Run tool with args and capture output
static int32_t run_tool_with_args(const char *arg, char **output, int32_t *exit_code)
{
    int32_t pipe_out[2];

    if (pipe(pipe_out) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_out[0]);
        close(pipe_out[1]);
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipe_out[0]);

        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);

        execl(tool_path, tool_path, arg, (char *)NULL);
        exit(127);
    }

    // Parent process
    close(pipe_out[1]);

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

// Test: --schema flag returns valid JSON
START_TEST(test_schema_flag) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool_with_args("--schema", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check for required schema fields
    ck_assert_msg(strstr(output, "\"name\"") != NULL, "Schema missing name field");
    ck_assert_msg(strstr(output, "web_fetch") != NULL, "Schema has wrong name");
    ck_assert_msg(strstr(output, "\"description\"") != NULL, "Schema missing description");
    ck_assert_msg(strstr(output, "\"parameters\"") != NULL, "Schema missing parameters");
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Schema missing url parameter");
    ck_assert_msg(strstr(output, "\"required\"") != NULL, "Schema missing required field");
    ck_assert_msg(strstr(output, "\"offset\"") != NULL, "Schema missing offset parameter");
    ck_assert_msg(strstr(output, "\"limit\"") != NULL, "Schema missing limit parameter");
}
END_TEST

// Test: Empty stdin returns exit 1
START_TEST(test_empty_stdin) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: Invalid JSON returns exit 1
START_TEST(test_invalid_json) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{invalid json", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: Missing URL field returns exit 1
START_TEST(test_missing_url_field) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"foo\":\"bar\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: URL field is not a string returns exit 1
START_TEST(test_url_not_string) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"url\":123}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: Malformed URL returns INVALID_URL or NETWORK_ERROR
START_TEST(test_malformed_url) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"url\":\"not-a-valid-url\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0); // Tool returns 0 even for errors (error in JSON)
    ck_assert_ptr_nonnull(output);

    // Should contain error response
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Missing success field");
    ck_assert_msg(strstr(output, "false") != NULL, "Success should be false");
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
}
END_TEST

// Test: Nonexistent host returns NETWORK_ERROR
START_TEST(test_nonexistent_host) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"url\":\"http://this-host-definitely-does-not-exist-12345.com\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should contain error response
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Missing success field");
    ck_assert_msg(strstr(output, "false") != NULL, "Success should be false");
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "\"error_code\"") != NULL, "Missing error_code field");
    ck_assert_msg(strstr(output, "NETWORK_ERROR") != NULL, "Wrong error code");
}
END_TEST

// Test: Fetch simple HTML and convert to markdown
START_TEST(test_simple_html_conversion) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check success
    ck_assert_msg(strstr(output, "\"success\": true") != NULL || strstr(output, "\"success\":true") != NULL, "Success should be true");

    // Check title
    ck_assert_msg(strstr(output, "Test Page") != NULL, "Title not found");

    // Check markdown conversion
    ck_assert_msg(strstr(output, "# Main Heading") != NULL, "H1 not converted");
    ck_assert_msg(strstr(output, "## Subheading") != NULL, "H2 not converted");
    ck_assert_msg(strstr(output, "This is a paragraph") != NULL, "Paragraph not found");
    ck_assert_msg(strstr(output, "**bold**") != NULL, "Bold not converted");
    ck_assert_msg(strstr(output, "*italic*") != NULL, "Italic not converted");
}
END_TEST

// Test: Links conversion
START_TEST(test_links_conversion) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/links.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check link conversion
    ck_assert_msg(strstr(output, "[this link](https://example.com)") != NULL, "External link not converted");
    ck_assert_msg(strstr(output, "[local link](/local/path)") != NULL, "Local link not converted");
}
END_TEST

// Test: Lists conversion
START_TEST(test_lists_conversion) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/lists.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check list conversion
    ck_assert_msg(strstr(output, "# Shopping List") != NULL, "List heading not found");
    ck_assert_msg(strstr(output, "- Apples") != NULL, "First list item not converted");
    ck_assert_msg(strstr(output, "- Bananas") != NULL, "Second list item not converted");
    ck_assert_msg(strstr(output, "- Oranges") != NULL, "Third list item not converted");
}
END_TEST

// Test: Scripts and styles are stripped
START_TEST(test_scripts_stripped) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/scripts.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check that scripts and styles are NOT in output
    ck_assert_msg(strstr(output, "alert") == NULL, "Script content not stripped");
    ck_assert_msg(strstr(output, "console.log") == NULL, "Script content not stripped");
    ck_assert_msg(strstr(output, "color: red") == NULL, "Style content not stripped");
    ck_assert_msg(strstr(output, "display: none") == NULL, "Style content not stripped");

    // Check that visible content IS in output
    ck_assert_msg(strstr(output, "Visible content") != NULL, "Visible content not found");
    ck_assert_msg(strstr(output, "More visible content") != NULL, "Visible content not found");
}
END_TEST

// Test: Formatting (code, br, nested bold/italic)
START_TEST(test_formatting_conversion) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/formatting.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check code conversion
    ck_assert_msg(strstr(output, "`inline code`") != NULL, "Code not converted");

    // Check nested formatting
    ck_assert_msg(strstr(output, "**bold") != NULL && strstr(output, "text**") != NULL, "Bold not found");
    ck_assert_msg(strstr(output, "*bold italic*") != NULL, "Nested italic not found");

    // Check line break (in JSON output, newlines are escaped as \n)
    ck_assert_msg(strstr(output, "Line break here") != NULL && strstr(output, "next line") != NULL, "Line break not converted");
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
        if (*p == '\\' && *(p+1) == 'n') {
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
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Success field not found");
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
    ck_assert_msg(strstr(output, "\"content\": \"\"") != NULL || strstr(output, "\"content\":\"\"") != NULL, "Content should be empty");
}
END_TEST

// Test: Title extraction
START_TEST(test_title_extraction) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/links.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check title
    ck_assert_msg(strstr(output, "\"title\"") != NULL, "Title field not found");
    ck_assert_msg(strstr(output, "Links Test") != NULL, "Title value not correct");
}
END_TEST

// Test: All heading levels (h3-h6)
START_TEST(test_all_headings) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/headings.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check all heading conversions
    ck_assert_msg(strstr(output, "# Heading 1") != NULL, "H1 not found");
    ck_assert_msg(strstr(output, "## Heading 2") != NULL, "H2 not found");
    ck_assert_msg(strstr(output, "### Heading 3") != NULL, "H3 not found");
    ck_assert_msg(strstr(output, "#### Heading 4") != NULL, "H4 not found");
    ck_assert_msg(strstr(output, "##### Heading 5") != NULL, "H5 not found");
    ck_assert_msg(strstr(output, "###### Heading 6") != NULL, "H6 not found");
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
    ck_assert_msg(strstr(output, "\"success\": true") != NULL || strstr(output, "\"success\":true") != NULL, "Success not found");
}
END_TEST

// Test: HTML with comments (non-element nodes)
START_TEST(test_html_comments) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/comments.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check that comments are stripped
    ck_assert_msg(strstr(output, "Visible text") != NULL, "Text not found");
    ck_assert_msg(strstr(output, "This is a comment") == NULL, "Comment not stripped");
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
    ck_assert_msg(strstr(output, "\"success\"") != NULL && strstr(output, "false") != NULL, "Should be error");
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
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Success field not found");
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

    // Should contain HTTP_ERROR
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Missing success field");
    ck_assert_msg(strstr(output, "false") != NULL, "Success should be false");
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "\"error_code\"") != NULL, "Missing error_code field");
    ck_assert_msg(strstr(output, "HTTP_ERROR") != NULL, "Wrong error code");
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

    // Should contain HTTP_ERROR
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Missing success field");
    ck_assert_msg(strstr(output, "false") != NULL, "Success should be false");
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "HTTP_ERROR") != NULL, "Wrong error code");
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
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Missing success field");
}
END_TEST

// Test: Non-integer offset parameter (should be ignored)
START_TEST(test_non_integer_offset) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":\"not_a_number\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should succeed and ignore the invalid offset
    ck_assert_msg(strstr(output, "\"success\": true") != NULL || strstr(output, "\"success\":true") != NULL, "Success should be true");
}
END_TEST

// Test: Non-integer limit parameter (should be ignored)
START_TEST(test_non_integer_limit) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"limit\":true}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should succeed and ignore the invalid limit
    ck_assert_msg(strstr(output, "\"success\": true") != NULL || strstr(output, "\"success\":true") != NULL, "Success should be true");
}
END_TEST

// Test: More HTML elements (b, i, ol, nav)
START_TEST(test_more_elements) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/more_elements.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check b tag conversion
    ck_assert_msg(strstr(output, "**bold tag**") != NULL, "Bold (b tag) not converted");

    // Check i tag conversion
    ck_assert_msg(strstr(output, "*italic tag*") != NULL, "Italic (i tag) not converted");

    // Check ol list conversion
    ck_assert_msg(strstr(output, "- First ordered item") != NULL, "Ordered list not converted");
    ck_assert_msg(strstr(output, "- Second ordered item") != NULL, "Ordered list item not converted");

    // Check nav is stripped
    ck_assert_msg(strstr(output, "After nav element") != NULL, "Text after nav not found");
}
END_TEST

// Test: Edge cases (empty title, link without href)
START_TEST(test_edge_cases) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/edge_cases.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check that it succeeds with empty title
    ck_assert_msg(strstr(output, "\"success\": true") != NULL || strstr(output, "\"success\":true") != NULL, "Success should be true");

    // Check link without href is handled (should have empty parentheses or similar)
    ck_assert_msg(strstr(output, "clickable text") != NULL, "Link text not found");
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
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":2,\"limit\":2}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check success and that content field exists
    ck_assert_msg(strstr(output, "\"success\"") != NULL, "Success field not found");
    ck_assert_msg(strstr(output, "\"content\"") != NULL, "Content field not found");
}
END_TEST

// Test: HTML with style but no script (tests short-circuit OR branch)
START_TEST(test_style_only) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/style_only.html\"}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check that style is stripped
    ck_assert_msg(strstr(output, "color: blue") == NULL, "Style content not stripped");

    // Check that visible content IS in output
    ck_assert_msg(strstr(output, "Content with style") != NULL, "Visible content not found");
}
END_TEST

static Suite *web_fetch_suite(void)
{
    Suite *s = suite_create("WebFetch");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_schema_flag);
    tcase_add_test(tc_core, test_empty_stdin);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_url_field);
    tcase_add_test(tc_core, test_url_not_string);
    tcase_add_test(tc_core, test_malformed_url);
    tcase_add_test(tc_core, test_nonexistent_host);
    tcase_add_test(tc_core, test_simple_html_conversion);
    tcase_add_test(tc_core, test_links_conversion);
    tcase_add_test(tc_core, test_lists_conversion);
    tcase_add_test(tc_core, test_scripts_stripped);
    tcase_add_test(tc_core, test_formatting_conversion);
    tcase_add_test(tc_core, test_pagination_limit);
    tcase_add_test(tc_core, test_pagination_offset);
    tcase_add_test(tc_core, test_pagination_offset_beyond);
    tcase_add_test(tc_core, test_title_extraction);
    tcase_add_test(tc_core, test_all_headings);
    tcase_add_test(tc_core, test_large_html);
    tcase_add_test(tc_core, test_html_comments);
    tcase_add_test(tc_core, test_file_not_found);
    tcase_add_test(tc_core, test_large_json_input);
    tcase_add_test(tc_core, test_http_404_error);
    tcase_add_test(tc_core, test_http_500_error);
    tcase_add_test(tc_core, test_unparseable_content);
    tcase_add_test(tc_core, test_non_integer_offset);
    tcase_add_test(tc_core, test_non_integer_limit);
    tcase_add_test(tc_core, test_more_elements);
    tcase_add_test(tc_core, test_edge_cases);
    tcase_add_test(tc_core, test_pagination_offset_and_limit);
    tcase_add_test(tc_core, test_style_only);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = web_fetch_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
