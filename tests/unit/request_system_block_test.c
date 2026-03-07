#include <check.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "shared/error.h"

/* ================================================================
 * Zero system blocks: fresh request has count=0 and NULL array
 * ================================================================ */

START_TEST(test_no_system_blocks)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_request_t *req = NULL;

    res_t res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(req);
    ck_assert_uint_eq(req->system_block_count, 0);
    ck_assert_ptr_null(req->system_blocks);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * One system block: count=1, text and cacheable preserved
 * ================================================================ */

START_TEST(test_one_system_block)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_request_t *req = NULL;

    res_t res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_request_add_system_block(req, "You are helpful.", true);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(req->system_block_count, 1);
    ck_assert_ptr_nonnull(req->system_blocks);
    ck_assert_str_eq(req->system_blocks[0].text, "You are helpful.");
    ck_assert(req->system_blocks[0].cacheable == true);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_one_system_block_not_cacheable)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_request_t *req = NULL;

    res_t res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_request_add_system_block(req, "Plain block.", false);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(req->system_block_count, 1);
    ck_assert_str_eq(req->system_blocks[0].text, "Plain block.");
    ck_assert(req->system_blocks[0].cacheable == false);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Multiple system blocks: count accurate, order preserved
 * ================================================================ */

START_TEST(test_multiple_system_blocks)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_request_t *req = NULL;

    res_t res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_request_add_system_block(req, "Block one.", true);
    ck_assert(is_ok(&res));

    res = ik_request_add_system_block(req, "Block two.", false);
    ck_assert(is_ok(&res));

    res = ik_request_add_system_block(req, "Block three.", true);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(req->system_block_count, 3);
    ck_assert_ptr_nonnull(req->system_blocks);

    ck_assert_str_eq(req->system_blocks[0].text, "Block one.");
    ck_assert(req->system_blocks[0].cacheable == true);

    ck_assert_str_eq(req->system_blocks[1].text, "Block two.");
    ck_assert(req->system_blocks[1].cacheable == false);

    ck_assert_str_eq(req->system_blocks[2].text, "Block three.");
    ck_assert(req->system_blocks[2].cacheable == true);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * System blocks do not affect existing system_prompt field
 * ================================================================ */

START_TEST(test_system_blocks_independent_of_system_prompt)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_request_t *req = NULL;

    res_t res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_request_set_system(req, "Legacy system prompt.");
    ck_assert(is_ok(&res));

    res = ik_request_add_system_block(req, "New block.", false);
    ck_assert(is_ok(&res));

    ck_assert_str_eq(req->system_prompt, "Legacy system prompt.");
    ck_assert_uint_eq(req->system_block_count, 1);
    ck_assert_str_eq(req->system_blocks[0].text, "New block.");

    talloc_free(ctx);
}
END_TEST

static Suite *request_system_block_suite(void)
{
    Suite *s = suite_create("request_system_block");

    TCase *tc_zero = tcase_create("ZeroBlocks");
    tcase_add_test(tc_zero, test_no_system_blocks);
    suite_add_tcase(s, tc_zero);

    TCase *tc_one = tcase_create("OneBlock");
    tcase_add_test(tc_one, test_one_system_block);
    tcase_add_test(tc_one, test_one_system_block_not_cacheable);
    suite_add_tcase(s, tc_one);

    TCase *tc_multi = tcase_create("MultipleBlocks");
    tcase_add_test(tc_multi, test_multiple_system_blocks);
    suite_add_tcase(s, tc_multi);

    TCase *tc_compat = tcase_create("BackwardCompat");
    tcase_add_test(tc_compat, test_system_blocks_independent_of_system_prompt);
    suite_add_tcase(s, tc_compat);

    return s;
}

int32_t main(void)
{
    Suite *s = request_system_block_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/request_system_block_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
