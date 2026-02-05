#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "apps/ikigai/byte_array.h"
#include "tests/helpers/test_utils_helper.h"

// Test appending to empty array (first allocation)
START_TEST(test_byte_array_append_first) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    res = ik_byte_array_append(array, 42);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_byte_array_size(array), 1);

    ck_assert_uint_eq(ik_byte_array_get(array, 0), 42);

    talloc_free(ctx);
}

END_TEST
// Test appending multiple bytes within capacity
START_TEST(test_byte_array_append_no_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Append 5 bytes (within capacity of 10)
    for (uint8_t i = 0; i < 5; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(ik_byte_array_size(array), 5);

    // Verify values
    for (uint8_t i = 0; i < 5; i++) {
        ck_assert_uint_eq(ik_byte_array_get(array, i), i);
    }

    talloc_free(ctx);
}

END_TEST
// Test appending that triggers growth
START_TEST(test_byte_array_append_with_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 2);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Append 5 bytes: capacity goes 0 -> 2 -> 4 -> 8
    for (uint8_t i = 0; i < 5; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(ik_byte_array_size(array), 5);

    // Verify values survived growth
    for (uint8_t i = 0; i < 5; i++) {
        ck_assert_uint_eq(ik_byte_array_get(array, i), i);
    }

    talloc_free(ctx);
}

END_TEST
// Test insert at beginning
START_TEST(test_byte_array_insert_at_beginning) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add some values
    for (uint8_t i = 0; i < 3; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Insert at beginning
    res = ik_byte_array_insert(array, 0, 99);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_byte_array_size(array), 4);

    // Verify order: [99, 0, 1, 2]
    ck_assert_uint_eq(ik_byte_array_get(array, 0), 99);
    ck_assert_uint_eq(ik_byte_array_get(array, 1), 0);
    ck_assert_uint_eq(ik_byte_array_get(array, 2), 1);
    ck_assert_uint_eq(ik_byte_array_get(array, 3), 2);

    talloc_free(ctx);
}

END_TEST
// Test insert in middle
START_TEST(test_byte_array_insert_in_middle) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add values [0, 1, 2, 3]
    for (uint8_t i = 0; i < 4; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Insert 99 at index 2
    res = ik_byte_array_insert(array, 2, 99);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_byte_array_size(array), 5);

    // Verify order: [0, 1, 99, 2, 3]
    ck_assert_uint_eq(ik_byte_array_get(array, 0), 0);
    ck_assert_uint_eq(ik_byte_array_get(array, 1), 1);
    ck_assert_uint_eq(ik_byte_array_get(array, 2), 99);
    ck_assert_uint_eq(ik_byte_array_get(array, 3), 2);
    ck_assert_uint_eq(ik_byte_array_get(array, 4), 3);

    talloc_free(ctx);
}

END_TEST
// Test insert at end (same as append)
START_TEST(test_byte_array_insert_at_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add values [0, 1, 2]
    for (uint8_t i = 0; i < 3; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Insert at end (size = 3)
    res = ik_byte_array_insert(array, 3, 99);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_byte_array_size(array), 4);

    ck_assert_uint_eq(ik_byte_array_get(array, 3), 99);

    talloc_free(ctx);
}

END_TEST
// Test insert with growth
START_TEST(test_byte_array_insert_with_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 2);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Fill to capacity [0, 1]
    for (uint8_t i = 0; i < 2; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Insert requires growth
    res = ik_byte_array_insert(array, 1, 99);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_byte_array_size(array), 3);

    // Verify order: [0, 99, 1]
    ck_assert_uint_eq(ik_byte_array_get(array, 0), 0);
    ck_assert_uint_eq(ik_byte_array_get(array, 1), 99);
    ck_assert_uint_eq(ik_byte_array_get(array, 2), 1);

    talloc_free(ctx);
}

END_TEST

static Suite *byte_array_append_insert_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ByteArray_AppendInsert");
    tc_core = tcase_create("Core");

    // Append tests
    tcase_add_test(tc_core, test_byte_array_append_first);
    tcase_add_test(tc_core, test_byte_array_append_no_growth);
    tcase_add_test(tc_core, test_byte_array_append_with_growth);

    // Insert tests
    tcase_add_test(tc_core, test_byte_array_insert_at_beginning);
    tcase_add_test(tc_core, test_byte_array_insert_in_middle);
    tcase_add_test(tc_core, test_byte_array_insert_at_end);
    tcase_add_test(tc_core, test_byte_array_insert_with_growth);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = byte_array_append_insert_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/byte_array/append_insert_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
