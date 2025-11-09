#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <talloc.h>
#include "error.h"
#include "terminal.h"
#include "input.h"
#include "workspace.h"
#include "logger.h"

// Helper to escape control characters for display
static void escape_for_display(const char *text, size_t len, char *buf, size_t buf_size)
{
    size_t pos = 0;
    for (size_t i = 0; i < len && pos < buf_size - 1; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            if (pos + 2 < buf_size) {
                buf[pos++] = '\\';
                buf[pos++] = 'n';
            }
        } else if (ch == '\r') {
            if (pos + 2 < buf_size) {
                buf[pos++] = '\\';
                buf[pos++] = 'r';
            }
        } else if (ch < 0x20 || ch == 0x7F) {
            // Other control characters as hex
            if (pos + 4 < buf_size) {
                pos += (size_t)snprintf(buf + pos, buf_size - pos, "\\x%02X", ch);
            }
        } else {
            buf[pos++] = (char)ch;
        }
    }
    buf[pos] = '\0';
}

// Process input action and apply to workspace
static res_t process_action(ik_workspace_t *workspace,
                            const ik_input_action_t *action,
                            bool *should_display_out,
                            bool *should_exit_out)
{
    assert(workspace != NULL);
    assert(action != NULL);
    assert(should_display_out != NULL);
    assert(should_exit_out != NULL);

    *should_display_out = false;
    *should_exit_out = false;

    res_t result = {0};

    if (action->type == IK_INPUT_CHAR) {
        result = ik_workspace_insert_codepoint(workspace, action->codepoint);
        if (is_err(&result)) {
            return result;
        }
        *should_display_out = true;
    } else if (action->type == IK_INPUT_NEWLINE) {
        result = ik_workspace_insert_newline(workspace);
        if (is_err(&result)) {
            return result;
        }
        *should_display_out = true;
    } else if (action->type == IK_INPUT_BACKSPACE) {
        result = ik_workspace_backspace(workspace);
        if (is_err(&result)) {
            return result;
        }
        *should_display_out = true;
    } else if (action->type == IK_INPUT_DELETE) {
        result = ik_workspace_delete(workspace);
        if (is_err(&result)) {
            return result;
        }
        *should_display_out = true;
    } else if (action->type == IK_INPUT_CTRL_C) {
        *should_exit_out = true;
    }

    return OK(NULL);
}

int main(void)
{
    // Create root talloc context
    void *root_ctx = talloc_new(NULL);
    if (!root_ctx) {
        fprintf(stderr, "Failed to create talloc context\n");
        return EXIT_FAILURE;
    }

    // Initialize terminal
    ik_term_ctx_t *term_ctx = NULL;
    res_t result = ik_term_init(root_ctx, &term_ctx);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Get terminal dimensions
    int rows, cols;
    result = ik_term_get_size(term_ctx, &rows, &cols);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        ik_term_cleanup(term_ctx);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Create input parser
    ik_input_parser_t *parser = NULL;
    result = ik_input_parser_create(root_ctx, &parser);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        ik_term_cleanup(term_ctx);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Create workspace
    ik_workspace_t *workspace = NULL;
    result = ik_workspace_create(root_ctx, &workspace);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        ik_term_cleanup(term_ctx);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Display header
    char msg[1024];
    int len = snprintf(msg, sizeof(msg),
                       "Workspace Demo\r\n"
                       "Terminal dimensions: %d rows x %d columns\r\n"
                       "Type text, use backspace/delete to edit. Press Ctrl+C to exit.\r\n\r\n",
                       rows, cols);
    if (write(term_ctx->tty_fd, msg, (size_t)len) < 0) {
        // Ignore write errors in demo
    }

    // Main loop: read bytes, parse into actions, apply to workspace
    char byte;
    while (1) {
        ssize_t n = read(term_ctx->tty_fd, &byte, 1);
        if (n <= 0) {
            break;
        }

        // Parse byte into action
        ik_input_action_t action;
        result = ik_input_parse_byte(parser, byte, &action);
        if (is_err(&result)) {
            error_fprintf(stderr, result.err);
            break;
        }

        // Process action
        bool should_display, should_exit;
        result = process_action(workspace, &action, &should_display, &should_exit);
        if (is_err(&result)) {
            error_fprintf(stderr, result.err);
            break;
        }

        if (should_exit) {
            break;
        }

        // Display buffer contents and cursor position
        if (should_display) {
            char *text;
            size_t text_len;
            result = ik_workspace_get_text(workspace, &text, &text_len);
            if (is_err(&result)) {
                error_fprintf(stderr, result.err);
                break;
            }

            // Escape control characters for display
            char escaped[512];
            escape_for_display(text, text_len, escaped, sizeof(escaped));

            // Get cursor position
            size_t cursor = workspace->cursor_byte_offset;

            // Display buffer state
            len = snprintf(msg, sizeof(msg),
                           "Buffer: '%s' | Cursor: %zu\r\n",
                           escaped, cursor);
            if (write(term_ctx->tty_fd, msg, (size_t)len) < 0) {
                // Ignore write errors in demo
            }
        }
    }

    // Cleanup and exit
    ik_term_cleanup(term_ctx);
    talloc_free(root_ctx);
    return EXIT_SUCCESS;
}
