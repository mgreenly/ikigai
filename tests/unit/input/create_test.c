// Input parser module unit tests - Parser creation and assertion tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

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

// Test suite
static Suite *input_create_suite(void)
{
    Suite *s = suite_create("Input Create");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind

    tcase_add_test(tc_core, test_input_parser_create);

#ifndef NDEBUG
    tcase_add_test_raise_signal(tc_assertions, test_input_parser_create_null_parent_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_input_parser_create_null_parser_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_input_parse_byte_null_parser_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_input_parse_byte_null_action_out_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    Suite *s = input_create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
