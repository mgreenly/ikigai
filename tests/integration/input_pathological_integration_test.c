// Pathological input parser tests - trying to break the module with edge cases
// These tests adopt a hacker mindset to find vulnerabilities
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/input.h"
#include "../../src/error.h"
#include "../test_utils.h"

// ========================================================================
// State Confusion Tests
// ========================================================================

// Test: Start UTF-8 sequence, then send ESC - should this reset UTF-8 state?
START_TEST(test_state_confusion_utf8_then_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Start 2-byte UTF-8 sequence (é = 0xC3 0xA9)
    res = ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_utf8);

    // Now send ESC - what happens?
    // Current implementation: ESC is treated as continuation byte, will fail validation
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    // This should return UNKNOWN and reset UTF-8 state (invalid continuation byte)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Verify we can parse normally after
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}
END_TEST
// Test: Start escape sequence, then send UTF-8 lead byte
START_TEST(test_state_confusion_escape_then_utf8)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Start escape sequence
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Send UTF-8 lead byte (0xC3) - not '[', so invalid escape
    res = ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert(is_ok(&res));
    // Should reset escape state (first byte after ESC must be '[')
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// UTF-8 Overlong Encoding Tests (Security Vulnerability)
// ========================================================================

// Test: Overlong 2-byte encoding of ASCII 'A' (U+0041)
// Normal: 0x41
// Overlong: 0xC1 0x81 (INVALID - security risk)
START_TEST(test_utf8_overlong_2byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Send overlong encoding 0xC1 0x81 for 'A'
    // 0xC1 is a valid 2-byte lead byte pattern (110xxxxx)
    res = ik_input_parse_byte(parser, (char)0xC1, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0x81, &action);
    ck_assert(is_ok(&res));
    // Overlong encodings should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: Overlong 3-byte encoding of '/' (U+002F)
// Normal: 0x2F
// Overlong: 0xE0 0x80 0xAF (INVALID - used in directory traversal attacks)
START_TEST(test_utf8_overlong_3byte_slash)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Send overlong encoding 0xE0 0x80 0xAF for '/'
    res = ik_input_parse_byte(parser, (char)0xE0, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0xAF, &action);
    ck_assert(is_ok(&res));
    // Overlong encodings should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Invalid UTF-8 Lead Byte Tests
// ========================================================================

// Test: Invalid UTF-8 lead bytes 0xF8-0xFF (5-byte and 6-byte sequences, invalid in UTF-8)
START_TEST(test_utf8_invalid_lead_byte_f8)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // 0xF8 = 11111000 (would be 5-byte sequence, invalid in UTF-8)
    res = ik_input_parse_byte(parser, (char)0xF8, &action);
    ck_assert(is_ok(&res));
    // Should return UNKNOWN (not a valid lead byte)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_utf8); // Should not enter UTF-8 mode

    talloc_free(ctx);
}

END_TEST
// Test: Continuation byte without lead byte
START_TEST(test_utf8_continuation_without_lead)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Send continuation byte 0x80 (10xxxxxx) without lead byte
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    // Should return UNKNOWN (not a valid character or lead byte)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// UTF-16 Surrogate Tests (Invalid in UTF-8)
// ========================================================================

