#include "tests/test_constants.h"
/**
 * @file command_fork_test.c
 * @brief Unit tests for command and fork event rendering
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/scrollback.h"

// Test: Render command event with echo and output
START_TEST(test_render_command_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *command_output = "  - item1\n  - item2";
    const char *data_json = "{\"command\":\"test\",\"echo\":\"/test\"}";
    res_t result = ik_event_render(scrollback, "command", command_output, data_json, false);
    ck_assert(!is_err(&result));

    // Should have: echo line + blank + output lines + blank = 4 lines
    ck_assert_uint_ge(ik_scrollback_get_line_count(scrollback), 3);

    const char *text;
    size_t length;

    // Line 0: echo (gray, with /test)
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "/test"));

    // Line 1: blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    // Line 2: output (subdued yellow, with item1)
    ik_scrollback_get_line_text(scrollback, 2, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "item1"));

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event - parent role
START_TEST(test_render_fork_event_parent) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *fork_message = "Forked child agent-uuid-123";
    res_t result = ik_event_render(scrollback, "fork", fork_message, "{\"role\":\"parent\"}", false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    // Fork message should include color codes (subdued gray)
    ck_assert_ptr_nonnull(strstr(text, "Forked child"));

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event - child role
START_TEST(test_render_fork_event_child) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *fork_message = "Forked from parent-uuid-456";
    res_t result = ik_event_render(scrollback, "fork", fork_message, "{\"role\":\"child\"}", false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    // Fork message should include color codes (subdued gray)
    ck_assert_ptr_nonnull(strstr(text, "Forked from"));

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with echo only (no output)
START_TEST(test_render_command_echo_only) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *data_json = "{\"command\":\"clear\",\"echo\":\"/clear\"}";
    res_t result = ik_event_render(scrollback, "command", NULL, data_json, false);
    ck_assert(!is_err(&result));

    // Should have: echo line + blank = 2 lines
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;

    // Line 0: echo (gray, with /clear)
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "/clear"));

    // Line 1: blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with NULL content
START_TEST(test_render_command_null_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "command", NULL, NULL, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with empty content
START_TEST(test_render_command_empty_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "command", "", NULL, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with whitespace-only content
START_TEST(test_render_command_whitespace_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *data_json = "{\"command\":\"test\",\"echo\":\"/test\"}";
    res_t result = ik_event_render(scrollback, "command", "   \n  \t  ", data_json, false);
    ck_assert(!is_err(&result));

    // Should have: echo line + blank = 2 lines (output is trimmed to empty)
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with empty echo string in JSON
START_TEST(test_render_command_empty_echo) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *data_json = "{\"command\":\"test\",\"echo\":\"\"}";
    const char *output = "output text";
    res_t result = ik_event_render(scrollback, "command", output, data_json, false);
    ck_assert(!is_err(&result));

    // Should have: output line + blank = 2 lines (echo is empty, skipped)
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "output text"));

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with non-string echo in JSON
START_TEST(test_render_command_nonstring_echo) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *data_json = "{\"command\":\"test\",\"echo\":123}";
    const char *output = "output text";
    res_t result = ik_event_render(scrollback, "command", output, data_json, false);
    ck_assert(!is_err(&result));

    // Should have: output line + blank = 2 lines (echo is not a string, skipped)
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event with NULL content
START_TEST(test_render_fork_null_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "fork", NULL, NULL, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event with empty content
START_TEST(test_render_fork_empty_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "fork", "", NULL, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST

static Suite *event_render_command_fork_suite(void)
{
    Suite *s = suite_create("Event Render Command/Fork");

    TCase *tc_render = tcase_create("Render");
    tcase_set_timeout(tc_render, IK_TEST_TIMEOUT);
    tcase_add_test(tc_render, test_render_command_event);
    tcase_add_test(tc_render, test_render_command_echo_only);
    tcase_add_test(tc_render, test_render_fork_event_parent);
    tcase_add_test(tc_render, test_render_fork_event_child);
    tcase_add_test(tc_render, test_render_command_null_content);
    tcase_add_test(tc_render, test_render_command_empty_content);
    tcase_add_test(tc_render, test_render_command_whitespace_content);
    tcase_add_test(tc_render, test_render_command_empty_echo);
    tcase_add_test(tc_render, test_render_command_nonstring_echo);
    tcase_add_test(tc_render, test_render_fork_null_content);
    tcase_add_test(tc_render, test_render_fork_empty_content);
    suite_add_tcase(s, tc_render);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = event_render_command_fork_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/event_render/command_fork_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
