#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "apps/ikigai/array.h"
#include "tests/helpers/test_utils_helper.h"

// Security test: Use-after-free attempt via stale pointer after reallocation
START_TEST(test_array_stale_pointer_after_reallocation) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Fill to capacity
    int32_t value = 42;
    res = ik_array_append(array, &value);
    ck_assert(is_ok(&res));

    // Get pointer to first element
    int32_t *ptr = (int32_t *)ik_array_get(array, 0);
    ck_assert_ptr_nonnull(ptr);
    ck_assert_int_eq(*ptr, 42);

    // Save the old data pointer
    void *old_data = array->data;

    // Force reallocation by appending more elements (capacity 2 -> 4)
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Note: We don't check if array->data changed because realloc may expand in place
    // The important lesson: don't keep pointers into the array across modifications
    (void)old_data; // Suppress unused warning

    // The old pointer 'ptr' may point to freed memory (use-after-free if reallocated)
    // We cannot safely use it, but we can verify the data is still correct via new get
    int32_t *new_ptr = (int32_t *)ik_array_get(array, 0);
    ck_assert_int_eq(*new_ptr, 42);

    // Verify all data survived reallocation
    ck_assert_uint_eq(array->size, 6);
    for (size_t i = 1; i < 6; i++) {
        int32_t *val = (int32_t *)ik_array_get(array, i);
        ck_assert_int_eq(*val, (int32_t)(i - 1));
    }

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test assertion: get with NULL array
START_TEST(test_array_get_null_array_asserts) {
    ik_array_get(NULL, 0);
}

END_TEST
// Test assertion: get with out of bounds index
START_TEST(test_array_get_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    // Access beyond bounds
    ik_array_get(array, 0); // size is 0, so any index is out of bounds

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite setup
static Suite *array_get_set_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Array Get/Set");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_array_stale_pointer_after_reallocation);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    // Assertion tests - only in debug builds
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_add_test_raise_signal(tc_assertions, test_array_get_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_array_get_out_of_bounds_asserts, SIGABRT);
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

    s = array_get_set_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/array/get_set_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
