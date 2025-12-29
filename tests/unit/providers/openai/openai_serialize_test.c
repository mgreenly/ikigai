/**
 * @file openai_serialize_test.c
 * @brief Unit tests for OpenAI message serialization - coordinator
 */

#include <check.h>
#include "openai_serialize_user_test.h"
#include "openai_serialize_assistant_test.h"
#include "openai_serialize_tool_test.h"

/* ================================================================
 * Test Suite Coordination
 * ================================================================ */

int main(void)
{
    int number_failed = 0;

    // Run user message tests
    Suite *user_suite = openai_serialize_user_suite();
    SRunner *user_runner = srunner_create(user_suite);
    srunner_run_all(user_runner, CK_NORMAL);
    number_failed += srunner_ntests_failed(user_runner);
    srunner_free(user_runner);

    // Run assistant message tests
    Suite *assistant_suite = openai_serialize_assistant_suite();
    SRunner *assistant_runner = srunner_create(assistant_suite);
    srunner_run_all(assistant_runner, CK_NORMAL);
    number_failed += srunner_ntests_failed(assistant_runner);
    srunner_free(assistant_runner);

    // Run tool message tests
    Suite *tool_suite = openai_serialize_tool_suite();
    SRunner *tool_runner = srunner_create(tool_suite);
    srunner_run_all(tool_runner, CK_NORMAL);
    number_failed += srunner_ntests_failed(tool_runner);
    srunner_free(tool_runner);

    return (number_failed == 0) ? 0 : 1;
}
