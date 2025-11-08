// Input parser module unit tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/input.h"
#include "../../src/error.h"
#include "../test_utils.h"

// Test: successful input parser creation
START_TEST(test_input_parser_create)
{
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
    ck_assert(parser->in_escape); // Still in escape, not a recognized sequence

    talloc_free(ctx);
}
END_TEST

// Test: incomplete arrow sequence that doesn't complete
START_TEST(test_input_parse_incomplete_arrow_like_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse ESC [ X where X is not A/B/C/D
    res = ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, '[', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    res = ik_input_parse_byte(parser, 'Z', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still waiting for more input

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
