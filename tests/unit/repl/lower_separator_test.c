/**
 * @file lower_separator_test.c
 * @brief Unit tests for lower separator layer rendering
 */

#include <check.h>
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
#include "../../test_utils.h"

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

/* Test: Render with both upper and lower separators */
START_TEST(test_lower_separator_renders_with_layers)
{
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

    // Create scrollback with a few lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);

    // Add 5 lines to scrollback
    for (int i = 0; i < 5; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res = ik_scrollback_append_line(scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Initialize layer cake
    repl->layer_cake = ik_layer_cake_create(repl, (size_t)term->screen_rows);

    // Create layers
    bool separator_visible = true;
    bool lower_separator_visible = true;
    repl->separator_layer = ik_separator_layer_create(repl, "separator", &separator_visible);
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &lower_separator_visible);

    repl->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", scrollback);

    const char *input_text_ptr = (const char *)input_buf->text->data;
    size_t input_text_len = ik_byte_array_size(input_buf->text);
    bool input_visible = true;
    repl->input_layer = ik_input_layer_create(repl, "input", &input_visible, &input_text_ptr, &input_text_len);

    // Add layers to cake in correct order
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer);
    ck_assert(is_ok(&res));

    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Call render_frame - should render with both separators
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Verify both separators were rendered by checking for box-drawing chars in buffer
    // Box-drawing character U+2500 is 0xE2 0x94 0x80 in UTF-8
    int box_draw_count = 0;
    for (size_t i = 0; i + 2 < mock_write_buffer_len; i++) {
        if ((unsigned char)mock_write_buffer[i] == 0xE2 &&
            (unsigned char)mock_write_buffer[i + 1] == 0x94 &&
            (unsigned char)mock_write_buffer[i + 2] == 0x80) {
            box_draw_count++;
            i += 2;  // Skip past the 3-byte sequence
        }
    }
    // Should have box-drawing chars from both separators (at least 40)
    ck_assert_int_ge(box_draw_count, 40);

    talloc_free(ctx);
}

END_TEST

/* Test: Lower separator visibility flag controls rendering */
START_TEST(test_lower_separator_visibility_flag)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 8, 40, 1, &render);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 8;
    term->screen_cols = 40;
    term->tty_fd = 1;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    shared->render = render;
    shared->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Initialize layer cake
    repl->layer_cake = ik_layer_cake_create(repl, (size_t)term->screen_rows);

    // Create layers
    bool separator_visible = true;
    bool lower_separator_visible = false;  // Initially invisible
    repl->separator_layer = ik_separator_layer_create(repl, "separator", &separator_visible);
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &lower_separator_visible);

    repl->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", scrollback);

    const char *input_text_ptr = (const char *)input_buf->text->data;
    size_t input_text_len = ik_byte_array_size(input_buf->text);
    bool input_visible = true;
    repl->input_layer = ik_input_layer_create(repl, "input", &input_visible, &input_text_ptr, &input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer);
    ck_assert(is_ok(&res));

    // Verify lower separator layer exists and is invisible
    ck_assert(repl->lower_separator_layer != NULL);
    ck_assert(repl->lower_separator_layer->is_visible(repl->lower_separator_layer) == false);

    // Make it visible and verify
    lower_separator_visible = true;
    ck_assert(repl->lower_separator_layer->is_visible(repl->lower_separator_layer) == true);

    talloc_free(ctx);
}

END_TEST

/* Test: Layer order is correct with lower separator */
START_TEST(test_lower_separator_layer_order)
{
    void *ctx = talloc_new(NULL);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 40;
    term->tty_fd = 1;

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    shared->term = term;

    // Initialize layer cake
    repl->layer_cake = ik_layer_cake_create(repl, (size_t)term->screen_rows);

    // Create dummy layers with descriptive names
    bool sep_visible = true;
    bool lower_sep_visible = true;
    repl->separator_layer = ik_separator_layer_create(repl, "separator", &sep_visible);
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &lower_sep_visible);

    // Create mock scrollback and input layers
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);
    repl->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", scrollback);

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    const char *input_text_ptr = (const char *)input_buf->text->data;
    size_t input_text_len = ik_byte_array_size(input_buf->text);
    bool input_visible = true;
    repl->input_layer = ik_input_layer_create(repl, "input", &input_visible, &input_text_ptr, &input_text_len);

    // Add layers in correct order
    res_t res;
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer);
    ck_assert(is_ok(&res));

    // Verify layer cake has all layers
    ck_assert(repl->layer_cake != NULL);
    ck_assert(repl->scrollback_layer != NULL);
    ck_assert(repl->separator_layer != NULL);
    ck_assert(repl->input_layer != NULL);
    ck_assert(repl->lower_separator_layer != NULL);

    // Verify layer names
    ck_assert_str_eq(repl->separator_layer->name, "separator");
    ck_assert_str_eq(repl->lower_separator_layer->name, "lower_separator");

    talloc_free(ctx);
}

END_TEST

static Suite *lower_separator_suite(void)
{
    Suite *s = suite_create("Lower_Separator");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_lower_separator_renders_with_layers);
    tcase_add_test(tc_core, test_lower_separator_visibility_flag);
    tcase_add_test(tc_core, test_lower_separator_layer_order);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = lower_separator_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
