#include "../../test_constants.h"
#include "../../../src/db/message.h"
#include <check.h>

// Test: NULL kind is invalid
START_TEST(test_null_kind_is_invalid) {
    bool result = ik_db_message_is_valid_kind(NULL);
    ck_assert(!result);
}
END_TEST
// Test: Invalid kind string is rejected
START_TEST(test_invalid_kind_string) {
    bool result = ik_db_message_is_valid_kind("invalid");
    ck_assert(!result);
}

END_TEST
// Test: Another invalid kind
START_TEST(test_another_invalid_kind) {
    bool result = ik_db_message_is_valid_kind("foobar");
    ck_assert(!result);
}

END_TEST
// Test: Empty string is invalid
START_TEST(test_empty_string_is_invalid) {
    bool result = ik_db_message_is_valid_kind("");
    ck_assert(!result);
}

END_TEST
// Test: Valid kinds are accepted (sanity checks)
START_TEST(test_valid_clear) {
    bool result = ik_db_message_is_valid_kind("clear");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_system) {
    bool result = ik_db_message_is_valid_kind("system");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_user) {
    bool result = ik_db_message_is_valid_kind("user");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_assistant) {
    bool result = ik_db_message_is_valid_kind("assistant");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_mark) {
    bool result = ik_db_message_is_valid_kind("mark");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_rewind) {
    bool result = ik_db_message_is_valid_kind("rewind");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_tool_call) {
    bool result = ik_db_message_is_valid_kind("tool_call");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_tool_result) {
    bool result = ik_db_message_is_valid_kind("tool_result");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_agent_killed) {
    bool result = ik_db_message_is_valid_kind("agent_killed");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_command) {
    bool result = ik_db_message_is_valid_kind("command");
    ck_assert(result);
}

END_TEST

START_TEST(test_valid_fork) {
    bool result = ik_db_message_is_valid_kind("fork");
    ck_assert(result);
}

END_TEST
// Test: Case sensitivity
START_TEST(test_kind_case_sensitive) {
    // Uppercase should be invalid
    bool result = ik_db_message_is_valid_kind("CLEAR");
    ck_assert(!result);
}

END_TEST

// Test suite
static Suite *message_kind_validation_suite(void)
{
    Suite *s = suite_create("Message Kind Validation");

    TCase *tc_invalid = tcase_create("Invalid Kinds");
    tcase_set_timeout(tc_invalid, IK_TEST_TIMEOUT);
    tcase_add_test(tc_invalid, test_null_kind_is_invalid);
    tcase_add_test(tc_invalid, test_invalid_kind_string);
    tcase_add_test(tc_invalid, test_another_invalid_kind);
    tcase_add_test(tc_invalid, test_empty_string_is_invalid);
    tcase_add_test(tc_invalid, test_kind_case_sensitive);
    suite_add_tcase(s, tc_invalid);

    TCase *tc_valid = tcase_create("Valid Kinds");
    tcase_set_timeout(tc_valid, IK_TEST_TIMEOUT);
    tcase_add_test(tc_valid, test_valid_clear);
    tcase_add_test(tc_valid, test_valid_system);
    tcase_add_test(tc_valid, test_valid_user);
    tcase_add_test(tc_valid, test_valid_assistant);
    tcase_add_test(tc_valid, test_valid_mark);
    tcase_add_test(tc_valid, test_valid_rewind);
    tcase_add_test(tc_valid, test_valid_tool_call);
    tcase_add_test(tc_valid, test_valid_tool_result);
    tcase_add_test(tc_valid, test_valid_agent_killed);
    tcase_add_test(tc_valid, test_valid_command);
    tcase_add_test(tc_valid, test_valid_fork);
    suite_add_tcase(s, tc_valid);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_kind_validation_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/message_kind_validation_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
