// Input parser module - Convert raw bytes to semantic actions
#include "input.h"

#include "input_escape.h"
#include "input_xkb.h"
#include "logger.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

// Create input parser
ik_input_parser_t *ik_input_parser_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate parser
    ik_input_parser_t *parser = talloc_zero_(parent, sizeof(ik_input_parser_t));
    if (parser == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize fields (talloc_zero already set to 0, but be explicit)
    parser->esc_len = 0;
    parser->in_escape = false;
    parser->utf8_len = 0;
    parser->utf8_expected = 0;
    parser->in_utf8 = false;

    // Initialize xkbcommon for shifted key translation
    ik_input_xkb_init_state(parser);
    talloc_set_destructor(parser, ik_input_xkb_cleanup_destructor);

    return parser;
}

// Helper to reset UTF-8 sequence state
static void reset_utf8_state(ik_input_parser_t *parser)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE

    parser->in_utf8 = false;
    parser->utf8_len = 0;
    parser->utf8_expected = 0;
}

// Helper to decode complete UTF-8 sequence into codepoint
// Returns U+FFFD (replacement character) for invalid sequences
static uint32_t decode_utf8_sequence(const char *buf, size_t len)
{
    assert(buf != NULL);  // LCOV_EXCL_BR_LINE

    unsigned char b0 = (unsigned char)buf[0];
    unsigned char b1, b2, b3;
    uint32_t codepoint = 0;

    switch (len) {  // LCOV_EXCL_BR_LINE
        case 2:
            // 110xxxxx 10xxxxxx
            b1 = (unsigned char)buf[1];
            codepoint = ((uint32_t)(b0 & 0x1F) << 6) |
                        ((uint32_t)(b1 & 0x3F));
            break;
        case 3:
            // 1110xxxx 10xxxxxx 10xxxxxx
            b1 = (unsigned char)buf[1];
            b2 = (unsigned char)buf[2];
            codepoint = ((uint32_t)(b0 & 0x0F) << 12) |
                        ((uint32_t)(b1 & 0x3F) << 6) |
                        ((uint32_t)(b2 & 0x3F));
            break;
        case 4:
            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            b1 = (unsigned char)buf[1];
            b2 = (unsigned char)buf[2];
            b3 = (unsigned char)buf[3];
            codepoint = ((uint32_t)(b0 & 0x07) << 18) |
                        ((uint32_t)(b1 & 0x3F) << 12) |
                        ((uint32_t)(b2 & 0x3F) << 6) |
                        ((uint32_t)(b3 & 0x3F));
            break;
        default:  // LCOV_EXCL_LINE
            PANIC("UTF-8 parser state corruption: invalid length");  // LCOV_EXCL_LINE
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
static void parse_utf8_continuation(ik_input_parser_t *parser, char byte,
                                    ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    unsigned char ubyte = (unsigned char)byte;

    // Validate continuation byte (must be 10xxxxxx, i.e., 0x80-0xBF)
    if ((ubyte & 0xC0) != 0x80) {
        // Invalid continuation byte - reset and return unknown
        reset_utf8_state(parser);
        action_out->type = IK_INPUT_UNKNOWN;
        return;
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
        return;
    }

    // Still incomplete - need more bytes
    action_out->type = IK_INPUT_UNKNOWN;
}

// Parse single byte into action
void ik_input_parse_byte(ik_input_parser_t *parser, char byte,
                         ik_input_action_t *action_out)
{
    assert(parser != NULL);      // LCOV_EXCL_BR_LINE
    assert(action_out != NULL);  // LCOV_EXCL_BR_LINE

    // If we're in UTF-8 mode, handle continuation byte
    if (parser->in_utf8) {
        parse_utf8_continuation(parser, byte, action_out);
        return;
    }

    // If we're in escape sequence mode, handle escape byte
    if (parser->in_escape) {
        ik_input_parse_escape_sequence(parser, byte, action_out);
        return;
    }

    // Check for ESC to start escape sequence
    if (byte == 0x1B) {  // ESC
        parser->in_escape = true;
        parser->esc_len = 0;
        action_out->type = IK_INPUT_UNKNOWN;
        return;
    }

    // Handle control characters (except DEL)
    if (byte == '\t') {  // 0x09 (Tab) - completion trigger
        action_out->type = IK_INPUT_TAB;
        action_out->codepoint = 0;
        return;
    }
    if (byte == '\r') {  // 0x0D (CR) - Enter key submits
        action_out->type = IK_INPUT_NEWLINE;
        return;
    }
    if (byte == '\n') {  // 0x0A (LF) - Ctrl+J inserts newline without submitting
        action_out->type = IK_INPUT_INSERT_NEWLINE;
        return;
    }
    if (byte == 0x01) {  // Ctrl+A
        action_out->type = IK_INPUT_CTRL_A;
        return;
    }
    if (byte == 0x03) {  // Ctrl+C
        action_out->type = IK_INPUT_CTRL_C;
        return;
    }
    if (byte == 0x05) {  // Ctrl+E
        action_out->type = IK_INPUT_CTRL_E;
        return;
    }
    if (byte == 0x0B) {  // Ctrl+K
        action_out->type = IK_INPUT_CTRL_K;
        return;
    }
    if (byte == 0x0E) {  // Ctrl+N
        action_out->type = IK_INPUT_CTRL_N;
        return;
    }
    if (byte == 0x10) {  // Ctrl+P
        action_out->type = IK_INPUT_CTRL_P;
        return;
    }
    if (byte == 0x15) {  // Ctrl+U
        action_out->type = IK_INPUT_CTRL_U;
        return;
    }
    if (byte == 0x17) {  // Ctrl+W
        action_out->type = IK_INPUT_CTRL_W;
        return;
    }

    // Handle printable ASCII and DEL (0x20-0x7F)
    unsigned char ubyte = (unsigned char)byte;
    if (ubyte >= 0x20 && ubyte <= 0x7F) {
        if (byte == 0x7F) {
            // DEL - Backspace
            action_out->type = IK_INPUT_BACKSPACE;
            return;
        }
        // Printable ASCII (0x20-0x7E)
        action_out->type = IK_INPUT_CHAR;
        action_out->codepoint = (uint32_t)ubyte;
        return;
    }

    // Check for UTF-8 multi-byte sequence lead bytes
    if ((ubyte & 0xE0) == 0xC0) {
        // 2-byte sequence: 110xxxxx 10xxxxxx
        parser->in_utf8 = true;
        parser->utf8_buf[0] = byte;
        parser->utf8_len = 1;
        parser->utf8_expected = 2;
        action_out->type = IK_INPUT_UNKNOWN;  // Incomplete
        return;
    }
    if ((ubyte & 0xF0) == 0xE0) {
        // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
        parser->in_utf8 = true;
        parser->utf8_buf[0] = byte;
        parser->utf8_len = 1;
        parser->utf8_expected = 3;
        action_out->type = IK_INPUT_UNKNOWN;  // Incomplete
        return;
    }
    if ((ubyte & 0xF8) == 0xF0) {
        // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        parser->in_utf8 = true;
        parser->utf8_buf[0] = byte;
        parser->utf8_len = 1;
        parser->utf8_expected = 4;
        action_out->type = IK_INPUT_UNKNOWN;  // Incomplete
        return;
    }

    // Unknown/unhandled byte
    action_out->type = IK_INPUT_UNKNOWN;
}
