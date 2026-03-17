// Input parser module unit tests - Bracketed paste sequence parsing
#include <check.h>
#include <talloc.h>
#include "apps/ikigai/input.h"
#include "tests/helpers/test_utils_helper.h"

// Helper: send byte sequence through parser
static void parse_bytes(ik_input_parser_t *parser, const char *bytes, size_t len,
                         ik_input_action_t *action_out)
{
    for (size_t i = 0; i < len; i++) {
        ik_input_parse_byte(parser, bytes[i], action_out);
    }
}

// Test: ESC [ 2 0 0 ~ produces IK_INPUT_PASTE_START
START_TEST(test_input_parse_paste_start) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 2 0 0 ~
    const char seq[] = { 0x1B, '[', '2', '0', '0', '~' };
    parse_bytes(parser, seq, sizeof(seq), &action);

    ck_assert_int_eq(action.type, IK_INPUT_PASTE_START);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

// Test: ESC [ 2 0 1 ~ produces IK_INPUT_PASTE_END
START_TEST(test_input_parse_paste_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 2 0 1 ~
    const char seq[] = { 0x1B, '[', '2', '0', '1', '~' };
    parse_bytes(parser, seq, sizeof(seq), &action);

    ck_assert_int_eq(action.type, IK_INPUT_PASTE_END);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

// Test: parser recovers normally after paste start sequence
START_TEST(test_input_parse_paste_start_recovery) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send PASTE_START
    const char seq[] = { 0x1B, '[', '2', '0', '0', '~' };
    parse_bytes(parser, seq, sizeof(seq), &action);
    ck_assert_int_eq(action.type, IK_INPUT_PASTE_START);

    // Parser should recover: next regular char works
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}
END_TEST

// Test: parser recovers normally after paste end sequence
START_TEST(test_input_parse_paste_end_recovery) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send PASTE_END
    const char seq[] = { 0x1B, '[', '2', '0', '1', '~' };
    parse_bytes(parser, seq, sizeof(seq), &action);
    ck_assert_int_eq(action.type, IK_INPUT_PASTE_END);

    // Parser should recover: next regular char works
    ik_input_parse_byte(parser, 'z', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'z');

    talloc_free(ctx);
}
END_TEST

// Test: paste sequences don't collide with existing tilde parsing (ESC [ 5 ~)
START_TEST(test_input_paste_no_collision_with_page_up) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 5 ~ should still be PAGE_UP
    const char seq[] = { 0x1B, '[', '5', '~' };
    parse_bytes(parser, seq, sizeof(seq), &action);
    ck_assert_int_eq(action.type, IK_INPUT_PAGE_UP);

    talloc_free(ctx);
}
END_TEST

// Test: paste sequences don't collide with existing tilde parsing (ESC [ 3 ~)
START_TEST(test_input_paste_no_collision_with_delete) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 3 ~ should still be DELETE
    const char seq[] = { 0x1B, '[', '3', '~' };
    parse_bytes(parser, seq, sizeof(seq), &action);
    ck_assert_int_eq(action.type, IK_INPUT_DELETE);

    talloc_free(ctx);
}
END_TEST

static Suite *input_paste_suite(void)
{
    Suite *s = suite_create("Input Paste Sequences");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_test(tc, test_input_parse_paste_start);
    tcase_add_test(tc, test_input_parse_paste_end);
    tcase_add_test(tc, test_input_parse_paste_start_recovery);
    tcase_add_test(tc, test_input_parse_paste_end_recovery);
    tcase_add_test(tc, test_input_paste_no_collision_with_page_up);
    tcase_add_test(tc, test_input_paste_no_collision_with_delete);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = input_paste_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input/paste_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
