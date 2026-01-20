// UTF-8 security tests - testing for vulnerabilities and malformed sequences
// These tests adopt a hacker mindset to find vulnerabilities
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/input.h"
#include "../../src/error.h"
#include "../test_utils_helper.h"

// ========================================================================
// UTF-8 Overlong Encoding Tests (Security Vulnerability)
// ========================================================================

// Test: Overlong 2-byte encoding of ASCII 'A' (U+0041)
// Normal: 0x41
// Overlong: 0xC1 0x81 (INVALID - security risk)
START_TEST(test_utf8_overlong_2byte) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send overlong encoding 0xC1 0x81 for 'A'
    // 0xC1 is a valid 2-byte lead byte pattern (110xxxxx)
    ik_input_parse_byte(parser, (char)0xC1, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0x81, &action);
    // Overlong encodings should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: Overlong 3-byte encoding of '/' (U+002F)
// Normal: 0x2F
// Overlong: 0xE0 0x80 0xAF (INVALID - used in directory traversal attacks)
START_TEST(test_utf8_overlong_3byte_slash) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send overlong encoding 0xE0 0x80 0xAF for '/'
    ik_input_parse_byte(parser, (char)0xE0, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0xAF, &action);
    // Overlong encodings should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: Overlong 4-byte encoding (U+0001 encoded as 4 bytes)
// Normal: 0x01
// Overlong 4-byte: 0xF0 0x80 0x80 0x81 (INVALID - security risk)
START_TEST(test_utf8_overlong_4byte) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send overlong 4-byte encoding 0xF0 0x80 0x80 0x81
    ik_input_parse_byte(parser, (char)0xF0, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0x81, &action);
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
START_TEST(test_utf8_invalid_lead_byte_f8) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // 0xF8 = 11111000 (would be 5-byte sequence, invalid in UTF-8)
    ik_input_parse_byte(parser, (char)0xF8, &action);
    // Should return UNKNOWN (not a valid lead byte)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_utf8); // Should not enter UTF-8 mode

    talloc_free(ctx);
}

END_TEST
// Test: Continuation byte without lead byte
START_TEST(test_utf8_continuation_without_lead) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send continuation byte 0x80 (10xxxxxx) without lead byte
    ik_input_parse_byte(parser, (char)0x80, &action);
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
START_TEST(test_utf8_surrogate_high) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Encode U+D800 (high surrogate) - INVALID in UTF-8
    ik_input_parse_byte(parser, (char)0xED, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0xA0, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0x80, &action);
    // Surrogates should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: UTF-16 low surrogate (U+DFFF) should be rejected
START_TEST(test_utf8_surrogate_low) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // U+DFFF (low surrogate) = 0xED 0xBF 0xBF (INVALID)
    ik_input_parse_byte(parser, (char)0xED, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0xBF, &action);
    // Should be rejected and replaced with U+FFFD
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
START_TEST(test_utf8_codepoint_too_large) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Encode U+110000 (beyond valid Unicode range)
    ik_input_parse_byte(parser, (char)0xF4, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0x90, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0x80, &action);
    // Out-of-range codepoints should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// Test: Null codepoint (U+0000) via UTF-8: 0xC0 0x80 (overlong encoding)
START_TEST(test_utf8_null_codepoint_overlong) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Overlong encoding of null: 0xC0 0x80
    ik_input_parse_byte(parser, (char)0xC0, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, (char)0x80, &action);
    // Overlong null encoding should be rejected and replaced with U+FFFD
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement character

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Comprehensive Validation Tests
// ========================================================================

// Test: U+FFFD (replacement character) itself should work correctly
START_TEST(test_utf8_replacement_char_U_FFFD) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // U+FFFD = 0xEF 0xBF 0xBD (valid 3-byte UTF-8)
    ik_input_parse_byte(parser, (char)0xEF, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0xBD, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0xFFFD); // Replacement char

    talloc_free(ctx);
}

END_TEST
// Test: Valid boundary codepoints (U+0080, U+0800, U+10000)
START_TEST(test_utf8_valid_boundary_codepoints) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // U+0080 (minimum valid 2-byte): 0xC2 0x80
    ik_input_parse_byte(parser, (char)0xC2, &action);
    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x80);

    // U+0800 (minimum valid 3-byte): 0xE0 0xA0 0x80
    ik_input_parse_byte(parser, (char)0xE0, &action);
    ik_input_parse_byte(parser, (char)0xA0, &action);
    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x800);

    // U+10000 (minimum valid 4-byte): 0xF0 0x90 0x80 0x80
    ik_input_parse_byte(parser, (char)0xF0, &action);
    ik_input_parse_byte(parser, (char)0x90, &action);
    ik_input_parse_byte(parser, (char)0x80, &action);
    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x10000);

    talloc_free(ctx);
}

END_TEST
// Test: Maximum valid Unicode codepoint (U+10FFFF)
START_TEST(test_utf8_max_valid_codepoint) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // U+10FFFF = 0xF4 0x8F 0xBF 0xBF (maximum valid Unicode)
    ik_input_parse_byte(parser, (char)0xF4, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0x8F, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    ik_input_parse_byte(parser, (char)0xBF, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x10FFFF); // Max valid

    talloc_free(ctx);
}

END_TEST

// ========================================================================
// Test Suite
// ========================================================================

static Suite *utf8_security_suite(void)
{
    Suite *s = suite_create("UTF8Security");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

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

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = utf8_security_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
