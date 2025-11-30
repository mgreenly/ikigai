#include <check.h>
#include <talloc.h>
#include "../../../src/openai/tool_choice.h"
#include "../../test_utils.h"

/*
 * Test tool_choice type and helper functions
 *
 * Tests for creating and serializing tool_choice values:
 * - "auto" mode
 * - "none" mode
 * - "required" mode
 * - specific tool mode (e.g., {"type": "function", "function": {"name": "glob"}})
 */

// Test creating tool_choice for "auto" mode
START_TEST(test_tool_choice_auto) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_auto();

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_AUTO);
    ck_assert_ptr_null(choice.tool_name);

    talloc_free(ctx);
}
END_TEST

// Test creating tool_choice for "none" mode
START_TEST(test_tool_choice_none) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_none();

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_NONE);
    ck_assert_ptr_null(choice.tool_name);

    talloc_free(ctx);
}
END_TEST

// Test creating tool_choice for "required" mode
START_TEST(test_tool_choice_required) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_required();

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_REQUIRED);
    ck_assert_ptr_null(choice.tool_name);

    talloc_free(ctx);
}
END_TEST

// Test creating tool_choice for specific tool
START_TEST(test_tool_choice_specific) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_specific(ctx, "glob");

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_SPECIFIC);
    ck_assert_ptr_nonnull(choice.tool_name);
    ck_assert_str_eq(choice.tool_name, "glob");

    talloc_free(ctx);
}
END_TEST

// Test suite setup
static Suite *tool_choice_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Tool Choice");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tool_choice_auto);
    tcase_add_test(tc_core, test_tool_choice_none);
    tcase_add_test(tc_core, test_tool_choice_required);
    tcase_add_test(tc_core, test_tool_choice_specific);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = tool_choice_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
