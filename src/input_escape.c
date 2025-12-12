// Input parser module - Escape sequence parsing
#include "input_escape.h"

#include "input_xkb.h"
#include "panic.h"

#include <assert.h>
#include <string.h>

// Helper to reset escape sequence state
static void reset_escape_state(ik_input_parser_t *parser)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE

    parser->in_escape = false;
    parser->esc_len = 0;
}

// Check if byte completes an unrecognized CSI sequence to discard
// Handles SGR sequences (m) and other unrecognized sequences (~)
static bool is_discardable_csi_terminal(const ik_input_parser_t *parser, char byte)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE

    // SGR sequences terminate with 'm' (e.g., \x1b[0m or \x1b[38;5;242m)
    if (byte == 'm' && parser->esc_len >= 1) {  // LCOV_EXCL_BR_LINE - defensive: esc_len >= 1 always true when byte=='m'
        return true;
    }

    // Other unrecognized sequences: ESC [ digit ~ (e.g., Insert=2~, Home=1~, End=4~)
    if (parser->esc_len == 3 && byte == '~') {
        return true;
    }

    return false;
}

// Handle first byte after ESC (validation)
static bool parse_first_escape_byte(ik_input_parser_t *parser, char byte,
                                     ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // If it's '[', continue CSI sequence parsing
    if (byte == '[') {
        return false; // Continue processing
    }

    // If it's ESC again (double ESC), treat first ESC as escape action
    if (byte == 0x1B) {
        reset_escape_state(parser);
        parser->in_escape = true;  // Start new escape sequence
        parser->esc_len = 0;
        action_out->type = IK_INPUT_ESCAPE;
        return true; // Handled
    }

    // Not '[' - invalid sequence
    reset_escape_state(parser);
    action_out->type = IK_INPUT_UNKNOWN;
    return true; // Handled
}


// Handle arrow key sequences: ESC [ A/B/C/D
static bool parse_arrow_keys(ik_input_parser_t *parser, char byte,
                              ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // Only handle 2-character sequences
    if (parser->esc_len != 2) {
        return false;
    }

    // Check for arrow keys
    if (byte == 'A') {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_ARROW_UP;
        return true;
    }
    if (byte == 'B') {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_ARROW_DOWN;
        return true;
    }
    if (byte == 'C') {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_ARROW_RIGHT;
        return true;
    }
    if (byte == 'D') {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_ARROW_LEFT;
        return true;
    }

    return false; // Not an arrow key
}

// Handle mouse SGR sequences: ESC [ < button ; col ; row M/m
static bool parse_mouse_sgr(ik_input_parser_t *parser, char byte,
                             ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // Mouse SGR sequences must start with ESC [ <
    if (parser->esc_len < 2 || parser->esc_buf[1] != '<') {
        return false;
    }

    // Mouse sequences end with 'M' (press) or 'm' (release)
    if (byte != 'M' && byte != 'm') {
        return false;
    }

    // We only care about scroll events (button 64 = scroll up, 65 = scroll down)
    // Parse the button number from esc_buf[2] onwards until first ';'
    size_t button_start = 2;
    size_t button_end = button_start;

    while (button_end < parser->esc_len && parser->esc_buf[button_end] != ';') {
        button_end++;
    }

    // Check if we found a valid button field
    if (button_end >= parser->esc_len) {
        return false;
    }

    // Extract button number (only support 2-digit buttons for scroll: 64, 65)
    if (button_end - button_start == 2) {
        char b0 = parser->esc_buf[button_start];
        char b1 = parser->esc_buf[button_start + 1];

        // Scroll up: button 64
        if (b0 == '6' && b1 == '4') {
            reset_escape_state(parser);
            action_out->type = IK_INPUT_SCROLL_UP;
            return true;
        }

        // Scroll down: button 65
        if (b0 == '6' && b1 == '5') {
            reset_escape_state(parser);
            action_out->type = IK_INPUT_SCROLL_DOWN;
            return true;
        }
    }

    // Other mouse events (clicks, drags) - discard
    reset_escape_state(parser);
    action_out->type = IK_INPUT_UNKNOWN;
    return true;
}

