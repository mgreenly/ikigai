#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../../src/array.h"
#include "../../test_utils.h"

// Test appending to empty array (first allocation)
START_TEST(test_array_append_first) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    int32_t value = 42;
    res = ik_array_append(array, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 1);
    ck_assert_uint_eq(array->capacity, 10); // First allocation uses increment
    ck_assert_ptr_nonnull(array->data);

    // Verify the value was stored
    int32_t *stored = (int32_t *)ik_array_get(array, 0);
    ck_assert_int_eq(*stored, 42);

    talloc_free(ctx);
}

END_TEST
// Test appending within capacity (no growth)
START_TEST(test_array_append_no_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Append 5 values (within capacity of 10)
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);
    ck_assert_uint_eq(array->capacity, 10);

    // Verify values
    for (int32_t i = 0; i < 5; i++) {
        int32_t *val = (int32_t *)ik_array_get(array, (size_t)i);
        ck_assert_int_eq(*val, i);
    }

    talloc_free(ctx);
}

END_TEST
// Test appending that triggers growth (doubling)
START_TEST(test_array_append_with_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Append 5 values: capacity goes 0 -> 2 -> 4 -> 8
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);
    ck_assert_uint_eq(array->capacity, 8); // 2 -> 4 -> 8

    // Verify values survived growth
    for (int32_t i = 0; i < 5; i++) {
        int32_t *val = (int32_t *)ik_array_get(array, (size_t)i);
        ck_assert_int_eq(*val, i);
    }

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test assertion: append with NULL array
START_TEST(test_array_append_null_array_asserts) {
    int32_t value = 42;
    ik_array_append(NULL, &value);
}

END_TEST
// Test assertion: append with NULL element
START_TEST(test_array_append_null_element_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    ik_array_append(array, NULL);

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite setup
static Suite *array_append_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Array Append");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_array_append_first);
    tcase_add_test(tc_core, test_array_append_no_growth);
    tcase_add_test(tc_core, test_array_append_with_growth);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    // Assertion tests - only in debug builds
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_array_append_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_array_append_null_element_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = array_append_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
