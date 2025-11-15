#include "repl.h"
#include "panic.h"
#include "wrapper.h"
#include "format.h"
#include "workspace.h"
#include <assert.h>
#include <talloc.h>
#include <stdio.h>
#include <string.h>

res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(repl_out != NULL);   /* LCOV_EXCL_BR_LINE */

    // Allocate REPL context
    ik_repl_ctx_t *repl = ik_talloc_zero_wrapper(parent, sizeof(ik_repl_ctx_t));
    if (repl == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Initialize terminal (raw mode + alternate screen)
    res_t result = ik_term_init(repl, &repl->term);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize render
    result = ik_render_create(repl,
                              repl->term->screen_rows,
                              repl->term->screen_cols,
                              repl->term->tty_fd,
                              &repl->render);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize workspace
    result = ik_workspace_create(repl, &repl->workspace);
    if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

    // Initialize input parser
    result = ik_input_parser_create(repl, &repl->input_parser);
    if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

    // Initialize scrollback buffer (Phase 4)
    result = ik_scrollback_create(repl, repl->term->screen_cols, &repl->scrollback);
    if (is_err(&result))PANIC("allocation failed");  /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset to 0 (at bottom)
    repl->viewport_offset = 0;

    // Set quit flag to false
    repl->quit = false;

    *repl_out = repl;
    return OK(repl);
}

void ik_repl_cleanup(ik_repl_ctx_t *repl)
{
    if (repl == NULL) {
        return;
    }

    // Cleanup terminal (restore state)
    if (repl->term != NULL) {
        ik_term_cleanup(repl->term);
    }

    // Other components cleaned up via talloc hierarchy
    talloc_free(repl);
}

res_t ik_repl_run(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Initial render
    res_t result = ik_repl_render_frame(repl);
    if (is_err(&result)) {
        return result;
    }

    // Main event loop: read bytes, parse into actions, process, render
    char byte;
    while (!repl->quit) {
        ssize_t n = ik_read_wrapper(repl->term->tty_fd, &byte, 1);
        if (n <= 0) {
            // EOF or error - exit loop
            break;
        }

        // Parse byte into action
        ik_input_action_t action;
        ik_input_parse_byte(repl->input_parser, byte, &action);

        // Process action
        result = ik_repl_process_action(repl, &action);
        if (is_err(&result))return result;  // LCOV_EXCL_LINE - Defensive check, input parser validates codepoints

        // Render frame (only if action was not UNKNOWN)
        if (action.type != IK_INPUT_UNKNOWN) {
            result = ik_repl_render_frame(repl);
            if (is_err(&result)) {
                return result;
            }
        }
    }

    return OK(NULL);
}

