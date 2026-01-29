// Input parser module unit tests - Escape sequence tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

// Test: parse arrow up escape sequence byte by byte
START_TEST(test_input_parse_arrow_up) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC (0x1B) - incomplete sequence
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Should be in escape mode

    // Parse '[' - still incomplete
    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Parse 'A' - complete sequence for arrow up
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);
    ck_assert(!parser->in_escape); // Should exit escape mode

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow down escape sequence
START_TEST(test_input_parse_arrow_down) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[B
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'B', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_DOWN);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow left escape sequence
START_TEST(test_input_parse_arrow_left) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[D
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'D', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_LEFT);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow right escape sequence
START_TEST(test_input_parse_arrow_right) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[C
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'C', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_RIGHT);

    talloc_free(ctx);
}

END_TEST
// Test: parse delete escape sequence
START_TEST(test_input_parse_delete) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[3~
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '3', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_DELETE);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow up with NumLock modifier (ESC [ 1 ; 129 A)
START_TEST(test_input_parse_arrow_up_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 A (129 = 1 + 128 NumLock)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow down with NumLock modifier
START_TEST(test_input_parse_arrow_down_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 B
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'B', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_DOWN);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow left with NumLock modifier
START_TEST(test_input_parse_arrow_left_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 D
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'D', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_LEFT);

    talloc_free(ctx);
}

END_TEST
// Test: parse arrow right with NumLock modifier
START_TEST(test_input_parse_arrow_right_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 C
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'C', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_RIGHT);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+arrow with NumLock modifier (ESC [ 1 ; 133 A)
START_TEST(test_input_parse_ctrl_arrow_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 133 A (133 = 5 + 128 = Ctrl + NumLock)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '3', &action);
    ik_input_parse_byte(parser, '3', &action);
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NAV_PARENT);

    talloc_free(ctx);
}

END_TEST
// Test: parse Home key escape sequence
START_TEST(test_input_parse_home) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[1~
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);  // Home = beginning of line

    talloc_free(ctx);
}

END_TEST
// Test: parse End key escape sequence
START_TEST(test_input_parse_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[4~
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '4', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);  // End = end of line

    talloc_free(ctx);
}

END_TEST
// Test: parse Home key with NumLock modifier (ESC [ 1 ; 129 ~)
START_TEST(test_input_parse_home_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);  // Home = beginning of line

    talloc_free(ctx);
}

END_TEST
// Test: parse End key with NumLock modifier (ESC [ 4 ; 129 ~)
START_TEST(test_input_parse_end_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 4 ; 129 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '4', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);  // End = end of line

    talloc_free(ctx);
}

END_TEST
// Test: parse Home key alternate sequence (ESC [ H)
START_TEST(test_input_parse_home_alternate) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ H
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'H', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);  // Home = beginning of line

    talloc_free(ctx);
}

END_TEST
// Test: parse End key alternate sequence (ESC [ F)
START_TEST(test_input_parse_end_alternate) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ F
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'F', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);  // End = end of line

    talloc_free(ctx);
}

END_TEST
// Test: parse Home key with modifier (ESC [ 1 ; 129 H) - CSI u with NumLock
START_TEST(test_input_parse_home_modified_h) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 H
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'H', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);  // Home = beginning of line

    talloc_free(ctx);
}

END_TEST
// Test: parse End key with modifier (ESC [ 1 ; 129 F) - CSI u with NumLock
START_TEST(test_input_parse_end_modified_f) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 129 F
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'F', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);  // End = end of line

    talloc_free(ctx);
}

