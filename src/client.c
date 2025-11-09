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
#include "render.h"
#include "logger.h"

// Process input action and apply to workspace
static res_t process_action(ik_workspace_t *workspace,
                            const ik_input_action_t *action,
                            bool *should_exit_out)
{
    assert(workspace != NULL);
    assert(action != NULL);
    assert(should_exit_out != NULL);

    *should_exit_out = false;

    if (action->type == IK_INPUT_CHAR) {
        return ik_workspace_insert_codepoint(workspace, action->codepoint);
    } else if (action->type == IK_INPUT_NEWLINE) {
        return ik_workspace_insert_newline(workspace);
    } else if (action->type == IK_INPUT_BACKSPACE) {
        return ik_workspace_backspace(workspace);
    } else if (action->type == IK_INPUT_DELETE) {
        return ik_workspace_delete(workspace);
    } else if (action->type == IK_INPUT_ARROW_LEFT) {
        return ik_workspace_cursor_left(workspace);
    } else if (action->type == IK_INPUT_ARROW_RIGHT) {
        return ik_workspace_cursor_right(workspace);
    } else if (action->type == IK_INPUT_CTRL_C) {
        *should_exit_out = true;
    }

    return OK(NULL);
}

// Render the current workspace state to the screen
static res_t render_frame(ik_render_ctx_t *render,
                          ik_workspace_t *workspace,
                          int32_t tty_fd)
{
    assert(render != NULL);
    assert(workspace != NULL);
    assert(tty_fd >= 0);

    // Clear the render context
    ik_render_clear(render);

    // Get workspace text
    char *text = NULL;
    size_t text_len = 0;
    res_t result = ik_workspace_get_text(workspace, &text, &text_len);
    if (is_err(&result)) {
        return result;
    }

    // Write text to render context (if not empty)
    if (text_len > 0) {
        result = ik_render_write_text(render, text, text_len);
        if (is_err(&result)) {
            return result;
        }
    }

    // Blit to screen
    return ik_render_blit(render, tty_fd);
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

    // Create render context
    ik_render_ctx_t *render = NULL;
    result = ik_render_create(root_ctx, rows, cols, &render);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        ik_term_cleanup(term_ctx);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Initial render
    result = render_frame(render, workspace, term_ctx->tty_fd);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        ik_term_cleanup(term_ctx);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Main loop: read bytes, parse into actions, apply to workspace, render
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
        bool should_exit;
        result = process_action(workspace, &action, &should_exit);
        if (is_err(&result)) {
            error_fprintf(stderr, result.err);
            break;
        }

        if (should_exit) {
            break;
        }

        // Render frame (only if action was not UNKNOWN)
        if (action.type != IK_INPUT_UNKNOWN) {
            result = render_frame(render, workspace, term_ctx->tty_fd);
            if (is_err(&result)) {
                error_fprintf(stderr, result.err);
                break;
            }
        }
    }

    // Cleanup and exit
    ik_term_cleanup(term_ctx);
    talloc_free(root_ctx);
    return EXIT_SUCCESS;
}