res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(viewport_out != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->workspace != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */

    // Ensure workspace layout is up to date
    ik_workspace_ensure_layout(repl->workspace, repl->term->screen_cols);

    // Ensure scrollback layout is up to date
    ik_scrollback_ensure_layout(repl->scrollback, repl->term->screen_cols);

    // Get physical rows needed
    size_t workspace_rows = ik_workspace_get_physical_lines(repl->workspace);
    size_t total_scrollback_rows = ik_scrollback_get_total_physical_lines(repl->scrollback);
    size_t scrollback_line_count = ik_scrollback_get_line_count(repl->scrollback);
    int32_t terminal_rows = repl->term->screen_rows;

    // Ensure workspace fits on screen (should never occur - invariant)
    if (workspace_rows > (size_t)terminal_rows) { /* LCOV_EXCL_BR_LINE */
        PANIC("Workspace exceeds terminal height"); /* LCOV_EXCL_LINE */
    }

    // Calculate available rows for scrollback
    size_t available_for_scrollback = (size_t)terminal_rows - workspace_rows;

    // Calculate which scrollback lines to show
    if (total_scrollback_rows <= available_for_scrollback) {
        // All scrollback fits on screen
        viewport_out->scrollback_start_line = 0;
        viewport_out->scrollback_lines_count = scrollback_line_count;
        viewport_out->workspace_start_row = total_scrollback_rows;
    } else {
        // Scrollback overflows - need to calculate visible window
        // viewport_offset tells us how many rows we've scrolled up from bottom
        size_t visible_scrollback_rows = available_for_scrollback;

        // Calculate the bottom position (when offset = 0)
        // We want to show the last N rows of scrollback
        size_t rows_from_bottom = repl->viewport_offset;

        // Clamp viewport_offset to valid range
        size_t max_offset = total_scrollback_rows - available_for_scrollback;
        if (rows_from_bottom > max_offset) {
            rows_from_bottom = max_offset;
        }

        // Calculate which physical row to start from
        size_t target_physical_row = total_scrollback_rows - available_for_scrollback - rows_from_bottom;

        // Find the logical line that contains this physical row
        size_t start_line = 0;
        size_t row_offset = 0;
        res_t result = ik_scrollback_find_logical_line_at_physical_row(repl->scrollback,
                                                                       target_physical_row,
                                                                       &start_line,
                                                                       &row_offset);
        if (is_err(&result)) { /* LCOV_EXCL_BR_LINE */
            PANIC("Failed to find logical line at physical row"); /* LCOV_EXCL_LINE */
        }

        // Calculate how many logical lines fit in the visible window
        size_t lines_to_show = 0;
        size_t rows_used = 0;
        for (size_t i = start_line; i < scrollback_line_count && rows_used < visible_scrollback_rows; i++) {
            rows_used += repl->scrollback->layouts[i].physical_lines;
            lines_to_show++;
        }

        viewport_out->scrollback_start_line = start_line;
        viewport_out->scrollback_lines_count = lines_to_show;
        viewport_out->workspace_start_row = visible_scrollback_rows;
    }

    return OK(repl);
}

res_t ik_repl_render_frame(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->render != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->workspace != NULL);   /* LCOV_EXCL_BR_LINE */

    // Calculate viewport to determine what to render
    ik_viewport_t viewport;
    res_t result = ik_repl_calculate_viewport(repl, &viewport);
    if (is_err(&result))return result;  /* LCOV_EXCL_LINE */

    // If no scrollback visible, just render workspace (current behavior)
    if (viewport.scrollback_lines_count == 0) {
        // Get workspace text (cannot fail - always returns OK)
        char *text = NULL;
        size_t text_len = 0;
        ik_workspace_get_text(repl->workspace, &text, &text_len);

        // Get cursor byte offset (cannot fail - always returns OK)
        size_t cursor_byte_offset = 0;
        size_t cursor_grapheme = 0;
        ik_workspace_get_cursor_position(repl->workspace, &cursor_byte_offset, &cursor_grapheme);

        // Render workspace with cursor
        return ik_render_workspace(repl->render, text, text_len, cursor_byte_offset);
    }

    // Otherwise render both scrollback and workspace
    // Phase 4 Task 4.4: Combined frame rendering
    // Strategy: Call render_scrollback (which clears + writes scrollback)
    // then render workspace positioned after scrollback

    // Clear screen, render scrollback
    int32_t scrollback_rows_used = 0;
    result = ik_render_scrollback(repl->render, repl->scrollback,
                                  viewport.scrollback_start_line,
                                  viewport.scrollback_lines_count,
                                  &scrollback_rows_used);
    if (is_err(&result))return result;  /* LCOV_EXCL_LINE */

    // Get workspace text
    char *text = NULL;
    size_t text_len = 0;
    ik_workspace_get_text(repl->workspace, &text, &text_len);

    size_t cursor_byte_offset = 0;
    size_t cursor_grapheme = 0;
    ik_workspace_get_cursor_position(repl->workspace, &cursor_byte_offset, &cursor_grapheme);

    // Render workspace positioned after scrollback
    // For now, just use ik_render_workspace which will re-clear and render
    // TODO Phase 4.4: Optimize to single atomic write
    return ik_render_workspace(repl->render, text, text_len, cursor_byte_offset);
}

