#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../../src/array.h"
#include "../../test_utils_helper.h"

// Test insert at beginning
START_TEST(test_array_insert_at_beginning) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add some values
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert at beginning
    int32_t value = 99;
    res = ik_array_insert(array, 0, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 4);

    // Verify order: [99, 0, 1, 2]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 2);

    talloc_free(ctx);
}

END_TEST
// Test insert in middle
START_TEST(test_array_insert_in_middle) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add values [0, 1, 2, 3]
    for (int32_t i = 0; i < 4; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert 99 at index 2
    int32_t value = 99;
    res = ik_array_insert(array, 2, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);

    // Verify order: [0, 1, 99, 2, 3]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 2);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 4), 3);

    talloc_free(ctx);
}

END_TEST
// Test insert at end (equivalent to append)
START_TEST(test_array_insert_at_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add values [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert at end (index == size)
    int32_t value = 99;
    res = ik_array_insert(array, 3, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 4);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 99);

    talloc_free(ctx);
}

END_TEST
// Test insert with growth
START_TEST(test_array_insert_with_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Fill to capacity
    for (int32_t i = 0; i < 2; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert should trigger growth
    int32_t value = 99;
    res = ik_array_insert(array, 1, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 3);
    ck_assert_uint_eq(array->capacity, 4); // 2 -> 4

    // Verify order: [0, 99, 1]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 1);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test assertion: insert with invalid index
START_TEST(test_array_insert_invalid_index_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    int32_t value = 42;
    // Insert at index > size is invalid
    ik_array_insert(array, 1, &value); // size is 0, so index 1 is invalid

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite setup
static Suite *array_insert_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Array Insert");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_array_insert_at_beginning);
    tcase_add_test(tc_core, test_array_insert_in_middle);
    tcase_add_test(tc_core, test_array_insert_at_end);
    tcase_add_test(tc_core, test_array_insert_with_growth);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    // Assertion tests - only in debug builds
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_array_insert_invalid_index_asserts, SIGABRT);
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

    s = array_insert_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
