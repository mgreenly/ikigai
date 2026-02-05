#ifndef IK_INPUT_H
#define IK_INPUT_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <xkbcommon/xkbcommon.h>

#include "shared/error.h"

// Input action types representing semantic input events
typedef enum {
    IK_INPUT_CHAR,        // Regular character
    IK_INPUT_NEWLINE,     // Enter key (submit)
    IK_INPUT_INSERT_NEWLINE, // Ctrl+J (insert newline without submitting)
    IK_INPUT_BACKSPACE,   // Backspace key
    IK_INPUT_DELETE,      // Delete key
    IK_INPUT_ARROW_LEFT,  // Left arrow
    IK_INPUT_ARROW_RIGHT, // Right arrow
    IK_INPUT_ARROW_UP,    // Up arrow
    IK_INPUT_ARROW_DOWN,  // Down arrow
    IK_INPUT_PAGE_UP,     // Page Up key
    IK_INPUT_PAGE_DOWN,   // Page Down key
    IK_INPUT_SCROLL_UP,   // Mouse scroll up
    IK_INPUT_SCROLL_DOWN, // Mouse scroll down
    IK_INPUT_CTRL_A,      // Ctrl+A (beginning of line)
    IK_INPUT_CTRL_C,      // Ctrl+C (exit)
    IK_INPUT_CTRL_E,      // Ctrl+E (end of line)
    IK_INPUT_CTRL_K,      // Ctrl+K (kill to end of line)
    IK_INPUT_CTRL_N,      // Ctrl+N (history next - rel-05)
    IK_INPUT_CTRL_P,      // Ctrl+P (history previous - rel-05)
    IK_INPUT_CTRL_U,      // Ctrl+U (kill line)
    IK_INPUT_CTRL_W,      // Ctrl+W (delete word backward)
    IK_INPUT_TAB,         // Tab key (completion trigger)
    IK_INPUT_ESCAPE,      // Escape key (dismiss completion)
    IK_INPUT_NAV_PREV_SIBLING, // Ctrl+Left (previous sibling agent)
    IK_INPUT_NAV_NEXT_SIBLING, // Ctrl+Right (next sibling agent)
    IK_INPUT_NAV_PARENT,  // Ctrl+Up (parent agent)
    IK_INPUT_NAV_CHILD,   // Ctrl+Down (child agent)
    IK_INPUT_UNKNOWN      // Unrecognized sequence
} ik_input_action_type_t;

// Input action with associated data
typedef struct {
    ik_input_action_type_t type;
    uint32_t codepoint; // For IK_INPUT_CHAR
} ik_input_action_t;

// Reverse keymap: Unicode codepoint (0-127) -> X11 keycode
typedef struct {
    xkb_keycode_t keycodes[128];  // Index by ASCII codepoint
} ik_xkb_reverse_map_t;

// Input parser state for escape sequence buffering and UTF-8 decoding
typedef struct {
    char esc_buf[16];        // Escape sequence buffer
    size_t esc_len;          // Current escape sequence length
    bool in_escape;          // Currently parsing escape sequence
    char utf8_buf[4];        // UTF-8 byte buffer (max 4 bytes)
    size_t utf8_len;         // Current UTF-8 sequence length
    size_t utf8_expected;    // Expected total bytes for current UTF-8 sequence
    bool in_utf8;            // Currently parsing UTF-8 sequence
    // xkbcommon state for CSI u translation
    struct xkb_context *xkb_ctx;      // XKB context
    struct xkb_keymap *xkb_keymap;    // XKB keymap for current layout
    struct xkb_state *xkb_state;      // XKB state for modifier handling
    ik_xkb_reverse_map_t reverse_map; // Unicode -> keycode mapping
    xkb_mod_mask_t shift_mask;        // Shift modifier mask
    bool xkb_initialized;             // True if xkb state is ready
} ik_input_parser_t;

// Create input parser
ik_input_parser_t *ik_input_parser_create(void *parent);

// Parse single byte into action
void ik_input_parse_byte(ik_input_parser_t *parser, char byte, ik_input_action_t *action_out);

#endif // IK_INPUT_H