/**
 * @brief Handle slash commands (e.g., /pp workspace)
 *
 * @param repl REPL context
 * @param command Command text (without leading /)
 * @return res_t Result
 */
static res_t ik_repl_handle_slash_command(ik_repl_ctx_t *repl, const char *command)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(command != NULL); /* LCOV_EXCL_BR_LINE */

    // Parse command (simple whitespace-based parsing for now)
    // Expected format: "pp workspace" or "pp"
    if (strncmp(command, "pp", 2) == 0) {
        // Create format buffer for output
        ik_format_buffer_t *buf = NULL;
        res_t result = ik_format_buffer_create(repl, &buf);
        if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

        // Pretty-print the workspace
        ik_pp_workspace(repl->workspace, buf, 0);

        // Output to stdout (temporary until scrollback exists)
        const char *output = ik_format_get_string(buf);
        printf("%s", output);
        fflush(stdout);

        // Clean up format buffer
        talloc_free(buf);

        return OK(NULL);
    }

    // Unknown command - just ignore for now
    return OK(NULL);
}

res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(action != NULL);   /* LCOV_EXCL_BR_LINE */

    switch (action->type) { // LCOV_EXCL_BR_LINE
        case IK_INPUT_CHAR:
            return ik_workspace_insert_codepoint(repl->workspace, action->codepoint);
        case IK_INPUT_NEWLINE: {
            // Check if workspace contains a slash command
            const char *text = (const char *)repl->workspace->text->data;
            size_t text_len = ik_byte_array_size(repl->workspace->text);

            // Check if text starts with '/'
            if (text_len > 0 && text[0] == '/') {
                // Extract command (skip the '/' character)
                char *command = ik_talloc_zero_wrapper(repl, text_len); // Includes space for null terminator
                if (command == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                memcpy(command, text + 1, text_len - 1);
                command[text_len - 1] = '\0';

                // Handle the slash command
                res_t result = ik_repl_handle_slash_command(repl, command);
                talloc_free(command);

                if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

                // Clear workspace after executing command
                ik_workspace_clear(repl->workspace);
                return OK(NULL);
            }

            // Not a slash command, insert newline as usual
            return ik_workspace_insert_newline(repl->workspace);
        }
        case IK_INPUT_BACKSPACE:
            return ik_workspace_backspace(repl->workspace);
        case IK_INPUT_DELETE:
            return ik_workspace_delete(repl->workspace);
        case IK_INPUT_ARROW_LEFT:
            return ik_workspace_cursor_left(repl->workspace);
        case IK_INPUT_ARROW_RIGHT:
            return ik_workspace_cursor_right(repl->workspace);
        case IK_INPUT_ARROW_UP:
            return ik_workspace_cursor_up(repl->workspace);
        case IK_INPUT_ARROW_DOWN:
            return ik_workspace_cursor_down(repl->workspace);
        case IK_INPUT_CTRL_A:
            return ik_workspace_cursor_to_line_start(repl->workspace);
        case IK_INPUT_CTRL_E:
            return ik_workspace_cursor_to_line_end(repl->workspace);
        case IK_INPUT_CTRL_K:
            return ik_workspace_kill_to_line_end(repl->workspace);
        case IK_INPUT_CTRL_U:
            return ik_workspace_kill_line(repl->workspace);
        case IK_INPUT_CTRL_W:
            return ik_workspace_delete_word_backward(repl->workspace);
        case IK_INPUT_CTRL_C:
            repl->quit = true;
            return OK(NULL);
        case IK_INPUT_UNKNOWN:
            // Unknown actions are ignored
            return OK(NULL);
        default: // LCOV_EXCL_LINE
            PANIC("Invalid input action type"); // LCOV_EXCL_LINE
    }
}
