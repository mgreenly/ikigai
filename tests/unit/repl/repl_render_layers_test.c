#include "agent.h"
/**
 * @file repl_render_layers_test.c
 * @brief Unit tests for REPL render_frame function with layer-based rendering
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include "../../../src/repl.h"
#include "../../../src/render.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../../src/byte_array.h"
#include "../../test_utils_helper.h"

// Mock write tracking
static int32_t mock_write_calls = 0;
static char mock_write_buffer[4096];
static size_t mock_write_buffer_len = 0;
static bool mock_write_should_fail = false;

// Mock write wrapper declaration
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock write wrapper for testing
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    mock_write_calls++;

    if (mock_write_should_fail) {
        return -1;  // Simulate write failure
    }

    if (mock_write_buffer_len + count < sizeof(mock_write_buffer)) {
        memcpy(mock_write_buffer + mock_write_buffer_len, buf, count);
        mock_write_buffer_len += count;
    }
    return (ssize_t)count;
}

/* Test: Render with layer-based rendering and visible input buffer */
START_TEST(test_repl_render_frame_with_layers_visible_input) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    // Add some text to the input buffer
    const char *text = "test input";
    for (size_t i = 0; i < strlen(text); i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)text[i]);
        ck_assert(is_ok(&res));
    }

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 10, 40, 1, &render);  // Terminal: 10x40
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 40;
    term->tty_fd = 1;

    // Create scrollback with just a few lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);

    // Add 15 lines to scrollback (more than terminal height, to test viewport offset)
    for (int i = 0; i < 15; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res = ik_scrollback_append_line(scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;  // No offset - input buffer will be visible

    // Initialize layer cake
    repl->current->layer_cake = ik_layer_cake_create(repl, (size_t)term->screen_rows);

    // Create layers
    bool separator_visible = true;
    repl->current->separator_layer = ik_separator_layer_create(repl, "separator", &separator_visible);

    repl->current->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", scrollback);

    const char *input_text_ptr = (const char *)input_buf->text->data;
    size_t input_text_len = ik_byte_array_size(input_buf->text);
    bool input_visible = true;
    repl->current->input_layer = ik_input_layer_create(repl, "input", &input_visible, &input_text_ptr, &input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->input_layer);
    ck_assert(is_ok(&res));

    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Call render_frame - should render with visible input buffer and cursor
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Render with layer-based rendering and scrolling */
START_TEST(test_repl_render_frame_with_layers_scrolling) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    // Add some text to the input buffer
    const char *text = "test input";
    for (size_t i = 0; i < strlen(text); i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)text[i]);
        ck_assert(is_ok(&res));
    }

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 5, 40, 1, &render);  // Small terminal: 5x40
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 5;  // Small to trigger scrolling
    term->screen_cols = 40;
    term->tty_fd = 1;

    // Create scrollback with many lines (more than terminal height)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);

    // Add 10 lines to scrollback (more than 5 row terminal)
    for (int i = 0; i < 10; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Scrollback line %d", i);
        res = ik_scrollback_append_line(scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 100;  // Set very large offset to test clamping logic

    // Initialize layer cake
    repl->current->layer_cake = ik_layer_cake_create(repl, (size_t)term->screen_rows);

    // Create layers
    bool separator_visible = true;
    repl->current->separator_layer = ik_separator_layer_create(repl, "separator", &separator_visible);

    repl->current->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", scrollback);

    const char *input_text_ptr = (const char *)input_buf->text->data;
    size_t input_text_len = ik_byte_array_size(input_buf->text);
    bool input_visible = true;
    repl->current->input_layer = ik_input_layer_create(repl, "input", &input_visible, &input_text_ptr, &input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->input_layer);
    ck_assert(is_ok(&res));

    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Call render_frame - should use layer-based rendering and handle scrolling
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: IO write error handling */
START_TEST(test_repl_render_frame_write_failure) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    term->tty_fd = 1;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Initialize layer cake
    repl->current->layer_cake = ik_layer_cake_create(repl, (size_t)term->screen_rows);

    // Create layers
    bool separator_visible = true;
    repl->current->separator_layer = ik_separator_layer_create(repl, "separator", &separator_visible);

    repl->current->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", scrollback);

    const char *input_text_ptr = (const char *)input_buf->text->data;
    size_t input_text_len = ik_byte_array_size(input_buf->text);
    bool input_visible = true;
    repl->current->input_layer = ik_input_layer_create(repl, "input", &input_visible, &input_text_ptr, &input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->input_layer);
    ck_assert(is_ok(&res));

    // Enable write failure
    mock_write_should_fail = true;
    mock_write_calls = 0;

    // Call render_frame - should return IO error
    res = ik_repl_render_frame(repl);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    // Verify write was attempted
    ck_assert_int_gt(mock_write_calls, 0);

    // Reset mock state for subsequent tests
    mock_write_should_fail = false;

    talloc_free(ctx);
}

END_TEST

static Suite *repl_render_layers_suite(void)
{
    Suite *s = suite_create("REPL_Render_Layers");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Layer-based rendering tests */
    tcase_add_test(tc_core, test_repl_render_frame_with_layers_visible_input);
    tcase_add_test(tc_core, test_repl_render_frame_with_layers_scrolling);
    tcase_add_test(tc_core, test_repl_render_frame_write_failure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_render_layers_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
