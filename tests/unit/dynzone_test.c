/**
 * @file dynzone_test.c
 * @brief Unit tests for dynamic zone text buffer
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/dynzone.h"
#include "../test_utils.h"

/* Test: Create dynamic zone */
START_TEST(test_dynzone_create) {
    void *ctx = talloc_new(NULL);
    ik_dynzone_t *zone = NULL;

    /* Create zone */
    res_t res = ik_dynzone_create(ctx, &zone);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(zone);

    /* Verify text buffer is empty */
    char *text = NULL;
    size_t len = 0;
    res = ik_dynzone_get_text(zone, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at position 0 */
    ck_assert_uint_eq(zone->cursor_byte_offset, 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Create dynamic zone - OOM scenarios */
START_TEST(test_dynzone_create_oom) {
    void *ctx = talloc_new(NULL);
    ik_dynzone_t *zone = NULL;

    /* Test OOM during zone allocation */
    oom_test_fail_next_alloc();
    res_t res = ik_dynzone_create(ctx, &zone);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(zone);
    oom_test_reset();

    /* Test OOM during byte array allocation (after zone alloc succeeds) */
    oom_test_fail_after_n_calls(1);
    res = ik_dynzone_create(ctx, &zone);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(zone);
    oom_test_reset();

    talloc_free(ctx);
}
END_TEST

/* Test: NULL parameter assertions */
START_TEST(test_dynzone_create_null_param) {
    void *ctx = talloc_new(NULL);

    /* zone_out cannot be NULL - should abort */
    ik_dynzone_create(ctx, NULL);

    talloc_free(ctx);
}
END_TEST

/* Test: Get text NULL parameter assertions */
START_TEST(test_dynzone_get_text_null_params) {
    void *ctx = talloc_new(NULL);
    ik_dynzone_t *zone = NULL;
    char *text = NULL;
    size_t len = 0;

    ik_dynzone_create(ctx, &zone);

    /* zone cannot be NULL */
    ik_dynzone_get_text(NULL, &text, &len);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_dynzone_get_text_null_text_out) {
    void *ctx = talloc_new(NULL);
    ik_dynzone_t *zone = NULL;
    size_t len = 0;

    ik_dynzone_create(ctx, &zone);

    /* text_out cannot be NULL */
    ik_dynzone_get_text(zone, NULL, &len);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_dynzone_get_text_null_len_out) {
    void *ctx = talloc_new(NULL);
    ik_dynzone_t *zone = NULL;
    char *text = NULL;

    ik_dynzone_create(ctx, &zone);

    /* len_out cannot be NULL */
    ik_dynzone_get_text(zone, &text, NULL);

    talloc_free(ctx);
}
END_TEST

/* Test: Clear dynamic zone */
START_TEST(test_dynzone_clear) {
    void *ctx = talloc_new(NULL);
    ik_dynzone_t *zone = NULL;

    ik_dynzone_create(ctx, &zone);

    /* Manually add some data to test clearing */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(zone->text, test_data[i]);
    }
    zone->cursor_byte_offset = 3;

    /* Clear the zone */
    ik_dynzone_clear(zone);

    /* Verify empty */
    char *text = NULL;
    size_t len = 0;
    ik_dynzone_get_text(zone, &text, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at 0 */
    ck_assert_uint_eq(zone->cursor_byte_offset, 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Clear NULL parameter assertion */
START_TEST(test_dynzone_clear_null_param) {
    /* zone cannot be NULL - should abort */
    ik_dynzone_clear(NULL);
}
END_TEST

static Suite *dynzone_suite(void) {
    Suite *s = suite_create("DynamicZone");
    TCase *tc_core = tcase_create("Core");

    /* Normal tests */
    tcase_add_test(tc_core, test_dynzone_create);
    tcase_add_test(tc_core, test_dynzone_create_oom);
    tcase_add_test(tc_core, test_dynzone_clear);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_core, test_dynzone_create_null_param, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_dynzone_get_text_null_params, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_dynzone_get_text_null_text_out, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_dynzone_get_text_null_len_out, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_dynzone_clear_null_param, SIGABRT);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int number_failed;
    Suite *s = dynzone_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
