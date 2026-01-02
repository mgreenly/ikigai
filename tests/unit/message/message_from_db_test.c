/**
 * @file message_from_db_test.c
 * @brief Unit tests for message.c ik_message_from_db_msg function
 *
 * Coordinates test suites split across multiple files:
 * - Tool call message tests
 * - Tool result message tests
 * - Thinking block tests
 */

#include "message_tool_call_helpers.h"
#include "message_tool_result_helpers.h"
#include "message_thinking_helpers.h"

#include <check.h>

static Suite *message_from_db_suite(void)
{
    Suite *s = suite_create("Message from DB");

    // Add test cases from split files
    suite_add_tcase(s, create_tool_call_tcase());
    suite_add_tcase(s, create_tool_result_tcase());
    suite_add_tcase(s, create_thinking_tcase());

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_from_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
