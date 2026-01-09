#include "../../../test_constants.h"
/**
 * @file anthropic_response_coverage_test.c
 * @brief Additional coverage tests for Anthropic response parsing edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/response.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Finish Reason Mapping Coverage Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_all) {
    struct { const char *input; ik_finish_reason_t expected; } test_cases[] = {
        {"end_turn", IK_FINISH_STOP}, {"stop_sequence", IK_FINISH_STOP},
        {"max_tokens", IK_FINISH_LENGTH}, {"tool_use", IK_FINISH_TOOL_USE},
        {"refusal", IK_FINISH_CONTENT_FILTER}, {"unknown_reason", IK_FINISH_UNKNOWN},
        {NULL, IK_FINISH_UNKNOWN}, {"", IK_FINISH_UNKNOWN}
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        ik_finish_reason_t reason = ik_anthropic_map_finish_reason(test_cases[i].input);
        ck_assert_int_eq(reason, test_cases[i].expected);
    }
}

END_TEST

/* ================================================================
 * Additional Error Parsing Coverage Tests
 * ================================================================ */

/* ================================================================
 * Stub Function Coverage Tests
 * ================================================================ */

static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;
    (void)completion;
    return OK(NULL);
}

START_TEST(test_start_request_stub) {
    ik_request_t req = {0};
    int32_t dummy_ctx = 42;

    res_t r = ik_anthropic_start_request(&dummy_ctx, &req, dummy_completion_cb, NULL);

    ck_assert(!is_err(&r));
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_response_coverage_suite(void)
{
    Suite *s = suite_create("Anthropic Response Coverage");

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish, IK_TEST_TIMEOUT);
    tcase_add_test(tc_finish, test_map_finish_reason_all);
    suite_add_tcase(s, tc_finish);

    TCase *tc_stubs = tcase_create("Stub Functions");
    tcase_set_timeout(tc_stubs, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_stubs, setup, teardown);
    tcase_add_test(tc_stubs, test_start_request_stub);
    suite_add_tcase(s, tc_stubs);

    return s;
}

int main(void)
{
    Suite *s = anthropic_response_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
