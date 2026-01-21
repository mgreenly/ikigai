#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../../src/array.h"
#include "../../test_utils_helper.h"

// Test successful array creation
START_TEST(test_array_create_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);

    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;
    ck_assert_ptr_nonnull(array);
    ck_assert_ptr_null(array->data); // Lazy allocation - no data yet
    ck_assert_uint_eq(array->element_size, sizeof(int32_t));
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 0);
    ck_assert_uint_eq(array->increment, 10);

    talloc_free(ctx);
}
END_TEST
// Test array creation with invalid element_size (0)
START_TEST(test_array_create_invalid_element_size) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, 0, 10);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_nonnull(strstr(error_message(res.err), "element_size"));

    talloc_free(ctx);
}

END_TEST
// Test array creation with invalid increment (0)
START_TEST(test_array_create_invalid_increment) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 0);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_nonnull(strstr(error_message(res.err), "increment"));

    talloc_free(ctx);
}

END_TEST
// Test array_size on empty array
START_TEST(test_array_size_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    size_t size = ik_array_size(array);
    ck_assert_uint_eq(size, 0);

    talloc_free(ctx);
}

END_TEST
// Test array_capacity on empty array
START_TEST(test_array_capacity_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    size_t capacity = ik_array_capacity(array);
    ck_assert_uint_eq(capacity, 0);

    talloc_free(ctx);
}

END_TEST
// Test clear array
START_TEST(test_array_clear) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add some elements
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);
    ck_assert_uint_eq(array->capacity, 10);

    // Clear array
    ik_array_clear(array);

    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 10); // Capacity unchanged
    ck_assert_ptr_nonnull(array->data); // Data buffer still allocated

    talloc_free(ctx);
}

END_TEST
// Security test: Clear then append (verify array still works)
START_TEST(test_array_clear_then_append) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add elements
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Clear
    ik_array_clear(array);
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 10);

    // Append new elements (should reuse existing capacity)
    for (int32_t i = 100; i < 103; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Verify new elements
    ck_assert_uint_eq(array->size, 3);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 100);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 101);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 102);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test assertion: size with NULL array
START_TEST(test_array_size_null_array_asserts) {
    ik_array_size(NULL);
}

END_TEST
// Test assertion: capacity with NULL array
START_TEST(test_array_capacity_null_array_asserts) {
    ik_array_capacity(NULL);
}

END_TEST
#endif

// Test suite setup
static Suite *array_basic_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Array Basic");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_array_create_success);
    tcase_add_test(tc_core, test_array_create_invalid_element_size);
    tcase_add_test(tc_core, test_array_create_invalid_increment);
    tcase_add_test(tc_core, test_array_size_empty);
    tcase_add_test(tc_core, test_array_capacity_empty);
    tcase_add_test(tc_core, test_array_clear);
    tcase_add_test(tc_core, test_array_clear_then_append);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    // Assertion tests - only in debug builds
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_array_size_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_array_capacity_null_array_asserts, SIGABRT);
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

    s = array_basic_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/array/basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
