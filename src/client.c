#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <talloc.h>
#include "terminal.h"
#include "input.h"
#include "logger.h"

// Helper to get action type name
static const char *action_type_name(ik_input_action_type_t type)
{
    switch (type) {
        case IK_INPUT_CHAR:
            return "CHAR";
        case IK_INPUT_NEWLINE:
            return "NEWLINE";
        case IK_INPUT_BACKSPACE:
            return "BACKSPACE";
        case IK_INPUT_DELETE:
            return "DELETE";
        case IK_INPUT_ARROW_LEFT:
            return "ARROW_LEFT";
        case IK_INPUT_ARROW_RIGHT:
            return "ARROW_RIGHT";
        case IK_INPUT_ARROW_UP:
            return "ARROW_UP";
        case IK_INPUT_ARROW_DOWN:
            return "ARROW_DOWN";
        case IK_INPUT_CTRL_C:
            return "CTRL_C";
        case IK_INPUT_UNKNOWN:
            return "UNKNOWN";
        default:
            return "???";
    }
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

    // Display terminal information
    char msg[256];
    int len = snprintf(msg, sizeof(msg),
                       "Input Parser Demo\r\n"
                       "Terminal dimensions: %d rows x %d columns\r\n"
                       "Type to see parsed actions. Press Ctrl+C to exit.\r\n\r\n",
                       rows, cols);
    if (write(term_ctx->tty_fd, msg, (size_t)len) < 0) {
        // Ignore write errors in demo
    }

    // Main loop: read bytes, parse into actions, display action details
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

        // Display action details
        if (action.type == IK_INPUT_CHAR) {
            len = snprintf(msg, sizeof(msg), "%s: U+%04" PRIX32 "\r\n",
                           action_type_name(action.type), action.codepoint);
        } else if (action.type != IK_INPUT_UNKNOWN) {
            len = snprintf(msg, sizeof(msg), "%s\r\n",
                           action_type_name(action.type));
        } else {
            // Don't display UNKNOWN (incomplete sequences)
            len = 0;
        }

        if (len > 0 && write(term_ctx->tty_fd, msg, (size_t)len) < 0) {
            // Ignore write errors in demo
        }

        // Exit on Ctrl+C
        if (action.type == IK_INPUT_CTRL_C) {
            break;
        }
    }

    // Cleanup and exit
    ik_term_cleanup(term_ctx);
    talloc_free(root_ctx);
    return EXIT_SUCCESS;
}
