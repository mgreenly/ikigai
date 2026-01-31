// Input escape sequence parsing
#include "input_escape.h"
#include "input_xkb.h"
#include <assert.h>


#include "poison.h"
static void reset_escape_state(ik_input_parser_t *parser)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE
    parser->in_escape = false;
    parser->esc_len = 0;
}

// Check if byte completes an unrecognized CSI sequence to discard
static bool is_discardable_csi_terminal(const ik_input_parser_t *parser, char byte)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE
    if (byte == 'm' && parser->esc_len >= 1) {  // LCOV_EXCL_BR_LINE
        return true;
    }
    if (parser->esc_len == 3 && byte == '~') {
        return true;
    }
    return false;
}

static bool parse_first_escape_byte(ik_input_parser_t *parser, char byte,
                                    ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE
    if (byte == '[') {
        return false;
    }
    if (byte == 0x1B) {
        reset_escape_state(parser);
        parser->in_escape = true;
        parser->esc_len = 0;
        action_out->type = IK_INPUT_ESCAPE;
        return true;
    }
    reset_escape_state(parser);
    action_out->type = IK_INPUT_UNKNOWN;
    return true;
}

// Arrow keys: ESC [ A/B/C/D and ESC [ 1 ; N A/B/C/D (with modifiers)
static bool parse_arrow_keys(ik_input_parser_t *parser, char byte,
                             ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE
    if (byte != 'A' && byte != 'B' && byte != 'C' && byte != 'D') {
        return false;
    }
    // Plain: ESC [ A/B/C/D
    if (parser->esc_len == 2) {
        reset_escape_state(parser);
        if (byte == 'A') action_out->type = IK_INPUT_ARROW_UP;
        else if (byte == 'B') action_out->type = IK_INPUT_ARROW_DOWN;
        else if (byte == 'C') action_out->type = IK_INPUT_ARROW_RIGHT;
        else action_out->type = IK_INPUT_ARROW_LEFT;
        return true;
    }
    // Modified: ESC [ 1 ; N A/B/C/D
    if (parser->esc_len >= 5 && parser->esc_buf[1] == '1' && parser->esc_buf[2] == ';') {
        int32_t mod = 0;
        for (size_t i = 3; i < parser->esc_len - 1; i++) {
            char c = parser->esc_buf[i];
            if (c >= '0' && c <= '9') mod = mod * 10 + (c - '0');
            else return false;
        }
        mod &= ~128;  // Mask NumLock
        if (mod == 1) {
            reset_escape_state(parser);
            if (byte == 'A') action_out->type = IK_INPUT_ARROW_UP;
            else if (byte == 'B') action_out->type = IK_INPUT_ARROW_DOWN;
            else if (byte == 'C') action_out->type = IK_INPUT_ARROW_RIGHT;
            else action_out->type = IK_INPUT_ARROW_LEFT;
            return true;
        }
        if (mod == 5) {  // Ctrl
            reset_escape_state(parser);
            if (byte == 'A') action_out->type = IK_INPUT_NAV_PARENT;
            else if (byte == 'B') action_out->type = IK_INPUT_NAV_CHILD;
            else if (byte == 'C') action_out->type = IK_INPUT_NAV_NEXT_SIBLING;
            else action_out->type = IK_INPUT_NAV_PREV_SIBLING;
            return true;
        }
    }
    return false;
}

// Mouse SGR: ESC [ < button ; col ; row M/m
static bool parse_mouse_sgr(ik_input_parser_t *parser, char byte,
                            ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE
    if (parser->esc_len < 2 || parser->esc_buf[1] != '<') return false;
    if (byte != 'M' && byte != 'm') return false;
    size_t btn_end = 2;
    while (btn_end < parser->esc_len && parser->esc_buf[btn_end] != ';') btn_end++;
    if (btn_end >= parser->esc_len) return false;
    if (btn_end - 2 == 2) {
        char b0 = parser->esc_buf[2], b1 = parser->esc_buf[3];
        if (b0 == '6' && b1 == '4') {  // LCOV_EXCL_BR_LINE
            reset_escape_state(parser);
            action_out->type = IK_INPUT_SCROLL_UP;
            return true;
        }
        if (b0 == '6' && b1 == '5') {  // LCOV_EXCL_BR_LINE
            reset_escape_state(parser);
            action_out->type = IK_INPUT_SCROLL_DOWN;
            return true;
        }
    }
    reset_escape_state(parser);
    action_out->type = IK_INPUT_UNKNOWN;
    return true;
}

