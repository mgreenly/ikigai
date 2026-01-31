#include "../../test_constants.h"
/**
 * @file basic_test.c
 * @brief Unit tests for event_render module
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/event_render.h"
#include "../../../src/scrollback.h"

// Test: ik_event_renders_visible - user events are visible
START_TEST(test_renders_visible_user) {
    ck_assert(ik_event_renders_visible("user"));
}
END_TEST
// Test: ik_event_renders_visible - assistant events are visible
START_TEST(test_renders_visible_assistant) {
    ck_assert(ik_event_renders_visible("assistant"));
}

END_TEST
// Test: ik_event_renders_visible - system events are not visible (stored for LLM but not shown)
START_TEST(test_renders_visible_system) {
    ck_assert(!ik_event_renders_visible("system"));
}

END_TEST
// Test: ik_event_renders_visible - mark events are visible
START_TEST(test_renders_visible_mark) {
    ck_assert(ik_event_renders_visible("mark"));
}

END_TEST
// Test: ik_event_renders_visible - rewind events are not visible
START_TEST(test_renders_visible_rewind) {
    ck_assert(!ik_event_renders_visible("rewind"));
}

END_TEST
// Test: ik_event_renders_visible - clear events are not visible
START_TEST(test_renders_visible_clear) {
    ck_assert(!ik_event_renders_visible("clear"));
}

END_TEST
// Test: ik_event_renders_visible - NULL returns false
START_TEST(test_renders_visible_null) {
    ck_assert(!ik_event_renders_visible(NULL));
}

END_TEST
// Test: ik_event_renders_visible - unknown kinds return false
START_TEST(test_renders_visible_unknown) {
    ck_assert(!ik_event_renders_visible("unknown"));
    ck_assert(!ik_event_renders_visible(""));
    ck_assert(!ik_event_renders_visible("USER"));  // Case sensitive
}

END_TEST
// Test: Render user event
START_TEST(test_render_user_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "user", "Hello world", NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 15);
    ck_assert_mem_eq(text, "❯ Hello world", 15);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render assistant event
START_TEST(test_render_assistant_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "assistant", "I am an AI", NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    // Assistant messages now include color codes
    ck_assert_ptr_nonnull(strstr(text, "I am an AI"));

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: System events do not render (stored for LLM but not shown in UI)
START_TEST(test_render_system_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "system", "You are helpful.", NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render mark event with label from data_json
START_TEST(test_render_mark_event_with_label) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, "{\"label\":\"foo\"}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 9);
    ck_assert_mem_eq(text, "/mark foo", 9);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render mark event without label (auto-numbered)
START_TEST(test_render_mark_event_no_label) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, "{}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 5);
    ck_assert_mem_eq(text, "/mark", 5);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render mark event with NULL data_json
START_TEST(test_render_mark_event_null_json) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 5);
    ck_assert_mem_eq(text, "/mark", 5);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render mark event with empty label in data_json
START_TEST(test_render_mark_event_empty_label) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, "{\"label\":\"\"}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 5);
    ck_assert_mem_eq(text, "/mark", 5);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render rewind event (renders nothing)
START_TEST(test_render_rewind_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "rewind", NULL, "{\"target_message_id\":42}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render clear event (renders nothing)
START_TEST(test_render_clear_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "clear", NULL, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render agent_killed event (renders nothing)
START_TEST(test_render_agent_killed_event) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Test with target metadata
    const char *json = "{\"killed_by\":\"user\",\"target\":\"uuid-123\"}";
    res_t result = ik_event_render(scrollback, "agent_killed", NULL, json);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    // Test with cascade metadata
    const char *json2 = "{\"killed_by\":\"user\",\"target\":\"uuid-456\",\"cascade\":true,\"count\":5}";
    result = ik_event_render(scrollback, "agent_killed", NULL, json2);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render content event with NULL content
START_TEST(test_render_content_null) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "user", NULL, NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render content event with empty content
START_TEST(test_render_content_empty) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "assistant", "", NULL);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Unknown kind returns error
START_TEST(test_render_unknown_kind) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "unknown", "content", NULL);
    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(error_message(result.err), "Unknown event kind"));
    talloc_free(result.err);

    talloc_free(ctx);
}

END_TEST
// Test: Render mark event with invalid JSON (no crash, treated as no label)
START_TEST(test_render_mark_invalid_json) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, "not valid json");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 5);
    ck_assert_mem_eq(text, "/mark", 5);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render mark event with label not a string
START_TEST(test_render_mark_label_not_string) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "mark", NULL, "{\"label\":123}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 5);
    ck_assert_mem_eq(text, "/mark", 5);

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: NULL kind returns error
START_TEST(test_render_null_kind_returns_error) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, NULL, "content", NULL);
    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(error_message(result.err), "kind"));
    ck_assert_ptr_nonnull(strstr(error_message(result.err), "cannot be NULL"));
    talloc_free(result.err);

    talloc_free(ctx);
}

END_TEST

static Suite *event_render_basic_suite(void)
{
    Suite *s = suite_create("Event Render Basic");

    TCase *tc_visible = tcase_create("Visibility");
    tcase_set_timeout(tc_visible, IK_TEST_TIMEOUT);
    tcase_add_test(tc_visible, test_renders_visible_user);
    tcase_add_test(tc_visible, test_renders_visible_assistant);
    tcase_add_test(tc_visible, test_renders_visible_system);
    tcase_add_test(tc_visible, test_renders_visible_mark);
    tcase_add_test(tc_visible, test_renders_visible_rewind);
    tcase_add_test(tc_visible, test_renders_visible_clear);
    tcase_add_test(tc_visible, test_renders_visible_null);
    tcase_add_test(tc_visible, test_renders_visible_unknown);
    suite_add_tcase(s, tc_visible);

    TCase *tc_render = tcase_create("Render");
    tcase_set_timeout(tc_render, IK_TEST_TIMEOUT);
    tcase_add_test(tc_render, test_render_user_event);
    tcase_add_test(tc_render, test_render_assistant_event);
    tcase_add_test(tc_render, test_render_system_event);
    tcase_add_test(tc_render, test_render_mark_event_with_label);
    tcase_add_test(tc_render, test_render_mark_event_no_label);
    tcase_add_test(tc_render, test_render_mark_event_null_json);
    tcase_add_test(tc_render, test_render_mark_event_empty_label);
    tcase_add_test(tc_render, test_render_rewind_event);
    tcase_add_test(tc_render, test_render_clear_event);
    tcase_add_test(tc_render, test_render_agent_killed_event);
    tcase_add_test(tc_render, test_render_content_null);
    tcase_add_test(tc_render, test_render_content_empty);
    tcase_add_test(tc_render, test_render_unknown_kind);
    tcase_add_test(tc_render, test_render_mark_invalid_json);
    tcase_add_test(tc_render, test_render_mark_label_not_string);
    suite_add_tcase(s, tc_render);

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_test(tc_errors, test_render_null_kind_returns_error);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = event_render_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/event_render/basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
