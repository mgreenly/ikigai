// Input parser module - Convert raw bytes to semantic actions
#include <assert.h>
#include <talloc.h>
#include "input.h"
#include "wrapper.h"

// Create input parser
res_t ik_input_parser_create(void *parent, ik_input_parser_t **parser_out)
{
    assert(parent != NULL);      // LCOV_EXCL_BR_LINE
    assert(parser_out != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate parser
    ik_input_parser_t *parser = ik_talloc_zero_wrapper(parent, sizeof(ik_input_parser_t));
    if (parser == NULL) {
        return ERR(parent, OOM, "Failed to allocate input parser");
    }

    // Initialize fields (talloc_zero already set to 0, but be explicit)
    parser->esc_len = 0;
    parser->in_escape = false;

    *parser_out = parser;
    return OK(parser);
}

// Helper to reset escape sequence state
static void reset_escape_state(ik_input_parser_t *parser)
{
    parser->in_escape = false;
    parser->esc_len = 0;
}

// Parse single byte into action
res_t ik_input_parse_byte(ik_input_parser_t *parser, char byte,
                           ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // If we're in escape sequence mode, buffer the byte
    if (parser->in_escape) {
        // Buffer the byte
        parser->esc_buf[parser->esc_len++] = byte;
        parser->esc_buf[parser->esc_len] = '\0';

        // Buffer overflow protection - reset if we've filled the buffer
        if (parser->esc_len >= sizeof(parser->esc_buf) - 1) {
            reset_escape_state(parser);
            action_out->type = IK_INPUT_UNKNOWN;
            return OK(parser);
        }

        // Validate first byte after ESC must be '['
        if (parser->esc_len == 1 && byte != '[') {
            reset_escape_state(parser);
            action_out->type = IK_INPUT_UNKNOWN;
            return OK(parser);
        }

        // Check for arrow keys: ESC [ A/B/C/D
        // Note: We know esc_buf[0] == '[' due to validation above
        if (parser->esc_len == 2) {
            if (byte == 'A') {
                reset_escape_state(parser);
                action_out->type = IK_INPUT_ARROW_UP;
                return OK(parser);
            }
            if (byte == 'B') {
                reset_escape_state(parser);
                action_out->type = IK_INPUT_ARROW_DOWN;
                return OK(parser);
            }
            if (byte == 'C') {
                reset_escape_state(parser);
                action_out->type = IK_INPUT_ARROW_RIGHT;
                return OK(parser);
            }
            if (byte == 'D') {
                reset_escape_state(parser);
                action_out->type = IK_INPUT_ARROW_LEFT;
                return OK(parser);
            }
        }

        // Check for delete: ESC [ 3 ~
        // Note: We know esc_buf[0] == '[' due to validation above
        if (parser->esc_len == 3 && parser->esc_buf[1] == '3' && byte == '~') {
            reset_escape_state(parser);
            action_out->type = IK_INPUT_DELETE;
            return OK(parser);
        }

        // Incomplete sequence - need more bytes
        action_out->type = IK_INPUT_UNKNOWN;
        return OK(parser);
    }

    // Check for ESC to start escape sequence
    if (byte == 0x1B) {  // ESC
        parser->in_escape = true;
        parser->esc_len = 0;
        action_out->type = IK_INPUT_UNKNOWN;
        return OK(parser);
    }

    // Handle control characters (except DEL)
    if (byte == '\n') {  // 0x0A - Newline
        action_out->type = IK_INPUT_NEWLINE;
        return OK(parser);
    }
    if (byte == 0x03) {  // Ctrl+C
        action_out->type = IK_INPUT_CTRL_C;
        return OK(parser);
    }

    // Handle printable ASCII and DEL (0x20-0x7F)
    unsigned char ubyte = (unsigned char)byte;
    if (ubyte >= 0x20 && ubyte <= 0x7F) {
        if (byte == 0x7F) {
            // DEL - Backspace
            action_out->type = IK_INPUT_BACKSPACE;
            return OK(parser);
        }
        // Printable ASCII (0x20-0x7E)
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = (uint32_t)ubyte;
        return OK(parser);
    }

    // Unknown/unhandled byte
    action_out->type = IK_INPUT_UNKNOWN;
    return OK(parser);
}
