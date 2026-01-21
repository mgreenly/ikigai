#include "../../test_constants.h"
/**
 * @file edge_cases_test.c
 * @brief Edge case tests for event_render module with mocking
 *
 * Tests defensive code paths that require mocking yyjson behavior.
 */

#include <check.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/event_render.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"

// Mock state for yyjson_get_str_
static bool mock_yyjson_get_str_should_return_null = false;

// Mock state for ik_scrollback_append_line_
static bool mock_scrollback_append_should_fail = false;
static TALLOC_CTX *mock_err_ctx = NULL;

// Cleanup function for mock error context
static void cleanup_mock_err_ctx(void)
{
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

// Mock yyjson_get_str_ to return NULL when flag is set
const char *yyjson_get_str_(yyjson_val *val)
{
    if (mock_yyjson_get_str_should_return_null) {
        return NULL;
    }
    return yyjson_get_str(val);
}

// Mock ik_scrollback_append_line_ to fail when flag is set
MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    if (mock_scrollback_append_should_fail) {
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, IO, "Mock scrollback append failure");
    }

    // Call real implementation
    return ik_scrollback_append_line(scrollback, text, length);
}

static void reset_mocks(void)
{
    mock_yyjson_get_str_should_return_null = false;
    mock_scrollback_append_should_fail = false;
}

// Test: Render mark event when yyjson_get_str_ returns NULL
// This tests the defensive branch at line 55 in event_render.c
START_TEST(test_render_mark_yyjson_get_str_returns_null) {
    void *ctx = talloc_new(NULL);
    reset_mocks();

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Enable mock to return NULL
    mock_yyjson_get_str_should_return_null = true;

    // Even with valid JSON, our mock will make yyjson_get_str_ return NULL
    res_t result = ik_event_render(scrollback, "mark", NULL, "{\"label\":\"foo\"}");
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    // Should render as "/mark" since label extraction failed
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
// Test: Render mark event when scrollback append fails
// This tests the error handling at lines 85-87 in event_render.c
START_TEST(test_render_mark_scrollback_append_fails) {
    void *ctx = talloc_new(NULL);
    reset_mocks();

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Make scrollback append fail
    mock_scrollback_append_should_fail = true;

    // Attempt to render mark event
    res_t result = ik_event_render(scrollback, "mark", NULL, "{\"label\":\"foo\"}");

    // Should return error
    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(error_message(result.err), "Mock scrollback append failure"));

    // Clean up error
    talloc_free(result.err);
    talloc_free(ctx);
}

END_TEST

static Suite *event_render_edge_cases_suite(void)
{
    Suite *s = suite_create("Event Render Edge Cases");

    TCase *tc_mocked = tcase_create("Mocked Behavior");
    tcase_set_timeout(tc_mocked, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_mocked, NULL, reset_mocks);
    tcase_add_test(tc_mocked, test_render_mark_yyjson_get_str_returns_null);
    tcase_add_test(tc_mocked, test_render_mark_scrollback_append_fails);
    suite_add_tcase(s, tc_mocked);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = event_render_edge_cases_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/event_render/edge_cases_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
