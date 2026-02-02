#include "../../test_constants.h"
/**
 * @file usage_test.c
 * @brief Unit tests for usage event rendering
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/event_render.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"

// Test: ik_event_renders_visible - usage events are visible
START_TEST(test_renders_visible_usage) {
    ck_assert(ik_event_renders_visible("usage"));
}

END_TEST
// Test: Render usage event with all token types
START_TEST(test_render_usage_event_all_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":100,\"output_tokens\":50,\"thinking_tokens\":25}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "100"));
    ck_assert_ptr_nonnull(strstr(text, "50"));
    ck_assert_ptr_nonnull(strstr(text, "25"));
    ck_assert_ptr_nonnull(strstr(text, "175"));
    ck_assert_ptr_nonnull(strstr(text, "thinking"));

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event without thinking tokens
START_TEST(test_render_usage_event_no_thinking) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":100,\"output_tokens\":50}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "100"));
    ck_assert_ptr_nonnull(strstr(text, "50"));
    ck_assert_ptr_nonnull(strstr(text, "150"));
    ck_assert_ptr_null(strstr(text, "thinking"));

    // Second line should be blank
    ik_scrollback_get_line_text(scrollback, 1, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with NULL data_json
START_TEST(test_render_usage_event_null_json) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "usage", NULL, NULL, false);
    ck_assert(!is_err(&result));
    // Should render just a blank line
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 1);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with invalid JSON
START_TEST(test_render_usage_event_invalid_json) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    res_t result = ik_event_render(scrollback, "usage", NULL, "not valid json", false);
    ck_assert(!is_err(&result));
    // Should render just a blank line
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 1);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with zero tokens
START_TEST(test_render_usage_event_zero_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":0,\"output_tokens\":0,\"thinking_tokens\":0}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    // Should render just a blank line (no token line when total is 0)
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 1);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_uint_eq(length, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with non-integer token values
START_TEST(test_render_usage_event_non_integer_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":\"not a number\",\"output_tokens\":50}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    // Should render with the valid tokens only
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "50"));

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with missing token fields
START_TEST(test_render_usage_event_missing_fields) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"output_tokens\":50}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    // Should render with the available tokens
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "50"));

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with null output_tokens
START_TEST(test_render_usage_event_null_output_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":100,\"output_tokens\":null}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    // Should render with input tokens only
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "100"));

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with null thinking_tokens
START_TEST(test_render_usage_event_null_thinking_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":100,\"output_tokens\":50,\"thinking_tokens\":null}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "100"));
    ck_assert_ptr_nonnull(strstr(text, "50"));
    ck_assert_ptr_null(strstr(text, "thinking"));

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with boolean output_tokens
START_TEST(test_render_usage_event_boolean_output_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":100,\"output_tokens\":true,\"thinking_tokens\":25}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "100"));
    ck_assert_ptr_nonnull(strstr(text, "25"));

    talloc_free(ctx);
}

END_TEST
// Test: Render usage event with boolean thinking_tokens
START_TEST(test_render_usage_event_boolean_thinking_tokens) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    const char *json = "{\"input_tokens\":100,\"output_tokens\":50,\"thinking_tokens\":false}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *text;
    size_t length;
    ik_scrollback_get_line_text(scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "100"));
    ck_assert_ptr_nonnull(strstr(text, "50"));
    ck_assert_ptr_null(strstr(text, "thinking"));

    talloc_free(ctx);
}

END_TEST

// Mock ik_scrollback_append_line_ to simulate error
static bool mock_scrollback_error = false;

res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    if (mock_scrollback_error) {
        return ERR(scrollback, IO, "Mock scrollback error");
    }
    return ik_scrollback_append_line(scrollback, text, length);
}

// Test: Render usage event with scrollback error
START_TEST(test_render_usage_event_scrollback_error) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    mock_scrollback_error = true;

    const char *json = "{\"input_tokens\":100,\"output_tokens\":50}";
    res_t result = ik_event_render(scrollback, "usage", NULL, json, false);
    ck_assert(is_err(&result));
    talloc_free(result.err);

    mock_scrollback_error = false;

    talloc_free(ctx);
}

END_TEST

static Suite *event_render_usage_suite(void)
{
    Suite *s = suite_create("Event Render Usage");

    TCase *tc_visible = tcase_create("Visibility");
    tcase_set_timeout(tc_visible, IK_TEST_TIMEOUT);
    tcase_add_test(tc_visible, test_renders_visible_usage);
    suite_add_tcase(s, tc_visible);

    TCase *tc_render = tcase_create("Render");
    tcase_set_timeout(tc_render, IK_TEST_TIMEOUT);
    tcase_add_test(tc_render, test_render_usage_event_all_tokens);
    tcase_add_test(tc_render, test_render_usage_event_no_thinking);
    tcase_add_test(tc_render, test_render_usage_event_null_json);
    tcase_add_test(tc_render, test_render_usage_event_invalid_json);
    tcase_add_test(tc_render, test_render_usage_event_zero_tokens);
    tcase_add_test(tc_render, test_render_usage_event_non_integer_tokens);
    tcase_add_test(tc_render, test_render_usage_event_missing_fields);
    tcase_add_test(tc_render, test_render_usage_event_null_output_tokens);
    tcase_add_test(tc_render, test_render_usage_event_null_thinking_tokens);
    tcase_add_test(tc_render, test_render_usage_event_boolean_output_tokens);
    tcase_add_test(tc_render, test_render_usage_event_boolean_thinking_tokens);
    tcase_add_test(tc_render, test_render_usage_event_scrollback_error);
    suite_add_tcase(s, tc_render);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = event_render_usage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/event_render/usage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