// Test: UTF-16 high surrogate (U+D800) encoded in UTF-8
// U+D800 = 1101 1000 0000 0000
// Invalid 3-byte UTF-8: 0xED 0xA0 0x80
START_TEST(test_utf8_surrogate_high)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Encode U+D800 (high surrogate) - INVALID in UTF-8
    res = ik_input_parse_byte(parser, (char)0xED, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0xA0, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    // Surrogates should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Codepoint Range Violation Tests
// ========================================================================

// Test: Codepoint beyond valid Unicode (> U+10FFFF)
// U+110000 encoded as 4-byte UTF-8: 0xF4 0x90 0x80 0x80 (INVALID)
START_TEST(test_utf8_codepoint_too_large)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Encode U+110000 (beyond valid Unicode range)
    res = ik_input_parse_byte(parser, (char)0xF4, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0x90, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    // Out-of-range codepoints should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: Null codepoint (U+0000) via UTF-8: 0xC0 0x80 (overlong encoding)
START_TEST(test_utf8_null_codepoint_overlong)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Overlong encoding of null: 0xC0 0x80
    res = ik_input_parse_byte(parser, (char)0xC0, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    // Overlong null encoding should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: Overlong 4-byte encoding (U+0001 encoded as 4 bytes)
// Normal: 0x01
// Overlong 4-byte: 0xF0 0x80 0x80 0x81 (INVALID - security risk)
START_TEST(test_utf8_overlong_4byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Send overlong 4-byte encoding 0xF0 0x80 0x80 0x81
    res = ik_input_parse_byte(parser, (char)0xF0, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0x81, &action);
    ck_assert(is_ok(&res));
    // Overlong encodings should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Comprehensive Validation Tests
// ========================================================================

// Test: U+FFFD (replacement character) itself should work correctly
START_TEST(test_utf8_replacement_char_U_FFFD)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // U+FFFD = 0xEF 0xBF 0xBD (valid 3-byte UTF-8)
    res = ik_input_parse_byte(parser, (char)0xEF, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0xBD, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement char

    talloc_free(ctx);
}

END_TEST
// Test: Valid boundary codepoints (U+0080, U+0800, U+10000)
START_TEST(test_utf8_valid_boundary_codepoints)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // U+0080 (minimum valid 2-byte): 0xC2 0x80
    res = ik_input_parse_byte(parser, (char)0xC2, &action);
    ck_assert(is_ok(&res));
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x80);

    // U+0800 (minimum valid 3-byte): 0xE0 0xA0 0x80
    res = ik_input_parse_byte(parser, (char)0xE0, &action);
    ck_assert(is_ok(&res));
    res = ik_input_parse_byte(parser, (char)0xA0, &action);
    ck_assert(is_ok(&res));
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x800);

    // U+10000 (minimum valid 4-byte): 0xF0 0x90 0x80 0x80
    res = ik_input_parse_byte(parser, (char)0xF0, &action);
    ck_assert(is_ok(&res));
    res = ik_input_parse_byte(parser, (char)0x90, &action);
    ck_assert(is_ok(&res));
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x10000);

    talloc_free(ctx);
}

END_TEST
// Test: Maximum valid Unicode codepoint (U+10FFFF)
START_TEST(test_utf8_max_valid_codepoint)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // U+10FFFF = 0xF4 0x8F 0xBF 0xBF (maximum valid Unicode)
    res = ik_input_parse_byte(parser, (char)0xF4, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0x8F, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    res = ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x10FFFF); // Max valid

    talloc_free(ctx);
}

