// REPL action processing - viewport and scrolling
#include "repl_actions_internal.h"
#include "repl.h"
#include "agent.h"
#include "shared.h"
#include "scrollback.h"
#include "input_buffer/core.h"
#include <assert.h>
#include <inttypes.h>

#define MOUSE_SCROLL_LINES 3

/**
 * @brief Calculate maximum viewport offset
 *
 * Computes max scrollback offset based on document height and terminal size.
 *
 * @param repl REPL context
 * @return Maximum viewport offset
 */
size_t ik_repl_calculate_max_viewport_offset(ik_repl_ctx_t *repl)
{
    ik_scrollback_ensure_layout(repl->current->scrollback, repl->shared->term->screen_cols);
    ik_input_buffer_ensure_layout(repl->input_buffer, repl->shared->term->screen_cols);

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    size_t input_buffer_rows = ik_input_buffer_get_physical_lines(repl->input_buffer);
    size_t input_buffer_display_rows = (input_buffer_rows == 0) ? 1 : input_buffer_rows;
    size_t document_height = scrollback_rows + 1 + input_buffer_display_rows + 1;  // +1 for lower separator

    if (document_height > (size_t)repl->shared->term->screen_rows) {
        return document_height - (size_t)repl->shared->term->screen_rows;
    }
    return 0;
}

/**
 * @brief Handle page up action
 *
 * Scrolls up by one terminal screen height.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_page_up_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    size_t max_offset = ik_repl_calculate_max_viewport_offset(repl);
    size_t new_offset = repl->current->viewport_offset + (size_t)repl->shared->term->screen_rows;
    repl->current->viewport_offset = (new_offset > max_offset) ? max_offset : new_offset;

    return OK(NULL);
}

/**
 * @brief Handle page down action
 *
 * Scrolls down by one terminal screen height.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_page_down_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->current->viewport_offset >= (size_t)repl->shared->term->screen_rows) {
        repl->current->viewport_offset -= (size_t)repl->shared->term->screen_rows;
    } else {
        repl->current->viewport_offset = 0;
    }
    return OK(NULL);
}

/**
 * @brief Handle scroll up action (mouse scroll)
 *
 * Scrolls up by a few lines (mouse wheel scroll).
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_scroll_up_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    size_t max_offset = ik_repl_calculate_max_viewport_offset(repl);
    size_t new_offset = repl->current->viewport_offset + MOUSE_SCROLL_LINES;
    repl->current->viewport_offset = (new_offset > max_offset) ? max_offset : new_offset;

    return OK(NULL);
}

/**
 * @brief Handle scroll down action (mouse scroll)
 *
 * Scrolls down by a few lines (mouse wheel scroll).
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_scroll_down_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->current->viewport_offset >= MOUSE_SCROLL_LINES) {
        repl->current->viewport_offset -= MOUSE_SCROLL_LINES;
    } else {
        repl->current->viewport_offset = 0;
    }

    return OK(NULL);
}
