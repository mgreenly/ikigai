#include "apps/ikigai/agent.h"
/**
 * @file repl_full_viewport_test.c
 * @brief Test for layer positioning when viewport is full
 *
 * Bug: When scrollback fills the entire viewport, the document model calculation
 * doesn't account for the lower separator, causing layers to be positioned incorrectly.
 */

#include <check.h>
#include "apps/ikigai/agent.h"
#include "apps/ikigai/shared.h"
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/layer.h"
#include "apps/ikigai/layer_wrappers.h"
#include "apps/ikigai/render.h"
#include "tests/helpers/test_utils_helper.h"

// Mock write wrapper for testing (required by render system)
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

// Helper function to create a basic REPL context for testing
static void create_test_repl(TALLOC_CTX *ctx, int32_t rows, int32_t cols, ik_repl_ctx_t **repl_out)
{
    res_t res;

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, rows, cols, 1, &render);
    ck_assert(is_ok(&res));

    // Create terminal context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = rows;
    term->screen_cols = cols;
    term->tty_fd = 1;

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;

    // Create agent context using ik_test_create_agent
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;
    agent->viewport_offset = 0;
    agent->input_buffer_visible = true;  // Required for document height calculation

    *repl_out = repl;
}

/**
 * Test: Bottom separator visible when viewport is full
 *
 * When the viewport is filled with content, the bottom separator should still be visible
 * (not pushed off-screen).
 */
START_TEST(test_bottom_separator_visible_when_viewport_full) {
    void *ctx = talloc_new(NULL);

    // Create REPL with 20 rows x 80 cols
    ik_repl_ctx_t *repl = NULL;
    create_test_repl(ctx, 20, 80, &repl);
    res_t res;

    // Fill scrollback with 17 lines (leaves exactly 3 rows for sep + input + lower_sep)
    for (int32_t i = 0; i < 17; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "scrollback line %d", i + 1);
        res = ik_scrollback_append_line(repl->current->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(repl->current->scrollback, 80);

    // Add text to input buffer
    res = ik_input_buffer_insert_codepoint(repl->current->input_buffer, '*');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(repl->current->input_buffer, 80);

    // Update input text pointers
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    repl->current->input_text = text;
    repl->current->input_text_len = text_len;

    // Render frame to test that lower separator is within viewport
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 4096);
    repl->current->layer_cake->viewport_row = 0;
    repl->current->layer_cake->viewport_height = 20;

    ik_layer_cake_render(repl->current->layer_cake, output, 80);

    // Verify output contains separator characters
    // The lower separator renders as a line of Unicode box-drawing characters (─)
    // UTF-8 encoding: 0xE2 0x94 0x80
    bool found_separator = false;
    for (size_t i = 0; i < output->size - 2; i++) {
        if ((unsigned char)output->data[i] == 0xE2 &&
            (unsigned char)output->data[i + 1] == 0x94 &&
            (unsigned char)output->data[i + 2] == 0x80) {
            found_separator = true;
            break;
        }
    }

    ck_assert_msg(found_separator,
                  "Lower separator should be visible in rendered output");

    talloc_free(ctx);
}

END_TEST

static Suite *repl_full_viewport_suite(void)
{
    Suite *s = suite_create("REPL Full Viewport");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_bottom_separator_visible_when_viewport_full);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = repl_full_viewport_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_full_viewport_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
