/**
 * @file request_serialize_success_test.c
 * @brief Test orchestrator for Anthropic request serialization success paths
 *
 * This test file orchestrates the success path tests by combining test
 * suites from content block and message serialization test modules.
 */

#include <check.h>
#include "content_block_serialize_helper.h"
#include "message_serialize_helper.h"

/* ================================================================
 * Test Suite Orchestration - runs split test suites
 * ================================================================ */

int main(void)
{
    int total_failures = 0;

    // Run content block serialization tests
    Suite *content_suite = content_block_serialize_suite();
    SRunner *content_runner = srunner_create(content_suite);
    srunner_set_xml(content_runner, "reports/check/unit/providers/anthropic/request_serialize_success_test.xml");
    srunner_run_all(content_runner, CK_NORMAL);
    total_failures += srunner_ntests_failed(content_runner);
    srunner_free(content_runner);

    // Run message serialization tests
    Suite *message_suite = message_serialize_suite();
    SRunner *message_runner = srunner_create(message_suite);
    srunner_set_xml(message_runner, "reports/check/unit/providers/anthropic/request_serialize_success_test.xml");
    srunner_run_all(message_runner, CK_NORMAL);
    total_failures += srunner_ntests_failed(message_runner);
    srunner_free(message_runner);

    return (total_failures == 0) ? 0 : 1;
}
