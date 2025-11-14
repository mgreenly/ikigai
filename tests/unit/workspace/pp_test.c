/**
 * @file pp_test.c
 * @brief Unit tests for workspace pretty-print functionality
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../../src/format.h"
#include "../../test_utils.h"

/* Test: Pretty-print empty workspace */
START_TEST(test_pp_workspace_empty) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create empty workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify output contains workspace address and fields */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "ik_workspace_t @"));
    ck_assert_ptr_nonnull(strstr(output, "text_len: 0"));
    ck_assert_ptr_nonnull(strstr(output, "ik_cursor_t @"));
    ck_assert_ptr_nonnull(strstr(output, "byte_offset: 0"));
    ck_assert_ptr_nonnull(strstr(output, "grapheme_offset: 0"));
    ck_assert_ptr_nonnull(strstr(output, "target_column: 0"));

    talloc_free(ctx);
}
END_TEST
/* Test: Pretty-print workspace with single-line text */
START_TEST(test_pp_workspace_single_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace and add text */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    res = ik_workspace_insert_codepoint(workspace, 'H');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'i');
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify output */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "text_len: 2"));
    ck_assert_ptr_nonnull(strstr(output, "byte_offset: 2"));
    ck_assert_ptr_nonnull(strstr(output, "grapheme_offset: 2"));
    ck_assert_ptr_nonnull(strstr(output, "text: \"Hi\""));

    talloc_free(ctx);
}

END_TEST
/* Test: Pretty-print workspace with multi-line text */
START_TEST(test_pp_workspace_multiline)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace with multi-line text */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    res = ik_workspace_insert_codepoint(workspace, 'L');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, '1');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'L');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, '2');
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify output */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "text_len: 5"));
    ck_assert_ptr_nonnull(strstr(output, "byte_offset: 5"));
    /* Output should show escaped newline in text */
    ck_assert_ptr_nonnull(strstr(output, "L1\\nL2"));

    talloc_free(ctx);
}

END_TEST
/* Test: Pretty-print workspace with UTF-8 text */
START_TEST(test_pp_workspace_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace with UTF-8 emoji */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    res = ik_workspace_insert_codepoint(workspace, 0x1F600); // 😀
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify output - emoji is 4 bytes but 1 grapheme */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "text_len: 4"));
    ck_assert_ptr_nonnull(strstr(output, "byte_offset: 4"));
    ck_assert_ptr_nonnull(strstr(output, "grapheme_offset: 1"));

    talloc_free(ctx);
}

END_TEST
/* Test: Pretty-print workspace with indentation */
START_TEST(test_pp_workspace_indented)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print with 4-space indent */
    ik_pp_workspace(workspace, buf, 4);

    /* Verify output has proper indentation */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);

    /* Each line should start with 4 spaces */
    const char *line = output;
    while (line && *line) {
        /* Skip empty lines */
        if (*line == '\n') {
            line++;
            continue;
        }
        /* Check indentation */
        ck_assert_msg(line[0] == ' ' && line[1] == ' ' &&
                      line[2] == ' ' && line[3] == ' ',
                      "Line not properly indented: %.20s", line);
        /* Move to next line */
        line = strchr(line, '\n');
        if (line)line++;
    }

    talloc_free(ctx);
}

END_TEST
/* Test: Pretty-print workspace with cursor in middle */
START_TEST(test_pp_workspace_cursor_middle)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace with text and cursor in middle */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'b');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'c');
    ck_assert(is_ok(&res));

    /* Move cursor back to middle (after 'a') */
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify cursor position */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "byte_offset: 1"));
    ck_assert_ptr_nonnull(strstr(output, "grapheme_offset: 1"));
    ck_assert_ptr_nonnull(strstr(output, "text: \"abc\""));

    talloc_free(ctx);
}

END_TEST
/* Test: Pretty-print workspace with target_column set */
START_TEST(test_pp_workspace_target_column)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    /* Set target_column manually (simulating multi-line navigation) */
    workspace->target_column = 5;

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify target_column is shown */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "target_column: 5"));

    talloc_free(ctx);
}

END_TEST
/* Test: Pretty-print workspace with special characters */
START_TEST(test_pp_workspace_special_chars)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    ik_format_buffer_t *buf = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    /* Manually insert text with special characters (bypassing validation) */
    /* Test: \r (carriage return) */
    res = ik_byte_array_insert(workspace->text, 0, '\r');
    ck_assert(is_ok(&res));

    /* Test: \t (tab) */
    res = ik_byte_array_insert(workspace->text, 1, '\t');
    ck_assert(is_ok(&res));

    /* Test: \\ (backslash) */
    res = ik_byte_array_insert(workspace->text, 2, '\\');
    ck_assert(is_ok(&res));

    /* Test: \" (quote) */
    res = ik_byte_array_insert(workspace->text, 3, '"');
    ck_assert(is_ok(&res));

    /* Test: control character (0x01) */
    res = ik_byte_array_insert(workspace->text, 4, 0x01);
    ck_assert(is_ok(&res));

    /* Test: DEL (127) */
    res = ik_byte_array_insert(workspace->text, 5, 127);
    ck_assert(is_ok(&res));

    /* Create format buffer */
    res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    /* Pretty-print the workspace */
    ik_pp_workspace(workspace, buf, 0);

    /* Verify escaping */
    const char *output = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(output);
    ck_assert_ptr_nonnull(strstr(output, "\\r"));     /* Carriage return escaped */
    ck_assert_ptr_nonnull(strstr(output, "\\t"));     /* Tab escaped */
    ck_assert_ptr_nonnull(strstr(output, "\\\\"));    /* Backslash escaped */
    ck_assert_ptr_nonnull(strstr(output, "\\\""));    /* Quote escaped */
    ck_assert_ptr_nonnull(strstr(output, "\\x01"));   /* Control char as hex */
    ck_assert_ptr_nonnull(strstr(output, "\\x7f"));   /* DEL as hex */

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *pp_workspace_suite(void)
{
    Suite *s = suite_create("Workspace Pretty-Print");

    TCase *tc_basic = tcase_create("Basic");
    tcase_add_test(tc_basic, test_pp_workspace_empty);
    tcase_add_test(tc_basic, test_pp_workspace_single_line);
    tcase_add_test(tc_basic, test_pp_workspace_multiline);
    tcase_add_test(tc_basic, test_pp_workspace_utf8);
    tcase_add_test(tc_basic, test_pp_workspace_indented);
    tcase_add_test(tc_basic, test_pp_workspace_cursor_middle);
    tcase_add_test(tc_basic, test_pp_workspace_target_column);
    tcase_add_test(tc_basic, test_pp_workspace_special_chars);
    suite_add_tcase(s, tc_basic);

    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s = pp_workspace_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
