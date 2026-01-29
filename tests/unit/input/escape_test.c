// Input parser module unit tests - Edge cases and error handling
#include <check.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../test_utils_helper.h"

START_TEST(test_input_parse_invalid_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Invalid: ESC followed by 'x' (not '[')
    ik_input_parse_byte(parser, 'x', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    // Verify recovery
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_buffer_overflow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert(parser->in_escape);

    ik_input_parse_byte(parser, '[', &action);

    // Send 14 bytes to overflow 16-byte buffer
    for (int32_t i = 0; i < 14; i++) {
        ik_input_parse_byte(parser, '1', &action);
        if (i < 13) {
            ck_assert(parser->in_escape);
        }
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    // Verify recovery
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_escape_partial_at_boundary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 3 A (not ~)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '3', &action);
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_unrecognized_csi_sequence) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ Z (unrecognized)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'Z', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_unrecognized_csi_middle_letter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ E (unrecognized)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'E', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_insert_key) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 2 ~ (Insert - unrecognized)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    // Verify recovery
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_double_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Second ESC - first becomes IK_INPUT_ESCAPE
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_ESCAPE);
    ck_assert(parser->in_escape);
    ck_assert_uint_eq(parser->esc_len, 0);

    talloc_free(ctx);
}
END_TEST

static Suite *escape_edge_suite(void)
{
    Suite *s = suite_create("Escape Edge Cases");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_test(tc, test_input_parse_invalid_escape);
    tcase_add_test(tc, test_input_parse_buffer_overflow);
    tcase_add_test(tc, test_input_parse_escape_partial_at_boundary);
    tcase_add_test(tc, test_input_parse_unrecognized_csi_sequence);
    tcase_add_test(tc, test_input_parse_unrecognized_csi_middle_letter);
    tcase_add_test(tc, test_input_parse_insert_key);
    tcase_add_test(tc, test_input_parse_double_escape);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = escape_edge_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input/escape_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
