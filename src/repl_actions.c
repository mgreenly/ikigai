// REPL action processing module
#include "repl_actions.h"
#include "repl.h"
#include "panic.h"
#include "wrapper.h"
#include "format.h"
#include "input_buffer.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

/**
 * @brief Append multi-line output to scrollback (splits by newlines)
 *
 * @param scrollback Scrollback buffer
 * @param output Multi-line text (may contain \n)
 * @param output_len Length of output
 * @return res_t Result
 */
static res_t append_multiline_to_scrollback(ik_scrollback_t *scrollback, const char *output, size_t output_len)
{
    assert(scrollback != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(output != NULL);  /* LCOV_EXCL_BR_LINE */

    if (output_len == 0) {  // LCOV_EXCL_BR_LINE - pp_input_buffer always produces output
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    // Split output by newlines and append each line separately
    size_t line_start = 0;
    for (size_t i = 0; i <= output_len; i++) {
        if (i == output_len || output[i] == '\n') {
            // Found end of line or end of string
            size_t line_len = i - line_start;
            if (line_len > 0 || i < output_len) {  // LCOV_EXCL_BR_LINE
                res_t result = ik_scrollback_append_line(scrollback, output + line_start, line_len);
                if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
            }
            line_start = i + 1;  // Start of next line (skip the \n)
        }
    }
    return OK(NULL);
}

/**
 * @brief Handle slash commands (e.g., /pp input_buffer)
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
    // Expected format: "pp input_buffer" or "pp"
    if (strncmp(command, "pp", 2) == 0) {
        // Create format buffer for output
        ik_format_buffer_t *buf = NULL;
        res_t result = ik_format_buffer_create(repl, &buf);
        if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

        // Pretty-print the input buffer
        ik_pp_input_buffer(repl->input_buffer, buf, 0);

        // Append output to scrollback buffer (split by newlines)
        const char *output = ik_format_get_string(buf);
        size_t output_len = strlen(output);
        result = append_multiline_to_scrollback(repl->scrollback, output, output_len);
        if (is_err(&result))return result;  // LCOV_EXCL_LINE

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
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_insert_codepoint(repl->input_buffer, action->codepoint);
        case IK_INPUT_INSERT_NEWLINE:
            // Ctrl+J inserts newline without submitting (multi-line editing)
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_insert_newline(repl->input_buffer);
        case IK_INPUT_NEWLINE: {
            // Check if input buffer contains a slash command
            const char *text = (const char *)repl->input_buffer->text->data;
            size_t text_len = ik_byte_array_size(repl->input_buffer->text);

            // Check if text starts with '/' and extract command BEFORE submit clears input buffer
            bool is_slash_command = (text_len > 0 && text[0] == '/');
            char *command = NULL;
            if (is_slash_command) {
                // Extract command (skip the '/' character)
                command = ik_talloc_zero_wrapper(repl, text_len); // Includes space for null terminator
                if (command == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                memcpy(command, text + 1, text_len - 1);
                command[text_len - 1] = '\0';
            }

            // Always submit line to scrollback first (so command appears before output)
            res_t result = ik_repl_submit_line(repl);
            if (is_err(&result))return result;  // LCOV_EXCL_LINE

            // If it was a slash command, handle it now (after input buffer text is in scrollback)
            if (is_slash_command) {
                // Handle the slash command (appends output to scrollback)
                result = ik_repl_handle_slash_command(repl, command);
                talloc_free(command);

                if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
            }

            return OK(NULL);
        }
        case IK_INPUT_BACKSPACE:
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_backspace(repl->input_buffer);
        case IK_INPUT_DELETE:
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_delete(repl->input_buffer);
        case IK_INPUT_ARROW_LEFT:
            // Auto-scroll to bottom on input buffer navigation (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_left(repl->input_buffer);
        case IK_INPUT_ARROW_RIGHT:
            // Auto-scroll to bottom on input buffer navigation (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_right(repl->input_buffer);
        case IK_INPUT_ARROW_UP:
            // Auto-scroll to bottom on input buffer navigation (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_up(repl->input_buffer);
        case IK_INPUT_ARROW_DOWN:
            // Auto-scroll to bottom on input buffer navigation (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_down(repl->input_buffer);
        case IK_INPUT_PAGE_UP: {
            // Scroll up by terminal height (increase offset)
            // First ensure layouts are current
            ik_scrollback_ensure_layout(repl->scrollback, repl->term->screen_cols);
            ik_input_buffer_ensure_layout(repl->input_buffer, repl->term->screen_cols);

            // Calculate maximum offset using unified document model
            // Document = scrollback + separator (1) + MAX(input_buffer, 1)
            // Workspace always occupies at least 1 row (for cursor visibility when empty)
            size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->scrollback);
            size_t input_buffer_rows = ik_input_buffer_get_physical_lines(repl->input_buffer);
            size_t input_buffer_display_rows = (input_buffer_rows == 0) ? 1 : input_buffer_rows;
            size_t document_height = scrollback_rows + 1 + input_buffer_display_rows;

            // Max offset = document_height - terminal_rows (can't scroll past top)
            size_t max_offset = 0;
            if (document_height > (size_t)repl->term->screen_rows) {
                max_offset = document_height - (size_t)repl->term->screen_rows;
            }

            // Scroll up by one page
            size_t new_offset = repl->viewport_offset + (size_t)repl->term->screen_rows;
            if (new_offset > max_offset) {
                repl->viewport_offset = max_offset;
            } else {
                repl->viewport_offset = new_offset;
            }
            return OK(NULL);
        }
        case IK_INPUT_PAGE_DOWN: {
            // Scroll down by terminal height (decrease offset)
            if (repl->viewport_offset >= (size_t)repl->term->screen_rows) {
                repl->viewport_offset -= (size_t)repl->term->screen_rows;
            } else {
                repl->viewport_offset = 0;  // At bottom
            }
            return OK(NULL);
        }
        case IK_INPUT_CTRL_A:
            // Auto-scroll to bottom on input buffer navigation (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_to_line_start(repl->input_buffer);
        case IK_INPUT_CTRL_E:
            // Auto-scroll to bottom on input buffer navigation (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_to_line_end(repl->input_buffer);
        case IK_INPUT_CTRL_K:
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_kill_to_line_end(repl->input_buffer);
        case IK_INPUT_CTRL_U:
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_kill_line(repl->input_buffer);
        case IK_INPUT_CTRL_W:
            // Auto-scroll to bottom on input buffer modification (Bug #6 fix)
            repl->viewport_offset = 0;
            return ik_input_buffer_delete_word_backward(repl->input_buffer);
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