END_TEST
// Test: ESC [ 1 ; X H with invalid modifier char is rejected
START_TEST(test_input_parse_home_invalid_modifier) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; x H (invalid: 'x' instead of digit)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, 'x', &action);
    ik_input_parse_byte(parser, 'H', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: ESC [ 2 ; 1 H is not recognized (must start with '1')
START_TEST(test_input_parse_home_wrong_prefix) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 2 ; 1 H (wrong: starts with '2' not '1')
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, 'H', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: ESC [ x ~ with invalid key number char
START_TEST(test_input_parse_tilde_invalid_key) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ x ~ (invalid: 'x' instead of digit)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'x', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}

END_TEST
// Test: parse invalid escape sequence resets parser
START_TEST(test_input_parse_invalid_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Start escape sequence
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Invalid sequence: ESC followed by 'x' (not '[')
    ik_input_parse_byte(parser, 'x', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset

    // Verify parser can handle next input correctly (regular char)
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: buffer overflow protection
START_TEST(test_input_parse_buffer_overflow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Start escape sequence
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Send '[' to start a valid-looking sequence
    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Now send 13 more bytes to overflow the buffer (16 byte buffer - 1 for '[' - 1 for null = 14 more)
    // Buffer size is 16, we've used 1 byte ('['), so we need 14 more to reach the limit
    for (int32_t i = 0; i < 14; i++) {
        ik_input_parse_byte(parser, '1', &action);
        if (i < 13) {
            ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
            ck_assert(parser->in_escape);
        }
    }

    // After 14 bytes (total 15 with '['), buffer should overflow and reset
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    // Verify parser can handle next input correctly
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: invalid delete-like sequence (ESC [ X ~ where X is not '3', '5', or '6')
START_TEST(test_input_parse_invalid_delete_like_sequence) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 7 ~ (unrecognized sequence)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '7', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape mode

    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset - complete but unrecognized sequence

    talloc_free(ctx);
}

END_TEST
// Test: incomplete escape sequence at boundary
START_TEST(test_input_parse_escape_partial_at_boundary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 3 X where X is not '~'
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '3', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape, waiting for more

    talloc_free(ctx);
}

END_TEST
// Test: unrecognized CSI sequence with letter
START_TEST(test_input_parse_unrecognized_csi_sequence) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ Z (complete but unrecognized arrow-like sequence)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'Z', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset - complete but unrecognized sequence

    talloc_free(ctx);
}

END_TEST
// Test: unrecognized CSI sequence with middle range letter
START_TEST(test_input_parse_unrecognized_csi_middle_letter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ E (complete but unrecognized arrow-like sequence)
    // This tests a letter in the middle range, not at boundaries
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'E', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset - complete but unrecognized sequence

    talloc_free(ctx);
}

END_TEST
// Test: unrecognized single character escape
START_TEST(test_input_parse_unrecognized_single_char_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Insert key sequence: ESC [ 2 ~
    ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    ik_input_parse_byte(parser, '2', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // When we get '~', the sequence is complete but unrecognized
    // Parser should reset and be ready for next input
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should have reset

    // Verify parser can handle next input correctly
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST
// Test: parse page up escape sequence
START_TEST(test_input_parse_page_up) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[5~
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_PAGE_UP);

    talloc_free(ctx);
}

END_TEST
// Test: parse page down escape sequence
START_TEST(test_input_parse_page_down) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse full sequence \x1b[6~
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '6', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_PAGE_DOWN);

    talloc_free(ctx);
}

END_TEST
// Test: double ESC sequence (ESC ESC) - first ESC should be treated as escape action
START_TEST(test_input_parse_double_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse first ESC - should start escape sequence
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Parse second ESC - first ESC should become IK_INPUT_ESCAPE, second starts new sequence
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_ESCAPE);
    ck_assert(parser->in_escape); // Should still be in escape mode (new sequence started)
    ck_assert_uint_eq(parser->esc_len, 0); // New sequence starts fresh

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_escape_suite(void)
{
    Suite *s = suite_create("Input Escape");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_input_parse_arrow_up);
    tcase_add_test(tc_core, test_input_parse_arrow_down);
    tcase_add_test(tc_core, test_input_parse_arrow_left);
    tcase_add_test(tc_core, test_input_parse_arrow_right);
    tcase_add_test(tc_core, test_input_parse_arrow_up_numlock);
    tcase_add_test(tc_core, test_input_parse_arrow_down_numlock);
    tcase_add_test(tc_core, test_input_parse_arrow_left_numlock);
    tcase_add_test(tc_core, test_input_parse_arrow_right_numlock);
    tcase_add_test(tc_core, test_input_parse_ctrl_arrow_numlock);
    tcase_add_test(tc_core, test_input_parse_delete);
    tcase_add_test(tc_core, test_input_parse_home);
    tcase_add_test(tc_core, test_input_parse_end);
    tcase_add_test(tc_core, test_input_parse_home_numlock);
    tcase_add_test(tc_core, test_input_parse_end_numlock);
    tcase_add_test(tc_core, test_input_parse_home_alternate);
    tcase_add_test(tc_core, test_input_parse_end_alternate);
    tcase_add_test(tc_core, test_input_parse_home_modified_h);
    tcase_add_test(tc_core, test_input_parse_end_modified_f);
    tcase_add_test(tc_core, test_input_parse_home_invalid_modifier);
    tcase_add_test(tc_core, test_input_parse_home_wrong_prefix);
    tcase_add_test(tc_core, test_input_parse_tilde_invalid_key);
    tcase_add_test(tc_core, test_input_parse_page_up);
    tcase_add_test(tc_core, test_input_parse_page_down);
    tcase_add_test(tc_core, test_input_parse_invalid_escape);
    tcase_add_test(tc_core, test_input_parse_buffer_overflow);
    tcase_add_test(tc_core, test_input_parse_invalid_delete_like_sequence);
    tcase_add_test(tc_core, test_input_parse_escape_partial_at_boundary);
    tcase_add_test(tc_core, test_input_parse_unrecognized_csi_sequence);
    tcase_add_test(tc_core, test_input_parse_unrecognized_csi_middle_letter);
    tcase_add_test(tc_core, test_input_parse_unrecognized_single_char_escape);
    tcase_add_test(tc_core, test_input_parse_double_escape);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_escape_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input/escape_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
