#include "apps/ikigai/msg.h"
#include <check.h>
#include <stdio.h>

/* Test conversation kinds return true */
START_TEST(test_conversation_kinds) {
    ck_assert(ik_msg_is_conversation_kind("system") == true);
    ck_assert(ik_msg_is_conversation_kind("user") == true);
    ck_assert(ik_msg_is_conversation_kind("assistant") == true);
    ck_assert(ik_msg_is_conversation_kind("tool_call") == true);
    ck_assert(ik_msg_is_conversation_kind("tool_result") == true);
    ck_assert(ik_msg_is_conversation_kind("tool") == true);
}
END_TEST
/* Test metadata kinds return false */
START_TEST(test_metadata_kinds) {
    ck_assert(ik_msg_is_conversation_kind("clear") == false);
    ck_assert(ik_msg_is_conversation_kind("mark") == false);
    ck_assert(ik_msg_is_conversation_kind("rewind") == false);
    ck_assert(ik_msg_is_conversation_kind("agent_killed") == false);
    ck_assert(ik_msg_is_conversation_kind("interrupted") == false);
}

END_TEST
/* Test NULL returns false */
START_TEST(test_null_kind) {
    ck_assert(ik_msg_is_conversation_kind(NULL) == false);
}

END_TEST
/* Test unknown kinds return false */
START_TEST(test_unknown_kinds) {
    ck_assert(ik_msg_is_conversation_kind("bogus") == false);
    ck_assert(ik_msg_is_conversation_kind("unknown") == false);
    ck_assert(ik_msg_is_conversation_kind("") == false);
}

END_TEST

static Suite *msg_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Message Helpers");

    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_conversation_kinds);
    tcase_add_test(tc_core, test_metadata_kinds);
    tcase_add_test(tc_core, test_null_kind);
    tcase_add_test(tc_core, test_unknown_kinds);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = msg_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/msg/msg_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