// CSI u: ESC [ keycode ; modifiers u
static bool parse_csi_u_sequence(ik_input_parser_t *parser,
                                 ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE
    if (parser->esc_len < 3) return false;
    if (parser->esc_buf[parser->esc_len - 1] != 'u') return false;  // LCOV_EXCL_BR_LINE
    int32_t keycode = 0, modifiers = 1;
    size_t i = 1;
    // LCOV_EXCL_BR_START
    while (i < parser->esc_len && parser->esc_buf[i] >= '0' && parser->esc_buf[i] <= '9') {
        // LCOV_EXCL_BR_STOP
        keycode = keycode * 10 + (parser->esc_buf[i] - '0');
        i++;
    }
    // LCOV_EXCL_BR_START
    if (i < parser->esc_len && parser->esc_buf[i] == ';') {
        // LCOV_EXCL_BR_STOP
        i++;
        modifiers = 0;
        // LCOV_EXCL_BR_START
        while (i < parser->esc_len && parser->esc_buf[i] >= '0' && parser->esc_buf[i] <= '9') {
            // LCOV_EXCL_BR_STOP
            modifiers = modifiers * 10 + (parser->esc_buf[i] - '0');
            i++;
        }
    }
    modifiers &= ~128;  // Mask NumLock
    if (keycode > 50000) { action_out->type = IK_INPUT_UNKNOWN; return true; }
    if (keycode == 13) {
        action_out->type = (modifiers == 1) ? IK_INPUT_NEWLINE : IK_INPUT_INSERT_NEWLINE;
        return true;
    }
    // Ctrl+key combinations (modifiers == 5)
    if (modifiers == 5) {
        switch (keycode) {
            case 99: action_out->type = IK_INPUT_CTRL_C; return true;
            case 97: action_out->type = IK_INPUT_CTRL_A; return true;
            case 101: action_out->type = IK_INPUT_CTRL_E; return true;
            case 107: action_out->type = IK_INPUT_CTRL_K; return true;
            case 110: action_out->type = IK_INPUT_CTRL_N; return true;
            case 112: action_out->type = IK_INPUT_CTRL_P; return true;
            case 117: action_out->type = IK_INPUT_CTRL_U; return true;
            case 119: action_out->type = IK_INPUT_CTRL_W; return true;
        }
    }
    if (modifiers == 1) {
        if (keycode == 9) { action_out->type = IK_INPUT_TAB; return true; }
        if (keycode == 127) { action_out->type = IK_INPUT_BACKSPACE; return true; }
        if (keycode == 27) { action_out->type = IK_INPUT_ESCAPE; return true; }
        if (keycode >= 32 && keycode <= 126) {
            action_out->type = IK_INPUT_CHAR;
            action_out->codepoint = (uint32_t)keycode;
            return true;
        }
        if (keycode > 126 && keycode <= 0x10FFFF) {  // LCOV_EXCL_BR_LINE
            action_out->type = IK_INPUT_CHAR;
            action_out->codepoint = (uint32_t)keycode;
            return true;
        }
    }
    if (keycode >= 32 && keycode <= 126 && modifiers == 2) {
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = ik_input_xkb_translate_shifted_key(parser, (uint32_t)keycode);
        return true;
    }
    action_out->type = IK_INPUT_UNKNOWN;
    return true;
}

// Tilde sequences (ESC [ N ~), Home/End (ESC [ H/F, ESC [ 1 ; N H/F)
static bool parse_tilde_sequences(ik_input_parser_t *parser, char byte,
                                  ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE
    // ESC [ H/F
    if (parser->esc_len == 2 && (byte == 'H' || byte == 'F')) {
        reset_escape_state(parser);
        action_out->type = (byte == 'H') ? IK_INPUT_CTRL_A : IK_INPUT_CTRL_E;
        return true;
    }
    // ESC [ 1 ; N H/F (with modifier)
    if ((byte == 'H' || byte == 'F') && parser->esc_len >= 5 &&
        parser->esc_buf[1] == '1' && parser->esc_buf[2] == ';') {
        bool valid = true;
        for (size_t i = 3; i < parser->esc_len - 1 && valid; i++) {
            if (parser->esc_buf[i] < '0' || parser->esc_buf[i] > '9') valid = false;
        }
        if (valid) {
            reset_escape_state(parser);
            action_out->type = (byte == 'H') ? IK_INPUT_CTRL_A : IK_INPUT_CTRL_E;
            return true;
        }
    }
    if (byte != '~') return false;
    int32_t key = 0;
    for (size_t i = 1; i < parser->esc_len - 1 && parser->esc_buf[i] != ';'; i++) {
        char c = parser->esc_buf[i];
        if (c >= '0' && c <= '9') key = key * 10 + (c - '0');
        else return false;
    }
    if (key == 0) return false;
    reset_escape_state(parser);
    switch (key) {
        case 1: action_out->type = IK_INPUT_CTRL_A; return true;
        case 3: action_out->type = IK_INPUT_DELETE; return true;
        case 4: action_out->type = IK_INPUT_CTRL_E; return true;
        case 5: action_out->type = IK_INPUT_PAGE_UP; return true;
        case 6: action_out->type = IK_INPUT_PAGE_DOWN; return true;
        default: action_out->type = IK_INPUT_UNKNOWN; return true;
    }
}

void ik_input_parse_escape_sequence(ik_input_parser_t *parser, char byte,
                                    ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE
    parser->esc_buf[parser->esc_len++] = byte;
    parser->esc_buf[parser->esc_len] = '\0';
    if (parser->esc_len >= sizeof(parser->esc_buf) - 1) {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return;
    }
    if (parser->esc_len == 1 && parse_first_escape_byte(parser, byte, action_out)) return;
    if (parse_arrow_keys(parser, byte, action_out)) return;
    if (parse_mouse_sgr(parser, byte, action_out)) return;
    if (parse_tilde_sequences(parser, byte, action_out)) return;
    if (byte == 'u') {
        if (parse_csi_u_sequence(parser, action_out)) { reset_escape_state(parser); return; }
        action_out->type = IK_INPUT_UNKNOWN;
        reset_escape_state(parser);
        return;
    }
    if (is_discardable_csi_terminal(parser, byte)) {
        reset_escape_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return;
    }
    // GCC 14.2.0 bug: branch coverage not recorded despite tests
    if (parser->esc_len == 2 && byte >= 'A' && byte <= 'Z') {  // LCOV_EXCL_BR_LINE
        reset_escape_state(parser);  // LCOV_EXCL_LINE
        action_out->type = IK_INPUT_UNKNOWN;  // LCOV_EXCL_LINE
        return;  // LCOV_EXCL_LINE
    }
    action_out->type = IK_INPUT_UNKNOWN;
}
