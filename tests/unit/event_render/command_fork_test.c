/**
 * @file command_fork_test.c
 * @brief Unit tests for command and fork event rendering
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/event_render.h"
#include "../../../src/scrollback.h"

// Test: ik_event_renders_visible - command events are visible
START_TEST(test_renders_visible_command) {
    ck_assert(ik_event_renders_visible("command"));
}
END_TEST
// Test: ik_event_renders_visible - fork events are visible
START_TEST(test_renders_visible_fork) {
    ck_assert(ik_event_renders_visible("fork"));
}

END_TEST
// Test: Render command event
START_TEST(test_render_command_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *command_output = "$ ls -la\ntotal 42\ndrwxr-xr-x 2 user user 4096 Jan 1 12:00 .";
    res_t result = ik_event_render(scrollback, "command", command_output, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_ge(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    // Command output should include color codes (subdued gray)
    ck_assert_ptr_nonnull(strstr(text, "$ ls -la"));

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event - parent role
START_TEST(test_render_fork_event_parent) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *fork_message = "Forked child agent-uuid-123";
    res_t result = ik_event_render(scrollback, "fork", fork_message, "{\"role\":\"parent\"}");
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
    res_t result = ik_event_render(scrollback, "fork", fork_message, "{\"role\":\"child\"}");
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
// Test: Render command event with NULL content
START_TEST(test_render_command_null_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "command", NULL, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render command event with empty content
START_TEST(test_render_command_empty_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "command", "", NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event with NULL content
START_TEST(test_render_fork_null_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "fork", NULL, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render fork event with empty content
START_TEST(test_render_fork_empty_content) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "fork", "", NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST

static Suite *event_render_command_fork_suite(void)
{
    Suite *s = suite_create("Event Render Command/Fork");

    TCase *tc_visible = tcase_create("Visibility");
    tcase_set_timeout(tc_visible, 30);
    tcase_add_test(tc_visible, test_renders_visible_command);
    tcase_add_test(tc_visible, test_renders_visible_fork);
    suite_add_tcase(s, tc_visible);

    TCase *tc_render = tcase_create("Render");
    tcase_set_timeout(tc_render, 30);
    tcase_add_test(tc_render, test_render_command_event);
    tcase_add_test(tc_render, test_render_fork_event_parent);
    tcase_add_test(tc_render, test_render_fork_event_child);
    tcase_add_test(tc_render, test_render_command_null_content);
    tcase_add_test(tc_render, test_render_command_empty_content);
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

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
