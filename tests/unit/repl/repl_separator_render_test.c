#include "agent.h"
/**
 * @file repl_separator_render_test.c
 * @brief Unit tests for separator rendering in REPL
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

/* Test: Separator renders on empty scrollback */
START_TEST(test_separator_renders_on_empty_scrollback) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 5, 40, 1, &render);  // Terminal: 5x40
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 5;
    term->screen_cols = 40;
    term->tty_fd = 1;

    // Create EMPTY scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);

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

    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Call render_frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Check that separator (line of box-drawing chars) appears in output
    // Separator should be a line of 40 box-drawing characters (3 bytes each) followed by \r\n = 122 bytes
    // Box-drawing character U+2500 is 0xE2 0x94 0x80 in UTF-8
    bool separator_found = false;
    for (size_t i = 0; i + 122 <= mock_write_buffer_len; i++) {
        if ((unsigned char)mock_write_buffer[i] == 0xE2 &&
            (unsigned char)mock_write_buffer[i + 1] == 0x94 &&
            (unsigned char)mock_write_buffer[i + 2] == 0x80) {
            // Check if the full line matches 40 box-drawing chars + \r\n
            bool match = true;
            for (size_t j = 0; j < 40; j++) {
                if ((unsigned char)mock_write_buffer[i + j * 3] != 0xE2 ||
                    (unsigned char)mock_write_buffer[i + j * 3 + 1] != 0x94 ||
                    (unsigned char)mock_write_buffer[i + j * 3 + 2] != 0x80) {
                    match = false;
                    break;
                }
            }
            if (match && mock_write_buffer[i + 120] == '\r' && mock_write_buffer[i + 121] == '\n') {
                separator_found = true;
                break;
            }
        }
    }
    ck_assert_msg(separator_found, "Separator line (box-drawing chars) not found in rendered output");

    talloc_free(ctx);
}

END_TEST
/* Test: Separator renders with scrollback content */
START_TEST(test_separator_renders_with_scrollback) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 10, 40, 1, &render);  // Terminal: 10x40
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 40;
    term->tty_fd = 1;

    // Create scrollback with content
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);
    for (int i = 0; i < 3; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res = ik_scrollback_append_line(scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
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

    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Call render_frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Check that separator (line of box-drawing chars) appears in output
    // Separator should be a line of 40 box-drawing characters (3 bytes each) followed by \r\n = 122 bytes
    // Box-drawing character U+2500 is 0xE2 0x94 0x80 in UTF-8
    bool separator_found = false;
    for (size_t i = 0; i + 122 <= mock_write_buffer_len; i++) {
        if ((unsigned char)mock_write_buffer[i] == 0xE2 &&
            (unsigned char)mock_write_buffer[i + 1] == 0x94 &&
            (unsigned char)mock_write_buffer[i + 2] == 0x80) {
            // Check if the full line matches 40 box-drawing chars + \r\n
            bool match = true;
            for (size_t j = 0; j < 40; j++) {
                if ((unsigned char)mock_write_buffer[i + j * 3] != 0xE2 ||
                    (unsigned char)mock_write_buffer[i + j * 3 + 1] != 0x94 ||
                    (unsigned char)mock_write_buffer[i + j * 3 + 2] != 0x80) {
                    match = false;
                    break;
                }
            }
            if (match && mock_write_buffer[i + 120] == '\r' && mock_write_buffer[i + 121] == '\n') {
                separator_found = true;
                break;
            }
        }
    }
    ck_assert_msg(separator_found, "Separator line (box-drawing chars) not found in rendered output with scrollback");

    talloc_free(ctx);
}

END_TEST
/* Test: Separator does NOT render when visibility flag is false */
START_TEST(test_separator_not_renders_when_invisible) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 5, 40, 1, &render);  // Terminal: 5x40
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 5;
    term->screen_cols = 40;
    term->tty_fd = 1;

    // Create scrollback with content
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);
    for (int i = 0; i < 3; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res = ik_scrollback_append_line(scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
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

    // Create layers - SEPARATOR IS INVISIBLE
    bool separator_visible = false;  // Intentionally false
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

    // Call render_frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Check that separator (line of box-drawing chars) does NOT appear in output when invisible
    // Box-drawing character U+2500 is 0xE2 0x94 0x80 in UTF-8
    bool separator_found = false;
    for (size_t i = 0; i + 122 <= mock_write_buffer_len; i++) {
        if ((unsigned char)mock_write_buffer[i] == 0xE2 &&
            (unsigned char)mock_write_buffer[i + 1] == 0x94 &&
            (unsigned char)mock_write_buffer[i + 2] == 0x80) {
            // Check if the full line matches 40 box-drawing chars + \r\n
            bool match = true;
            for (size_t j = 0; j < 40; j++) {
                if ((unsigned char)mock_write_buffer[i + j * 3] != 0xE2 ||
                    (unsigned char)mock_write_buffer[i + j * 3 + 1] != 0x94 ||
                    (unsigned char)mock_write_buffer[i + j * 3 + 2] != 0x80) {
                    match = false;
                    break;
                }
            }
            if (match && mock_write_buffer[i + 120] == '\r' && mock_write_buffer[i + 121] == '\n') {
                separator_found = true;
                break;
            }
        }
    }
    ck_assert_msg(!separator_found, "Separator line should NOT be in output when visibility is false");

    talloc_free(ctx);
}

END_TEST

static Suite *repl_separator_render_suite(void)
{
    Suite *s = suite_create("REPL_Separator_Render");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_separator_renders_on_empty_scrollback);
    tcase_add_test(tc_core, test_separator_renders_with_scrollback);
    tcase_add_test(tc_core, test_separator_not_renders_when_invisible);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_separator_render_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
