/**
 * @file spacing_test.c
 * @brief Tests for event render spacing (blank line after each event)
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/event_render.h"
#include "../../../src/scrollback.h"

// Test: Render user event adds blank line
START_TEST(test_event_render_user_adds_blank_line)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_event_render(sb, "user", "hello", "{}");
    ck_assert(!is_err(&res));

    // Should have 2 lines: "hello" and ""
    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 2);

    const char *text1;
    size_t len1;
    ik_scrollback_get_line_text(sb, 0, &text1, &len1);
    ck_assert_uint_eq(len1, 5);
    ck_assert_mem_eq(text1, "hello", 5);

    const char *text2;
    size_t len2;
    ik_scrollback_get_line_text(sb, 1, &text2, &len2);
    ck_assert_uint_eq(len2, 0);
    ck_assert_mem_eq(text2, "", 0);

    talloc_free(ctx);
}

END_TEST

// Test: Render event trims trailing newlines
START_TEST(test_event_render_trims_trailing_newlines)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_event_render(sb, "user", "hello\n\n\n", "{}");
    ck_assert(!is_err(&res));

    // Should have 2 lines: "hello" (trimmed) and ""
    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 2);

    const char *text1;
    size_t len1;
    ik_scrollback_get_line_text(sb, 0, &text1, &len1);
    ck_assert_uint_eq(len1, 5);
    ck_assert_mem_eq(text1, "hello", 5);

    const char *text2;
    size_t len2;
    ik_scrollback_get_line_text(sb, 1, &text2, &len2);
    ck_assert_uint_eq(len2, 0);

    talloc_free(ctx);
}

END_TEST

// Test: Render mark event adds blank line
START_TEST(test_event_render_mark_adds_blank_line)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_event_render(sb, "mark", NULL, "{\"label\": \"checkpoint\"}");
    ck_assert(!is_err(&res));

    // Should have 2 lines: "/mark checkpoint" and ""
    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 2);

    const char *text1;
    size_t len1;
    ik_scrollback_get_line_text(sb, 0, &text1, &len1);
    ck_assert_uint_eq(len1, 16);
    ck_assert_mem_eq(text1, "/mark checkpoint", 16);

    const char *text2;
    size_t len2;
    ik_scrollback_get_line_text(sb, 1, &text2, &len2);
    ck_assert_uint_eq(len2, 0);

    talloc_free(ctx);
}

END_TEST

// Test: Render tool_call event adds blank line
START_TEST(test_event_render_tool_call_adds_blank_line)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_event_render(sb, "tool_call", "→ glob: pattern=\"*.c\"", "{}");
    ck_assert(!is_err(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 2);

    const char *text1;
    size_t len1;
    ik_scrollback_get_line_text(sb, 0, &text1, &len1);
    // tool_call messages now include color codes
    ck_assert_ptr_nonnull(strstr(text1, "→ glob: pattern=\"*.c\""));

    const char *text2;
    size_t len2;
    ik_scrollback_get_line_text(sb, 1, &text2, &len2);
    ck_assert_uint_eq(len2, 0);

    talloc_free(ctx);
}

END_TEST

// Test: Empty content produces no output (no double blank line)
START_TEST(test_event_render_empty_content_no_double_blank)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Empty content should not add anything
    res_t res = ik_event_render(sb, "system", "", "{}");
    ck_assert(!is_err(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 0);

    talloc_free(ctx);
}

END_TEST

// Test: Multiline content gets one blank line after
START_TEST(test_event_render_multiline_content)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_event_render(sb, "user", "line1\nline2\nline3", "{}");
    ck_assert(!is_err(&res));

    // Content is one logical line (with embedded newlines) + blank line
    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 2);

    const char *text1;
    size_t len1;
    ik_scrollback_get_line_text(sb, 0, &text1, &len1);
    ck_assert_uint_eq(len1, 17);
    ck_assert_mem_eq(text1, "line1\nline2\nline3", 17);

    const char *text2;
    size_t len2;
    ik_scrollback_get_line_text(sb, 1, &text2, &len2);
    ck_assert_uint_eq(len2, 0);

    talloc_free(ctx);
}

END_TEST

static Suite *event_render_spacing_suite(void)
{
    Suite *s = suite_create("Event Render Spacing");

    TCase *tc_spacing = tcase_create("Spacing");
    tcase_add_test(tc_spacing, test_event_render_user_adds_blank_line);
    tcase_add_test(tc_spacing, test_event_render_trims_trailing_newlines);
    tcase_add_test(tc_spacing, test_event_render_mark_adds_blank_line);
    tcase_add_test(tc_spacing, test_event_render_tool_call_adds_blank_line);
    tcase_add_test(tc_spacing, test_event_render_empty_content_no_double_blank);
    tcase_add_test(tc_spacing, test_event_render_multiline_content);
    suite_add_tcase(s, tc_spacing);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = event_render_spacing_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
