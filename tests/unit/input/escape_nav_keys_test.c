// Input parser module unit tests - Navigation key escape sequences
// Home, End, Delete, Page Up, Page Down
#include <check.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../test_utils_helper.h"

START_TEST(test_input_parse_delete) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 3 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '3', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_DELETE);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_home) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 4 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '4', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_home_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ; 129 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_end_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 4 ; 129 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '4', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_home_alternate) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ H
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'H', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_end_alternate) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ F
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'F', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_home_modified_h) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ; 129 H
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'H', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_end_modified_f) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ; 129 F
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'F', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_home_invalid_modifier) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ; x H (invalid)
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

START_TEST(test_input_parse_home_wrong_prefix) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 2 ; 1 H (wrong prefix)
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

START_TEST(test_input_parse_tilde_invalid_key) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ x ~ (invalid)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'x', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_page_up) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 5 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '5', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_PAGE_UP);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_page_down) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 6 ~
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '6', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_PAGE_DOWN);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_invalid_delete_like) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 7 ~ (unrecognized)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '7', &action);
    ik_input_parse_byte(parser, '~', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

static Suite *escape_nav_keys_suite(void)
{
    Suite *s = suite_create("Escape Nav Keys");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_test(tc, test_input_parse_delete);
    tcase_add_test(tc, test_input_parse_home);
    tcase_add_test(tc, test_input_parse_end);
    tcase_add_test(tc, test_input_parse_home_numlock);
    tcase_add_test(tc, test_input_parse_end_numlock);
    tcase_add_test(tc, test_input_parse_home_alternate);
    tcase_add_test(tc, test_input_parse_end_alternate);
    tcase_add_test(tc, test_input_parse_home_modified_h);
    tcase_add_test(tc, test_input_parse_end_modified_f);
    tcase_add_test(tc, test_input_parse_home_invalid_modifier);
    tcase_add_test(tc, test_input_parse_home_wrong_prefix);
    tcase_add_test(tc, test_input_parse_tilde_invalid_key);
    tcase_add_test(tc, test_input_parse_page_up);
    tcase_add_test(tc, test_input_parse_page_down);
    tcase_add_test(tc, test_input_parse_invalid_delete_like);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = escape_nav_keys_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input/escape_nav_keys_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