END_TEST
// Test: UTF-16 low surrogate (U+DFFF) should be rejected
START_TEST(test_utf8_surrogate_low)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // U+DFFF (low surrogate) = 0xED 0xBF 0xBF (INVALID)
    res = ik_input_parse_byte(parser, (char)0xED, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert(is_ok(&res));
    // Should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Escape Sequence Edge Cases
// ========================================================================

// Test: ESC [ followed by null byte
START_TEST(test_escape_sequence_null_byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Send null byte - what happens?
    res = ik_input_parse_byte(parser, '\0', &action);
    ck_assert(is_ok(&res));
    // Should treat as incomplete (waiting for more)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape mode

    talloc_free(ctx);
}

END_TEST
// Test: ESC [ followed by control character (e.g., Ctrl+C)
START_TEST(test_escape_sequence_control_char)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Send Ctrl+C (0x03)
    res = ik_input_parse_byte(parser, 0x03, &action);
    ck_assert(is_ok(&res));
    // Should treat as incomplete/unknown (not a recognized sequence)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Very long escape sequence (just before buffer overflow)
START_TEST(test_escape_sequence_nearly_full_buffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));

    // Send 13 more bytes (total 15 in buffer: '[' + 13 '1's)
    // Buffer is 16 bytes, esc_len will be 14, which is < 15 (sizeof - 1)
    for (int i = 0; i < 13; i++) {
        res = ik_input_parse_byte(parser, '1', &action);
        ck_assert(is_ok(&res));
        ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    }

    // Should still be in escape mode (esc_len == 14, still < 15)
    ck_assert(parser->in_escape);
    ck_assert_uint_eq(parser->esc_len, 14);

    // One more byte brings us to esc_len == 15 (sizeof - 1), triggering overflow protection
    res = ik_input_parse_byte(parser, '1', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should have reset

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Rapid State Transition Tests
// ========================================================================

// Test: Alternating ESC and regular bytes
START_TEST(test_rapid_esc_transitions)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Send: ESC, 'x', ESC, 'y', ESC, '[', 'A'
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, 'x', &action); // Invalid escape
    ck_assert(is_ok(&res));
    ck_assert(!parser->in_escape);

    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC again
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, 'y', &action); // Invalid escape
    ck_assert(is_ok(&res));
    ck_assert(!parser->in_escape);

    // Now send valid arrow up
    res = ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));

    res = ik_input_parse_byte(parser, 'A', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP); // Should work correctly

    talloc_free(ctx);
}

END_TEST
// Test: Multiple incomplete UTF-8 sequences in a row
START_TEST(test_multiple_incomplete_utf8)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Send 3 different UTF-8 lead bytes without completions
    res = ik_input_parse_byte(parser, (char)0xC3, &action); // 2-byte lead
    ck_assert(is_ok(&res));
    ck_assert(parser->in_utf8);

    res = ik_input_parse_byte(parser, (char)0xE2, &action); // 3-byte lead (invalid continuation)
    ck_assert(is_ok(&res));
    // Should reset due to invalid continuation byte (0xE2 is not 10xxxxxx)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_utf8);

    talloc_free(ctx);
}

END_TEST

// ========================================================================
// Test Suite
// ========================================================================

static Suite *input_pathological_suite(void)
{
    Suite *s = suite_create("InputPathological");
    TCase *tc_core = tcase_create("Core");

    // State confusion tests
    tcase_add_test(tc_core, test_state_confusion_utf8_then_escape);
    tcase_add_test(tc_core, test_state_confusion_escape_then_utf8);

    // UTF-8 overlong encoding tests (security vulnerabilities)
    tcase_add_test(tc_core, test_utf8_overlong_2byte);
    tcase_add_test(tc_core, test_utf8_overlong_3byte_slash);
    tcase_add_test(tc_core, test_utf8_overlong_4byte);

    // Invalid UTF-8 lead bytes
    tcase_add_test(tc_core, test_utf8_invalid_lead_byte_f8);
    tcase_add_test(tc_core, test_utf8_continuation_without_lead);

    // UTF-16 surrogates (invalid in UTF-8)
    tcase_add_test(tc_core, test_utf8_surrogate_high);
    tcase_add_test(tc_core, test_utf8_surrogate_low);

    // Codepoint range violations
    tcase_add_test(tc_core, test_utf8_codepoint_too_large);
    tcase_add_test(tc_core, test_utf8_null_codepoint_overlong);

    // Comprehensive validation tests
    tcase_add_test(tc_core, test_utf8_replacement_char_U_FFFD);
    tcase_add_test(tc_core, test_utf8_valid_boundary_codepoints);
    tcase_add_test(tc_core, test_utf8_max_valid_codepoint);

    // Escape sequence edge cases
    tcase_add_test(tc_core, test_escape_sequence_null_byte);
    tcase_add_test(tc_core, test_escape_sequence_control_char);
    tcase_add_test(tc_core, test_escape_sequence_nearly_full_buffer);

    // Rapid state transitions
    tcase_add_test(tc_core, test_rapid_esc_transitions);
    tcase_add_test(tc_core, test_multiple_incomplete_utf8);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_pathological_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
