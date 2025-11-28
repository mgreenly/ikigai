#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../../src/byte_array.h"
#include "../../test_utils.h"

// Test delete from beginning
START_TEST(test_byte_array_delete_from_beginning) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add [0, 1, 2, 3]
    for (uint8_t i = 0; i < 4; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Delete first element
    ik_byte_array_delete(array, 0);

    ck_assert_uint_eq(ik_byte_array_size(array), 3);

    // Verify remaining: [1, 2, 3]
    ck_assert_uint_eq(ik_byte_array_get(array, 0), 1);
    ck_assert_uint_eq(ik_byte_array_get(array, 1), 2);
    ck_assert_uint_eq(ik_byte_array_get(array, 2), 3);

    talloc_free(ctx);
}

END_TEST
// Test delete from middle
START_TEST(test_byte_array_delete_from_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add [0, 1, 2, 3]
    for (uint8_t i = 0; i < 4; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Delete element at index 2
    ik_byte_array_delete(array, 2);

    ck_assert_uint_eq(ik_byte_array_size(array), 3);

    // Verify remaining: [0, 1, 3]
    ck_assert_uint_eq(ik_byte_array_get(array, 0), 0);
    ck_assert_uint_eq(ik_byte_array_get(array, 1), 1);
    ck_assert_uint_eq(ik_byte_array_get(array, 2), 3);

    talloc_free(ctx);
}

END_TEST
// Test delete from end
START_TEST(test_byte_array_delete_from_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add [0, 1, 2, 3]
    for (uint8_t i = 0; i < 4; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Delete last element
    ik_byte_array_delete(array, 3);

    ck_assert_uint_eq(ik_byte_array_size(array), 3);

    // Verify remaining: [0, 1, 2]
    for (uint8_t i = 0; i < 3; i++) {
        ck_assert_uint_eq(ik_byte_array_get(array, i), i);
    }

    talloc_free(ctx);
}

END_TEST
// Test set byte
START_TEST(test_byte_array_set)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add [0, 1, 2]
    for (uint8_t i = 0; i < 3; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Set middle element to 99
    ik_byte_array_set(array, 1, 99);

    // Verify: [0, 99, 2]
    ck_assert_uint_eq(ik_byte_array_get(array, 0), 0);
    ck_assert_uint_eq(ik_byte_array_get(array, 1), 99);
    ck_assert_uint_eq(ik_byte_array_get(array, 2), 2);

    talloc_free(ctx);
}

END_TEST

#ifndef NDEBUG
// Test assertion on get with out of bounds index
START_TEST(test_byte_array_get_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ik_byte_array_t *array = res.ok;

    ik_byte_array_get(array, 0); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on delete with out of bounds index
START_TEST(test_byte_array_delete_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ik_byte_array_t *array = res.ok;

    ik_byte_array_delete(array, 0); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on set with out of bounds index
START_TEST(test_byte_array_set_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ik_byte_array_t *array = res.ok;

    ik_byte_array_set(array, 0, 99); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on insert with out of bounds index
START_TEST(test_byte_array_insert_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ik_byte_array_t *array = res.ok;

    ik_byte_array_insert(array, 2, 99); // Can only insert at 0 for empty array - should assert

    talloc_free(ctx);
}

END_TEST
#endif

static Suite *byte_array_delete_set_suite(void)
{
    Suite *s;
    TCase *tc_core;
    TCase *tc_assertions;

    s = suite_create("ByteArray_DeleteSet");
    tc_core = tcase_create("Core");

    // Delete tests
    tcase_add_test(tc_core, test_byte_array_delete_from_beginning);
    tcase_add_test(tc_core, test_byte_array_delete_from_middle);
    tcase_add_test(tc_core, test_byte_array_delete_from_end);

    // Set test
    tcase_add_test(tc_core, test_byte_array_set);

    suite_add_tcase(s, tc_core);

#ifndef NDEBUG
    // Assertion tests (debug mode only)
    tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_byte_array_get_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_byte_array_delete_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_byte_array_set_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_byte_array_insert_out_of_bounds_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = byte_array_delete_set_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
