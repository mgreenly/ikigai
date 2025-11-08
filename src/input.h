#ifndef IK_INPUT_H
#define IK_INPUT_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "error.h"

// Input action types representing semantic input events
typedef enum {
    IK_INPUT_CHAR,        // Regular character
    IK_INPUT_NEWLINE,     // Enter key
    IK_INPUT_BACKSPACE,   // Backspace key
    IK_INPUT_DELETE,      // Delete key
    IK_INPUT_ARROW_LEFT,  // Left arrow
    IK_INPUT_ARROW_RIGHT, // Right arrow
    IK_INPUT_ARROW_UP,    // Up arrow
    IK_INPUT_ARROW_DOWN,  // Down arrow
    IK_INPUT_CTRL_C,      // Ctrl+C (exit)
    IK_INPUT_UNKNOWN      // Unrecognized sequence
} ik_input_action_type_t;

// Input action with associated data
typedef struct {
    ik_input_action_type_t type;
    uint32_t codepoint; // For IK_INPUT_CHAR
} ik_input_action_t;

// Input parser state for escape sequence buffering
typedef struct {
    char esc_buf[16];    // Escape sequence buffer
    size_t esc_len;      // Current escape sequence length
    bool in_escape;      // Currently parsing escape sequence
} ik_input_parser_t;

// Create input parser
res_t ik_input_parser_create(void *parent, ik_input_parser_t **parser_out);

// Parse single byte into action
res_t ik_input_parse_byte(ik_input_parser_t *parser, char byte,
                           ik_input_action_t *action_out);

#endif // IK_INPUT_H
