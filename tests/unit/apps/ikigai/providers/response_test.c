#include "tests/test_constants.h"
#include "apps/ikigai/providers/response.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
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
 * Response Builder Tests
 * ================================================================ */

START_TEST(test_response_create) {
    ik_response_t *resp = NULL;
    res_t result = ik_response_create(test_ctx, &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    ck_assert_int_eq(resp->usage.cached_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
    ck_assert_ptr_null(resp->model);
    ck_assert_ptr_null(resp->provider_data);
}
END_TEST




/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *response_suite(void)
{
    Suite *s = suite_create("Response Builders");

    TCase *tc_response = tcase_create("Response Builders");
    tcase_set_timeout(tc_response, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_response, setup, teardown);
    tcase_add_test(tc_response, test_response_create);
    suite_add_tcase(s, tc_response);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = response_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/response_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
