#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../../src/array.h"
#include "../../test_utils.h"

// Test delete from beginning
START_TEST(test_array_delete_from_beginning) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2, 3]
    for (int32_t i = 0; i < 4; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete first element
    ik_array_delete(array, 0);

    ck_assert_uint_eq(array->size, 3);
    // Verify: [1, 2, 3]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 2);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 3);

    talloc_free(ctx);
}

END_TEST
// Test delete from middle
START_TEST(test_array_delete_from_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2, 3, 4]
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete middle element
    ik_array_delete(array, 2);

    ck_assert_uint_eq(array->size, 4);
    // Verify: [0, 1, 3, 4]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 3);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 4);

    talloc_free(ctx);
}

END_TEST
// Test delete from end
START_TEST(test_array_delete_from_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete last element
    ik_array_delete(array, 2);

    ck_assert_uint_eq(array->size, 2);
    // Verify: [0, 1]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);

    talloc_free(ctx);
}

END_TEST
// Security test: Delete all elements one by one (check for size underflow)
START_TEST(test_array_delete_all_elements_no_underflow)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add 5 elements
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);

    // Delete all elements one by one from the end
    for (size_t i = 0; i < 5; i++) {
        ik_array_delete(array, 0);
        ck_assert_uint_eq(array->size, 4 - i);
    }

    // Verify size is exactly 0, not wrapped around to SIZE_MAX
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 10); // Capacity unchanged

    talloc_free(ctx);
}

END_TEST
// Security test: Complex interleaved insert/delete sequence
START_TEST(test_array_interleaved_insert_delete_stress)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Build initial array [0, 1, 2, 3, 4]
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete middle element -> [0, 1, 3, 4]
    ik_array_delete(array, 2);
    ck_assert_uint_eq(array->size, 4);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 3);

    // Insert at beginning -> [99, 0, 1, 3, 4]
    int32_t value = 99;
    res = ik_array_insert(array, 0, &value);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 99);

    // Delete from beginning -> [0, 1, 3, 4]
    ik_array_delete(array, 0);
    ck_assert_uint_eq(array->size, 4);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);

    // Insert in middle -> [0, 1, 88, 3, 4]
    value = 88;
    res = ik_array_insert(array, 2, &value);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 88);

    // Delete from end -> [0, 1, 88, 3]
    ik_array_delete(array, 4);
    ck_assert_uint_eq(array->size, 4);

    // Insert at end -> [0, 1, 88, 3, 77]
    value = 77;
    res = ik_array_insert(array, 4, &value);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);

    // Verify final state: [0, 1, 88, 3, 77]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 88);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 3);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 4), 77);

    talloc_free(ctx);
}

END_TEST
// Security test: Repeated insert/delete at same position
START_TEST(test_array_repeated_insert_delete_same_position)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Build initial array [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Repeatedly insert and delete at position 1
    for (int32_t i = 0; i < 10; i++) {
        int32_t value = 99 + i;
        res = ik_array_insert(array, 1, &value);
        ck_assert(is_ok(&res));
        ck_assert_uint_eq(array->size, 4);
        ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), value);

        ik_array_delete(array, 1);
        ck_assert_uint_eq(array->size, 3);
    }

    // Verify original elements intact: [0, 1, 2]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 2);

    talloc_free(ctx);
}

END_TEST

#ifndef NDEBUG
// Test assertion: delete with invalid index
START_TEST(test_array_delete_invalid_index_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    // Delete from empty array
    ik_array_delete(array, 0);

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite setup
static Suite *array_delete_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Array Delete");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_array_delete_from_beginning);
    tcase_add_test(tc_core, test_array_delete_from_middle);
    tcase_add_test(tc_core, test_array_delete_from_end);
    tcase_add_test(tc_core, test_array_delete_all_elements_no_underflow);
    tcase_add_test(tc_core, test_array_interleaved_insert_delete_stress);
    tcase_add_test(tc_core, test_array_repeated_insert_delete_same_position);

#ifndef NDEBUG
    // Assertion tests - only in debug builds
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_array_delete_invalid_index_asserts, SIGABRT);
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

    s = array_delete_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