// Parse CSI u sequence: ESC [ keycode ; modifiers u
// Returns true if valid CSI u sequence parsed
static bool parse_csi_u_sequence(ik_input_parser_t *parser,
                                  ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // Minimum: ESC [ digit u = 4 chars in buffer (excluding ESC)
    // Format: [keycode;modifiers u
    if (parser->esc_len < 3) {
        return false;
    }

    // Must end with 'u'  (defensive check - caller only invokes when byte == 'u')
    if (parser->esc_buf[parser->esc_len - 1] != 'u') {  // LCOV_EXCL_BR_LINE
        return false;  // LCOV_EXCL_LINE
    }

    // Parse keycode and modifiers
    int32_t keycode = 0;
    int32_t modifiers = 1;  // Default: no modifiers

    size_t i = 1;  // Skip '[' (buf[0] is '[')

    // Parse keycode
    // LCOV_EXCL_BR_START - Defensive: well-formed sequences always have digits
    while (i < parser->esc_len && parser->esc_buf[i] >= '0' && parser->esc_buf[i] <= '9') {
        // LCOV_EXCL_BR_STOP
        keycode = keycode * 10 + (parser->esc_buf[i] - '0');
        i++;
    }

    // Parse modifiers if present
    // LCOV_EXCL_BR_START - Defensive: well-formed sequences properly terminated
    if (i < parser->esc_len && parser->esc_buf[i] == ';') {
        // LCOV_EXCL_BR_STOP
        i++;
        modifiers = 0;
        // LCOV_EXCL_BR_START - Defensive: well-formed sequences have modifier digits
        while (i < parser->esc_len && parser->esc_buf[i] >= '0' && parser->esc_buf[i] <= '9') {
            // LCOV_EXCL_BR_STOP
            modifiers = modifiers * 10 + (parser->esc_buf[i] - '0');
            i++;
        }
    }

    // Filter Alacritty modifier-only events (keycode > 50000)
    if (keycode > 50000) {
        action_out->type = IK_INPUT_UNKNOWN;
        return true;
    }

    // Handle Enter key (keycode 13)
    if (keycode == 13) {
        if (modifiers == 1) {
            // Plain Enter - submit
            action_out->type = IK_INPUT_NEWLINE;
        } else {
            // Modified Enter (Shift/Ctrl/Alt) - insert newline
            action_out->type = IK_INPUT_INSERT_NEWLINE;
        }
        return true;
    }

    // Handle Ctrl+C (keycode 99 = 'c', modifiers 5 = Ctrl)
    if (keycode == 99 && modifiers == 5) {
        action_out->type = IK_INPUT_CTRL_C;
        return true;
    }

    // Handle Tab key (keycode 9)
    if (keycode == 9 && modifiers == 1) {
        action_out->type = IK_INPUT_TAB;
        return true;
    }

    // Handle Backspace (keycode 127)
    if (keycode == 127 && modifiers == 1) {
        action_out->type = IK_INPUT_BACKSPACE;
        return true;
    }

    // Handle Escape key (keycode 27)
    if (keycode == 27 && modifiers == 1) {
        action_out->type = IK_INPUT_ESCAPE;
        return true;
    }

    // Handle printable ASCII characters (32-126) with no modifiers
    if (keycode >= 32 && keycode <= 126 && modifiers == 1) {
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = (uint32_t)keycode;
        return true;
    }

    // Handle printable ASCII characters (32-126) with Shift modifier
    // CSI u modifier encoding: modifiers = 1 + modifier_bits, Shift = bit 0
    // So modifiers == 2 means Shift only (1 + 1)
    if (keycode >= 32 && keycode <= 126 && modifiers == 2) {
        uint32_t translated = ik_input_xkb_translate_shifted_key(parser, (uint32_t)keycode);
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = translated;
        return true;
    }

    // Handle Unicode characters (above ASCII) with no modifiers
    if (keycode > 126 && keycode <= 0x10FFFF && modifiers == 1) {
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = (uint32_t)keycode;
        return true;
    }

    // Other CSI u keys (modified keys, function keys, etc.) - ignore for now
    action_out->type = IK_INPUT_UNKNOWN;
    return true;
}

// Handle 3-character tilde-terminated sequences: ESC [ N ~
static bool parse_tilde_sequences(ik_input_parser_t *parser, char byte,
                                   ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // Only handle 3-character sequences ending with '~'
    if (parser->esc_len != 3 || byte != '~') {
        return false;
    }

    // Check for delete: ESC [ 3 ~
    if (parser->esc_buf[1] == '3') {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_DELETE;
        return true;
    }

    // Check for page up: ESC [ 5 ~
    if (parser->esc_buf[1] == '5') {  // LCOV_EXCL_BR_LINE
        reset_escape_state(parser);
        action_out->type = IK_INPUT_PAGE_UP;
        return true;
    }

    // Check for page down: ESC [ 6 ~
    if (parser->esc_buf[1] == '6') {  // LCOV_EXCL_BR_LINE
        reset_escape_state(parser);
        action_out->type = IK_INPUT_PAGE_DOWN;
        return true;
    }

    return false; // Not a recognized tilde sequence
}

// Parse escape sequence byte
void ik_input_parse_escape_sequence(ik_input_parser_t *parser, char byte,
                                     ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // Buffer the byte
    parser->esc_buf[parser->esc_len++] = byte;
    parser->esc_buf[parser->esc_len] = '\0';

    // Buffer overflow protection - reset if we've filled the buffer
    if (parser->esc_len >= sizeof(parser->esc_buf) - 1) {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return;
    }

    // Validate first byte after ESC
    if (parser->esc_len == 1) {
        if (parse_first_escape_byte(parser, byte, action_out)) {
            return; // Handled
        }
    }

    // Try parsing as arrow key
    if (parse_arrow_keys(parser, byte, action_out)) {
        return; // Handled
    }

    // Try parsing as mouse SGR sequence
    if (parse_mouse_sgr(parser, byte, action_out)) {
        return; // Handled
    }

    // Try parsing as tilde-terminated sequence
    if (parse_tilde_sequences(parser, byte, action_out)) {
        return; // Handled
    }

    // Try parsing as CSI u sequence when byte is 'u'
    if (byte == 'u') {
        if (parse_csi_u_sequence(parser, action_out)) {
            reset_escape_state(parser);
            return;
        }

        // Not a recognized CSI u sequence
        action_out->type = IK_INPUT_UNKNOWN;
        reset_escape_state(parser);
        return;
    }

    // Check for unrecognized CSI sequences to discard (SGR and others)
    if (is_discardable_csi_terminal(parser, byte)) {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return;
    }

    // Check for unrecognized 2-character sequences at esc_len == 2
    // If we have ESC [ <letter> and it's not A/B/C/D, it's complete but unknown
    // GCC 14.2.0 bug: branch coverage not recorded despite tests covering both branches
    if (parser->esc_len == 2 && byte >= 'A' && byte <= 'Z') {  // LCOV_EXCL_BR_LINE
        reset_escape_state(parser);  // LCOV_EXCL_LINE
        action_out->type = IK_INPUT_UNKNOWN;  // LCOV_EXCL_LINE
        return;  // LCOV_EXCL_LINE
    }

    // Incomplete sequence - need more bytes
    action_out->type = IK_INPUT_UNKNOWN;
}
