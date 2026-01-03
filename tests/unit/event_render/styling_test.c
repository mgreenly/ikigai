/**
 * @file styling_test.c
 * @brief Unit tests for event_render color styling
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/event_render.h"
#include "../../../src/scrollback.h"
#include "../../../src/ansi.h"

// Test: user message has no color codes
START_TEST(test_user_message_no_color) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "user", "Hello", NULL);
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Verify no ANSI escape sequences in user messages
    ck_assert_ptr_null(strstr(text, "\x1b["));
    ck_assert_mem_eq(text, "Hello", 5);

    talloc_free(ctx);
}
END_TEST
// Test: assistant message wrapped with gray 249
START_TEST(test_assistant_message_gray_249) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "assistant", "I am here", NULL);
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Should contain ANSI color sequence for gray 249
    ck_assert_ptr_nonnull(strstr(text, "\x1b[38;5;249m"));
    ck_assert_ptr_nonnull(strstr(text, "\x1b[0m"));
    ck_assert_ptr_nonnull(strstr(text, "I am here"));

    talloc_free(ctx);
}

END_TEST
// Test: tool_call message wrapped with gray 242
START_TEST(test_tool_call_message_gray_242) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "tool_call", "function_call", NULL);
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Should contain ANSI color sequence for gray 242
    ck_assert_ptr_nonnull(strstr(text, "\x1b[38;5;242m"));
    ck_assert_ptr_nonnull(strstr(text, "\x1b[0m"));
    ck_assert_ptr_nonnull(strstr(text, "function_call"));

    talloc_free(ctx);
}

END_TEST
// Test: tool_result message wrapped with gray 242
START_TEST(test_tool_result_message_gray_242) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "tool_result", "result data", NULL);
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Should contain ANSI color sequence for gray 242
    ck_assert_ptr_nonnull(strstr(text, "\x1b[38;5;242m"));
    ck_assert_ptr_nonnull(strstr(text, "\x1b[0m"));
    ck_assert_ptr_nonnull(strstr(text, "result data"));

    talloc_free(ctx);
}

END_TEST
// Test: system message wrapped with gray 242
START_TEST(test_system_message_gray_242) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "system", "System prompt", NULL);
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Should contain ANSI color sequence for gray 242
    ck_assert_ptr_nonnull(strstr(text, "\x1b[38;5;242m"));
    ck_assert_ptr_nonnull(strstr(text, "\x1b[0m"));
    ck_assert_ptr_nonnull(strstr(text, "System prompt"));

    talloc_free(ctx);
}

END_TEST
// Test: mark renders without color (it's user input)
START_TEST(test_mark_no_color) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, "{\"label\":\"checkpoint\"}");
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Verify no ANSI escape sequences in mark messages
    ck_assert_ptr_null(strstr(text, "\x1b["));
    ck_assert_mem_eq(text, "/mark checkpoint", 16);

    talloc_free(ctx);
}

END_TEST
// Test: rewind has no visible output (command input)
START_TEST(test_rewind_no_color) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "rewind", NULL, "{\"target_message_id\":42}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: clear has no visible output (command input)
START_TEST(test_clear_no_color) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "clear", NULL, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: colors disabled - no escape sequences in output
START_TEST(test_colors_disabled) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Initialize ANSI with NO_COLOR set
    setenv("NO_COLOR", "1", 1);
    ik_ansi_init();

    res_t result = ik_event_render(scrollback, "assistant", "Response text", NULL);
    ck_assert(!is_err(&result));

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);

    // Verify no ANSI escape sequences when colors are disabled
    ck_assert_ptr_null(strstr(text, "\x1b["));
    ck_assert_mem_eq(text, "Response text", 13);

    // Clean up
    unsetenv("NO_COLOR");
    ik_ansi_init();

    talloc_free(ctx);
}

END_TEST
// Test: verify scrollback line contains expected escape sequences
START_TEST(test_scrollback_contains_escapes) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Render different kinds of messages
    ik_event_render(scrollback, "user", "User text", NULL);
    ik_event_render(scrollback, "assistant", "AI text", NULL);
    ik_event_render(scrollback, "tool_call", "Tool", NULL);

    const char *text;
    size_t length;

    // Line 0: User text (no color)
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_null(strstr(text, "\x1b["));

    // Line 2: Assistant text (with color 249)
    ik_scrollback_get_line_text(scrollback, 2, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "\x1b[38;5;249m"));

    // Line 4: Tool call (with color 242)
    ik_scrollback_get_line_text(scrollback, 4, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "\x1b[38;5;242m"));

    talloc_free(ctx);
}

END_TEST

static Suite *event_render_styling_suite(void)
{
    Suite *s = suite_create("Event Render Styling");

    TCase *tc_colors = tcase_create("Color Styling");
    tcase_set_timeout(tc_colors, 30);
    tcase_add_test(tc_colors, test_user_message_no_color);
    tcase_add_test(tc_colors, test_assistant_message_gray_249);
    tcase_add_test(tc_colors, test_tool_call_message_gray_242);
    tcase_add_test(tc_colors, test_tool_result_message_gray_242);
    tcase_add_test(tc_colors, test_system_message_gray_242);
    tcase_add_test(tc_colors, test_mark_no_color);
    tcase_add_test(tc_colors, test_rewind_no_color);
    tcase_add_test(tc_colors, test_clear_no_color);
    tcase_add_test(tc_colors, test_colors_disabled);
    tcase_add_test(tc_colors, test_scrollback_contains_escapes);
    suite_add_tcase(s, tc_colors);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = event_render_styling_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
