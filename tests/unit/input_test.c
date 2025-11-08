// Input parser module unit tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/input.h"
#include "../../src/error.h"
#include "../test_utils.h"

// Test: successful input parser creation
START_TEST(test_input_parser_create) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;

    res_t res = ik_input_parser_create(ctx, &parser);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(parser);
    ck_assert_uint_eq(parser->esc_len, 0);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST
// Test: OOM scenario
START_TEST(test_input_parser_create_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;

    // Fail on first allocation
    oom_test_fail_next_alloc();
    res_t res = ik_input_parser_create(ctx, &parser);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    ck_assert_ptr_null(parser);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST

#ifndef NDEBUG
// Test: ik_input_parser_create with NULL parent asserts
START_TEST(test_input_parser_create_null_parent_asserts)
{
    ik_input_parser_t *parser = NULL;
    ik_input_parser_create(NULL, &parser);
}

END_TEST
// Test: ik_input_parser_create with NULL parser_out asserts
START_TEST(test_input_parser_create_null_parser_out_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_create(ctx, NULL);
    talloc_free(ctx);
}

END_TEST
// Test: ik_input_parse_byte with NULL parser asserts
START_TEST(test_input_parse_byte_null_parser_asserts)
{
    ik_input_action_t action = {0};
    ik_input_parse_byte(NULL, 'a', &action);
}

END_TEST
// Test: ik_input_parse_byte with NULL action_out asserts
START_TEST(test_input_parse_byte_null_action_out_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_parser_create(ctx, &parser);
    ik_input_parse_byte(parser, 'a', NULL);
    talloc_free(ctx);
}

END_TEST
#endif

// Test: parse regular ASCII characters
START_TEST(test_input_parse_regular_char)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse 'a'
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    // Parse 'Z'
    res = ik_input_parse_byte(parser, 'Z', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'Z');

    // Parse '5'
    res = ik_input_parse_byte(parser, '5', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, '5');

    talloc_free(ctx);
}

