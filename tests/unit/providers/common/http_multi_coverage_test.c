#include "../../../../src/providers/common/http_multi.c"
#include "../../../test_utils.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/**
 * Coverage tests for http_multi.c internals
 *
 * Tests static functions and edge cases that require direct access.
 */

/* Test context */
static TALLOC_CTX *test_ctx = NULL;

/* Setup/teardown */
static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Write Callback Tests
 */

/* User callback that consumes all data */
static size_t user_callback_success(const char *data, size_t len, void *ctx)
{
    (void)data;
    int *call_count = (int *)ctx;
    (*call_count)++;
    return len;
}

/* User callback that fails to consume all data */
static size_t user_callback_partial(const char *data, size_t len, void *ctx)
{
    (void)data;
    (void)ctx;
    return len / 2;  /* Only consume half */
}

START_TEST(test_write_callback_with_user_callback) {
    http_write_ctx_t *ctx = talloc_zero(test_ctx, http_write_ctx_t);
    int call_count = 0;

    ctx->user_callback = user_callback_success;
    ctx->user_ctx = &call_count;
    ctx->response_buffer = talloc_array(test_ctx, char, 4096);
    ctx->buffer_capacity = 4096;
    ctx->response_len = 0;
    ctx->response_buffer[0] = '\0';

    const char *data = "test data";
    size_t data_len = strlen(data);
    size_t result = http_write_callback(data, 1, data_len, ctx);

    ck_assert_uint_eq(result, data_len);
    ck_assert_int_eq(call_count, 1);
    ck_assert_uint_eq(ctx->response_len, data_len);
}

END_TEST

START_TEST(test_write_callback_user_error) {
    http_write_ctx_t *ctx = talloc_zero(test_ctx, http_write_ctx_t);

    ctx->user_callback = user_callback_partial;
    ctx->user_ctx = NULL;
    ctx->response_buffer = talloc_array(test_ctx, char, 4096);
    ctx->buffer_capacity = 4096;
    ctx->response_len = 0;
    ctx->response_buffer[0] = '\0';

    const char *data = "test data";
    size_t result = http_write_callback(data, 1, strlen(data), ctx);

    /* Should return 0 to indicate error to curl */
    ck_assert_uint_eq(result, 0);
}

END_TEST

START_TEST(test_write_callback_no_resize_needed) {
    http_write_ctx_t *ctx = talloc_zero(test_ctx, http_write_ctx_t);

    ctx->user_callback = NULL;
    ctx->user_ctx = NULL;
    ctx->response_buffer = talloc_array(test_ctx, char, 4096);
    ctx->buffer_capacity = 4096;
    ctx->response_len = 0;
    ctx->response_buffer[0] = '\0';

    const char *data = "small";
    size_t data_len = strlen(data);
    size_t result = http_write_callback(data, 1, data_len, ctx);

    ck_assert_uint_eq(result, data_len);
    ck_assert_uint_eq(ctx->response_len, data_len);
    ck_assert_str_eq(ctx->response_buffer, "small");
    ck_assert_uint_eq(ctx->buffer_capacity, 4096);  /* No resize */
}

END_TEST

START_TEST(test_write_callback_buffer_resize_double) {
    http_write_ctx_t *ctx = talloc_zero(test_ctx, http_write_ctx_t);

    ctx->user_callback = NULL;
    ctx->user_ctx = NULL;
    /* Start with tiny buffer */
    ctx->response_buffer = talloc_array(test_ctx, char, 10);
    ctx->buffer_capacity = 10;
    ctx->response_len = 0;
    ctx->response_buffer[0] = '\0';

    /* Add data that fits in doubled buffer (10 -> 20) */
    const char *data = "12345678901234";  /* 14 chars, needs 15 with null */
    size_t data_len = strlen(data);
    size_t result = http_write_callback(data, 1, data_len, ctx);

    ck_assert_uint_eq(result, data_len);
    ck_assert_uint_eq(ctx->response_len, data_len);
    ck_assert_str_eq(ctx->response_buffer, data);
    ck_assert_uint_ge(ctx->buffer_capacity, 15);  /* At least 15 */
}

END_TEST

START_TEST(test_write_callback_buffer_resize_exact) {
    http_write_ctx_t *ctx = talloc_zero(test_ctx, http_write_ctx_t);

    ctx->user_callback = NULL;
    ctx->user_ctx = NULL;
    /* Start with tiny buffer */
    ctx->response_buffer = talloc_array(test_ctx, char, 10);
    ctx->buffer_capacity = 10;
    ctx->response_len = 0;
    ctx->response_buffer[0] = '\0';

    /* Add data that needs more than doubled buffer */
    char large_data[100];
    memset(large_data, 'X', 99);
    large_data[99] = '\0';  /* 99 chars, needs 100 with null */

    size_t data_len = strlen(large_data);
    size_t result = http_write_callback(large_data, 1, data_len, ctx);

    ck_assert_uint_eq(result, data_len);
    ck_assert_uint_eq(ctx->response_len, data_len);
    ck_assert_int_eq(strncmp(ctx->response_buffer, large_data, 99), 0);
    ck_assert_uint_ge(ctx->buffer_capacity, 100);  /* At least 100 */
}

END_TEST

START_TEST(test_write_callback_accumulate_multiple) {
    http_write_ctx_t *ctx = talloc_zero(test_ctx, http_write_ctx_t);

    ctx->user_callback = NULL;
    ctx->user_ctx = NULL;
    ctx->response_buffer = talloc_array(test_ctx, char, 10);
    ctx->buffer_capacity = 10;
    ctx->response_len = 0;
    ctx->response_buffer[0] = '\0';

    /* Add data in multiple chunks */
    const char *chunk1 = "Hello ";
    const char *chunk2 = "World!";

    size_t len1 = strlen(chunk1);
    size_t result1 = http_write_callback(chunk1, 1, len1, ctx);
    ck_assert_uint_eq(result1, len1);

    size_t len2 = strlen(chunk2);
    size_t result2 = http_write_callback(chunk2, 1, len2, ctx);
    ck_assert_uint_eq(result2, len2);

    ck_assert_str_eq(ctx->response_buffer, "Hello World!");
    ck_assert_uint_eq(ctx->response_len, 12);
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *http_multi_coverage_suite(void)
{
    Suite *s = suite_create("HTTP Multi Coverage");

    /* Write callback tests */
    TCase *tc_write = tcase_create("Write Callback");
    tcase_set_timeout(tc_write, 30);
    tcase_add_checked_fixture(tc_write, setup, teardown);
    tcase_add_test(tc_write, test_write_callback_with_user_callback);
    tcase_add_test(tc_write, test_write_callback_user_error);
    tcase_add_test(tc_write, test_write_callback_no_resize_needed);
    tcase_add_test(tc_write, test_write_callback_buffer_resize_double);
    tcase_add_test(tc_write, test_write_callback_buffer_resize_exact);
    tcase_add_test(tc_write, test_write_callback_accumulate_multiple);
    suite_add_tcase(s, tc_write);

    return s;
}

int main(void)
{
    Suite *s = http_multi_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
