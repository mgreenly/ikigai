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
    parser->utf8_len = 0;
    parser->utf8_expected = 0;
    parser->in_utf8 = false;

    *parser_out = parser;
    return OK(parser);
}

// Helper to reset escape sequence state
static void reset_escape_state(ik_input_parser_t *parser)
{
    parser->in_escape = false;
    parser->esc_len = 0;
}

// Helper to reset UTF-8 sequence state
static void reset_utf8_state(ik_input_parser_t *parser)
{
    parser->in_utf8 = false;
    parser->utf8_len = 0;
    parser->utf8_expected = 0;
}

// Helper to decode complete UTF-8 sequence into codepoint
// Returns U+FFFD (replacement character) for invalid sequences
static uint32_t decode_utf8_sequence(const char *buf, size_t len)
{
    assert(len >= 1 && len <= 4);  // LCOV_EXCL_BR_LINE

    unsigned char b0 = (unsigned char)buf[0];
    uint32_t codepoint = 0;

    if (len == 2) {
        // 110xxxxx 10xxxxxx
        unsigned char b1 = (unsigned char)buf[1];
        codepoint = ((uint32_t)(b0 & 0x1F) << 6) |
                    ((uint32_t)(b1 & 0x3F));
    } else if (len == 3) {
        // 1110xxxx 10xxxxxx 10xxxxxx
        unsigned char b1 = (unsigned char)buf[1];
        unsigned char b2 = (unsigned char)buf[2];
        codepoint = ((uint32_t)(b0 & 0x0F) << 12) |
                    ((uint32_t)(b1 & 0x3F) << 6) |
                    ((uint32_t)(b2 & 0x3F));
    } else if (len == 4) {  // LCOV_EXCL_BR_LINE
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        unsigned char b1 = (unsigned char)buf[1];
        unsigned char b2 = (unsigned char)buf[2];
        unsigned char b3 = (unsigned char)buf[3];
        codepoint = ((uint32_t)(b0 & 0x07) << 18) |
                    ((uint32_t)(b1 & 0x3F) << 12) |
                    ((uint32_t)(b2 & 0x3F) << 6) |
                    ((uint32_t)(b3 & 0x3F));
    } else {
        // Should never reach here due to assertion
        return 0xFFFD;  // LCOV_EXCL_LINE
    }

    // Validate codepoint (reject overlong encodings, surrogates, out-of-range)

    // Reject overlong encodings (RFC 3629)
    if (len == 2 && codepoint < 0x80) {
        return 0xFFFD;  // Overlong 2-byte encoding
    }
    if (len == 3 && codepoint < 0x800) {
        return 0xFFFD;  // Overlong 3-byte encoding
    }
    if (len == 4 && codepoint < 0x10000) {
        return 0xFFFD;  // Overlong 4-byte encoding
    }

    // Reject UTF-16 surrogates (U+D800 to U+DFFF)
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        return 0xFFFD;  // Surrogate codepoint
    }

    // Reject codepoints beyond valid Unicode range (> U+10FFFF)
    if (codepoint > 0x10FFFF) {
        return 0xFFFD;  // Out-of-range codepoint
    }

    return codepoint;
}

// Handle UTF-8 continuation byte
static res_t parse_utf8_continuation(ik_input_parser_t *parser, char byte,
                                     ik_input_action_t *action_out)
{
    unsigned char ubyte = (unsigned char)byte;

    // Validate continuation byte (must be 10xxxxxx, i.e., 0x80-0xBF)
    if ((ubyte & 0xC0) != 0x80) {
        // Invalid continuation byte - reset and return unknown
        reset_utf8_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return OK(parser);
    }

    // Buffer the continuation byte
    parser->utf8_buf[parser->utf8_len++] = byte;

    // Check if we have all expected bytes
    if (parser->utf8_len == parser->utf8_expected) {
        // Decode the complete sequence
        uint32_t codepoint = decode_utf8_sequence(parser->utf8_buf, parser->utf8_len);
        reset_utf8_state(parser);
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = codepoint;
        return OK(parser);
    }

    // Still incomplete - need more bytes
    action_out->type = IK_INPUT_UNKNOWN;
    return OK(parser);
}

// Handle escape sequence byte
static res_t parse_escape_sequence(ik_input_parser_t *parser, char byte,
                                   ik_input_action_t *action_out)
{
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

    // Check for other complete sequences we don't recognize
    // Pattern: ESC [ digit ~ (e.g., Insert=2~, Home=1~, End=4~, etc.)
    if (parser->esc_len == 3 && byte == '~') {
        // Complete but unrecognized sequence - reset parser
        reset_escape_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return OK(parser);
    }

    // Check for unrecognized 2-character sequences at esc_len == 2
    // If we have ESC [ <letter> and it's not A/B/C/D, it's complete but unknown
    if (parser->esc_len == 2 && byte >= 'A' && byte <= 'Z') {
        // Complete but unrecognized sequence - reset parser
        reset_escape_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return OK(parser);
    }

    // Incomplete sequence - need more bytes
    action_out->type = IK_INPUT_UNKNOWN;
    return OK(parser);
}

// Parse single byte into action
res_t ik_input_parse_byte(ik_input_parser_t *parser, char byte,
                          ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // If we're in UTF-8 mode, handle continuation byte
    if (parser->in_utf8) {
        return parse_utf8_continuation(parser, byte, action_out);
    }

    // If we're in escape sequence mode, handle escape byte
    if (parser->in_escape) {
        return parse_escape_sequence(parser, byte, action_out);
    }

    // Check for ESC to start escape sequence
    if (byte == 0x1B) {  // ESC
        parser->in_escape = true;
        parser->esc_len = 0;
        action_out->type = IK_INPUT_UNKNOWN;
        return OK(parser);
    }

    // Handle control characters (except DEL)
    if (byte == '\n' || byte == '\r') {  // 0x0A (LF) or 0x0D (CR) - Newline
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

    // Check for UTF-8 multi-byte sequence lead bytes
    if ((ubyte & 0xE0) == 0xC0) {
        // 2-byte sequence: 110xxxxx 10xxxxxx
        parser->in_utf8 = true;
        parser->utf8_buf[0] = byte;
        parser->utf8_len = 1;
        parser->utf8_expected = 2;
        action_out->type = IK_INPUT_UNKNOWN;  // Incomplete
        return OK(parser);
    }
    if ((ubyte & 0xF0) == 0xE0) {
        // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
        parser->in_utf8 = true;
        parser->utf8_buf[0] = byte;
        parser->utf8_len = 1;
        parser->utf8_expected = 3;
        action_out->type = IK_INPUT_UNKNOWN;  // Incomplete
        return OK(parser);
    }
    if ((ubyte & 0xF8) == 0xF0) {
        // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        parser->in_utf8 = true;
        parser->utf8_buf[0] = byte;
        parser->utf8_len = 1;
        parser->utf8_expected = 4;
        action_out->type = IK_INPUT_UNKNOWN;  // Incomplete
        return OK(parser);
    }

    // Unknown/unhandled byte
    action_out->type = IK_INPUT_UNKNOWN;
    return OK(parser);
}
