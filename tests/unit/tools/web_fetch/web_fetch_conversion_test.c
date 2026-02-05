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
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");

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
    ck_assert_msg(strstr(output, "Line break here") != NULL && strstr(output, "next line") != NULL,
                  "Line break not converted");
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
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");

    // Check link without href is handled (should have empty parentheses or similar)
    ck_assert_msg(strstr(output, "clickable text") != NULL, "Link text not found");
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

static Suite *web_fetch_conversion_suite(void)
{
    Suite *s = suite_create("WebFetchConversion");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_simple_html_conversion);
    tcase_add_test(tc_core, test_links_conversion);
    tcase_add_test(tc_core, test_lists_conversion);
    tcase_add_test(tc_core, test_scripts_stripped);
    tcase_add_test(tc_core, test_formatting_conversion);
    tcase_add_test(tc_core, test_title_extraction);
    tcase_add_test(tc_core, test_all_headings);
    tcase_add_test(tc_core, test_html_comments);
    tcase_add_test(tc_core, test_more_elements);
    tcase_add_test(tc_core, test_edge_cases);
    tcase_add_test(tc_core, test_style_only);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = web_fetch_conversion_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_fetch/web_fetch_conversion_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