END_TEST
// Test: parse non-printable characters return UNKNOWN
START_TEST(test_input_parse_nonprintable)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse non-printable character below 0x20 (except recognized control chars)
    res = ik_input_parse_byte(parser, 0x01, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Parse non-printable character above 0x7E (high byte)
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: parse newline character
START_TEST(test_input_parse_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse '\n' (0x0A)
    res = ik_input_parse_byte(parser, '\n', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: parse carriage return (Enter key in raw mode)
START_TEST(test_input_parse_carriage_return)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse '\r' (0x0D) - Enter key sends this in raw mode
    res = ik_input_parse_byte(parser, '\r', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: parse backspace character
START_TEST(test_input_parse_backspace)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse DEL (0x7F)
    res = ik_input_parse_byte(parser, 0x7F, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_BACKSPACE);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+C character
START_TEST(test_input_parse_ctrl_c)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse Ctrl+C (0x03)
    res = ik_input_parse_byte(parser, 0x03, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_C);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow up escape sequence byte by byte
START_TEST(test_input_parse_arrow_up)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC (0x1B) - incomplete sequence
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Should be in escape mode

    // Parse '[' - still incomplete
    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Parse 'A' - complete sequence for arrow up
    res = ik_input_parse_byte(parser, 'A', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);
    ck_assert(!parser->in_escape); // Should exit escape mode

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow down escape sequence
START_TEST(test_input_parse_arrow_down)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse full sequence \x1b[B
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, 'B', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_DOWN);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow left escape sequence
START_TEST(test_input_parse_arrow_left)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse full sequence \x1b[D
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, 'D', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_LEFT);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow right escape sequence
START_TEST(test_input_parse_arrow_right)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse full sequence \x1b[C
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, 'C', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_RIGHT);

    talloc_free(ctx);
}

END_TEST
// Test: parse delete escape sequence
START_TEST(test_input_parse_delete)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse full sequence \x1b[3~
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '3', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '~', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_DELETE);

    talloc_free(ctx);
}

END_TEST
// Test: parse invalid escape sequence resets parser
START_TEST(test_input_parse_invalid_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Start escape sequence
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Invalid sequence: ESC followed by 'x' (not '[')
    res = ik_input_parse_byte(parser, 'x', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset

    // Verify parser can handle next input correctly (regular char)
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: buffer overflow protection
START_TEST(test_input_parse_buffer_overflow)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Start escape sequence
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Send '[' to start a valid-looking sequence
    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Now send 13 more bytes to overflow the buffer (16 byte buffer - 1 for '[' - 1 for null = 14 more)
    // Buffer size is 16, we've used 1 byte ('['), so we need 14 more to reach the limit
    for (int32_t i = 0; i < 14; i++) {
        res = ik_input_parse_byte(parser, '1', &action);
        ck_assert(is_ok(&res));
        if (i < 13) {
            ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
            ck_assert(parser->in_escape);
        }
    }

    // After 14 bytes (total 15 with '['), buffer should overflow and reset
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    // Verify parser can handle next input correctly
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: invalid delete-like sequence (ESC [ X ~ where X is not '3')
START_TEST(test_input_parse_invalid_delete_like_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC [ 5 ~ (not a valid delete sequence)
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '5', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape mode

    res = ik_input_parse_byte(parser, '~', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset - complete but unrecognized sequence

    talloc_free(ctx);
}

END_TEST
// Test: unrecognized arrow-like sequence (ESC [ Z)
START_TEST(test_input_parse_incomplete_arrow_like_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC [ Z (complete but unrecognized arrow-like sequence)
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, 'Z', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset - complete but unrecognized sequence

    talloc_free(ctx);
}

END_TEST
// Test: incomplete delete-like sequence (ESC [ 3 X where X is not '~')
START_TEST(test_input_parse_incomplete_delete_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC [ 3 X where X is not '~'
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '3', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, 'A', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape, waiting for more

    talloc_free(ctx);
}

END_TEST
// Test: parse 2-byte UTF-8 character (é = U+00E9 = 0xC3 0xA9)
START_TEST(test_input_parse_utf8_2byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse é (0xC3 0xA9) - 2 byte UTF-8
    // First byte (lead byte)
    res = ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete sequence

    // Second byte (continuation byte)
    res = ik_input_parse_byte(parser, (char)0xA9, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x00E9); // U+00E9 (é)

    talloc_free(ctx);
}

END_TEST
// Test: parse 3-byte UTF-8 character (☃ = U+2603 = 0xE2 0x98 0x83)
START_TEST(test_input_parse_utf8_3byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ☃ (0xE2 0x98 0x83) - 3 byte UTF-8
    // First byte (lead byte)
    res = ik_input_parse_byte(parser, (char)0xE2, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    // Second byte (continuation)
    res = ik_input_parse_byte(parser, (char)0x98, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Still incomplete

    // Third byte (continuation)
    res = ik_input_parse_byte(parser, (char)0x83, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x2603); // U+2603 (☃)

    talloc_free(ctx);
}

END_TEST
// Test: parse 4-byte UTF-8 character (🎉 = U+1F389 = 0xF0 0x9F 0x8E 0x89)
START_TEST(test_input_parse_utf8_4byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse 🎉 (0xF0 0x9F 0x8E 0x89) - 4 byte UTF-8
    // First byte (lead byte)
    res = ik_input_parse_byte(parser, (char)0xF0, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    // Second byte (continuation)
    res = ik_input_parse_byte(parser, (char)0x9F, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Still incomplete

    // Third byte (continuation)
    res = ik_input_parse_byte(parser, (char)0x8E, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Still incomplete

    // Fourth byte (continuation)
    res = ik_input_parse_byte(parser, (char)0x89, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x1F389); // U+1F389 (🎉)

    talloc_free(ctx);
}

END_TEST
// Test: incomplete UTF-8 sequence (only lead byte)
START_TEST(test_input_parse_utf8_incomplete)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse only lead byte of 2-byte sequence
    res = ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete
    ck_assert(parser->in_utf8); // Should be in UTF-8 mode

    talloc_free(ctx);
}

END_TEST
// Test: invalid UTF-8 sequence (invalid continuation byte)
START_TEST(test_input_parse_utf8_invalid)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Start 2-byte sequence
    res = ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_utf8);

    // Send invalid continuation byte (not 10xxxxxx pattern)
    // Using 0xFF which is 11111111 (not a valid continuation byte)
    res = ik_input_parse_byte(parser, (char)0xFF, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_utf8); // Should reset

    // Verify parser can handle next input correctly
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: unrecognized escape sequence with tilde (e.g., Insert key)
START_TEST(test_input_parse_unrecognized_tilde_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse Insert key sequence: ESC [ 2 ~
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    res = ik_input_parse_byte(parser, '2', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // When we get '~', the sequence is complete but unrecognized
    // Parser should reset and be ready for next input
    res = ik_input_parse_byte(parser, '~', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should have reset

    // Verify parser can handle next input correctly
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: unrecognized 2-char escape sequence with letter
START_TEST(test_input_parse_unrecognized_letter_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse unrecognized sequence: ESC [ Z (shift-tab in some terminals)
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // When we get a letter that's not A/B/C/D, sequence is complete but unrecognized
    // Parser should reset
    res = ik_input_parse_byte(parser, 'Z', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should have reset

    // Verify parser can handle next input correctly
    res = ik_input_parse_byte(parser, 'b', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'b');

    talloc_free(ctx);
}

END_TEST

// Test: incomplete escape sequence with non-letter at esc_len==2
START_TEST(test_input_parse_escape_non_letter)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC [ followed by a digit (not a letter)
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Send a digit - not A-Z, so doesn't match the unrecognized letter check
    // This is still incomplete, waiting for more bytes (could be ESC [ 2 ~)
    res = ik_input_parse_byte(parser, '1', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape mode

    talloc_free(ctx);
}

END_TEST

// Test: escape sequence with character between Z and end of ASCII
START_TEST(test_input_parse_escape_char_above_Z)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC [ followed by a character > 'Z' (e.g., ']' which is 0x5D)
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Send ']' (0x5D) - this is >= 'A' (0x41) but > 'Z' (0x5A)
    // Should not match the A-Z check, stays in escape mode
    res = ik_input_parse_byte(parser, ']', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape mode

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_suite(void)
{
    Suite *s = suite_create("Input");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_input_parser_create);
    tcase_add_test(tc_core, test_input_parser_create_oom);
    tcase_add_test(tc_core, test_input_parse_regular_char);
    tcase_add_test(tc_core, test_input_parse_nonprintable);
    tcase_add_test(tc_core, test_input_parse_newline);
    tcase_add_test(tc_core, test_input_parse_carriage_return);
    tcase_add_test(tc_core, test_input_parse_backspace);
    tcase_add_test(tc_core, test_input_parse_ctrl_c);
    tcase_add_test(tc_core, test_input_parse_arrow_up);
    tcase_add_test(tc_core, test_input_parse_arrow_down);
    tcase_add_test(tc_core, test_input_parse_arrow_left);
    tcase_add_test(tc_core, test_input_parse_arrow_right);
    tcase_add_test(tc_core, test_input_parse_delete);
    tcase_add_test(tc_core, test_input_parse_invalid_escape);
    tcase_add_test(tc_core, test_input_parse_buffer_overflow);
    tcase_add_test(tc_core, test_input_parse_invalid_delete_like_sequence);
    tcase_add_test(tc_core, test_input_parse_incomplete_arrow_like_sequence);
    tcase_add_test(tc_core, test_input_parse_incomplete_delete_sequence);
    tcase_add_test(tc_core, test_input_parse_utf8_2byte);
    tcase_add_test(tc_core, test_input_parse_utf8_3byte);
    tcase_add_test(tc_core, test_input_parse_utf8_4byte);
    tcase_add_test(tc_core, test_input_parse_utf8_incomplete);
    tcase_add_test(tc_core, test_input_parse_utf8_invalid);
    tcase_add_test(tc_core, test_input_parse_unrecognized_tilde_sequence);
    tcase_add_test(tc_core, test_input_parse_unrecognized_letter_sequence);
    tcase_add_test(tc_core, test_input_parse_escape_non_letter);
    tcase_add_test(tc_core, test_input_parse_escape_char_above_Z);

#ifndef NDEBUG
    tcase_add_test_raise_signal(tc_core, test_input_parser_create_null_parent_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_input_parser_create_null_parser_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_input_parse_byte_null_parser_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_input_parse_byte_null_action_out_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
