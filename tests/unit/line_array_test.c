#include <check.h>
#include <talloc.h>
#include <signal.h>
#include <string.h>
#include <inttypes.h>
#include "../../src/line_array.h"
#include "../test_utils.h"

// Test successful line array creation
START_TEST(test_line_array_create_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);

    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;
    ck_assert_ptr_nonnull(array);
    ck_assert_uint_eq(ik_line_array_size(array), 0);
    ck_assert_uint_eq(ik_line_array_capacity(array), 0);

    talloc_free(ctx);
}
END_TEST
// Test line array creation with invalid increment (0)
START_TEST(test_line_array_create_invalid_increment)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 0);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST
// Test OOM during line array creation
START_TEST(test_line_array_create_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_next_alloc();
    res_t res = ik_line_array_create(ctx, 10);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    talloc_free(ctx);
}

END_TEST
// Test appending to empty array (first allocation)
START_TEST(test_line_array_append_first)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    char *line = talloc_strdup(ctx, "first line");
    res = ik_line_array_append(array, line);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_line_array_size(array), 1);
    ck_assert_uint_eq(ik_line_array_capacity(array), 10);

    ck_assert_str_eq(ik_line_array_get(array, 0), "first line");

    talloc_free(ctx);
}

END_TEST
// Test appending multiple lines within capacity
START_TEST(test_line_array_append_no_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Append 5 lines (within capacity of 10)
    for (size_t i = 0; i < 5; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(ik_line_array_size(array), 5);
    ck_assert_uint_eq(ik_line_array_capacity(array), 10);

    // Verify values
    for (size_t i = 0; i < 5; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "line %" PRIuMAX, (uintmax_t)i);
        ck_assert_str_eq(ik_line_array_get(array, i), expected);
    }

    talloc_free(ctx);
}

END_TEST
// Test appending that triggers growth
START_TEST(test_line_array_append_with_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 2);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Append 5 lines: capacity goes 0 -> 2 -> 4 -> 8
    for (size_t i = 0; i < 5; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(ik_line_array_size(array), 5);
    ck_assert_uint_eq(ik_line_array_capacity(array), 8);

    // Verify values survived growth
    for (size_t i = 0; i < 5; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "line %" PRIuMAX, (uintmax_t)i);
        ck_assert_str_eq(ik_line_array_get(array, i), expected);
    }

    talloc_free(ctx);
}

END_TEST
// Test OOM during first allocation in append
START_TEST(test_line_array_append_oom_first_alloc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    char *line = talloc_strdup(ctx, "test");
    oom_test_fail_next_alloc();
    res = ik_line_array_append(array, line);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    ck_assert_uint_eq(ik_line_array_size(array), 0);
    ck_assert_uint_eq(ik_line_array_capacity(array), 0);

    talloc_free(ctx);
}

END_TEST
// Test OOM during growth realloc
START_TEST(test_line_array_append_oom_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 2);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Fill to capacity
    for (size_t i = 0; i < 2; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Try to append with OOM during growth
    char *line = talloc_strdup(ctx, "overflow");
    oom_test_fail_next_alloc();
    res = ik_line_array_append(array, line);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    ck_assert_uint_eq(ik_line_array_size(array), 2);
    ck_assert_uint_eq(ik_line_array_capacity(array), 2);

    talloc_free(ctx);
}

END_TEST
// Test insert at beginning
START_TEST(test_line_array_insert_at_beginning)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add some values
    for (size_t i = 0; i < 3; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Insert at beginning
    char *new_line = talloc_strdup(ctx, "inserted");
    res = ik_line_array_insert(array, 0, new_line);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_line_array_size(array), 4);

    // Verify order: [inserted, line 0, line 1, line 2]
    ck_assert_str_eq(ik_line_array_get(array, 0), "inserted");
    ck_assert_str_eq(ik_line_array_get(array, 1), "line 0");
    ck_assert_str_eq(ik_line_array_get(array, 2), "line 1");
    ck_assert_str_eq(ik_line_array_get(array, 3), "line 2");

    talloc_free(ctx);
}

END_TEST
// Test insert in middle
START_TEST(test_line_array_insert_in_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add values [line 0, line 1, line 2, line 3]
    for (size_t i = 0; i < 4; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Insert at index 2
    char *new_line = talloc_strdup(ctx, "inserted");
    res = ik_line_array_insert(array, 2, new_line);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_line_array_size(array), 5);

    // Verify order: [line 0, line 1, inserted, line 2, line 3]
    ck_assert_str_eq(ik_line_array_get(array, 0), "line 0");
    ck_assert_str_eq(ik_line_array_get(array, 1), "line 1");
    ck_assert_str_eq(ik_line_array_get(array, 2), "inserted");
    ck_assert_str_eq(ik_line_array_get(array, 3), "line 2");
    ck_assert_str_eq(ik_line_array_get(array, 4), "line 3");

    talloc_free(ctx);
}

END_TEST
// Test insert at end (same as append)
START_TEST(test_line_array_insert_at_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add values [line 0, line 1, line 2]
    for (int32_t i = 0; i < 3; i++) {
        char *line = talloc_asprintf(ctx, "line %d", i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Insert at end (size = 3)
    char *new_line = talloc_strdup(ctx, "inserted");
    res = ik_line_array_insert(array, 3, new_line);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_line_array_size(array), 4);

    ck_assert_str_eq(ik_line_array_get(array, 3), "inserted");

    talloc_free(ctx);
}

END_TEST
// Test insert with growth
START_TEST(test_line_array_insert_with_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 2);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Fill to capacity [line 0, line 1]
    for (int32_t i = 0; i < 2; i++) {
        char *line = talloc_asprintf(ctx, "line %d", i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Insert requires growth
    char *new_line = talloc_strdup(ctx, "inserted");
    res = ik_line_array_insert(array, 1, new_line);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_line_array_size(array), 3);
    ck_assert_uint_eq(ik_line_array_capacity(array), 4);

    // Verify order: [line 0, inserted, line 1]
    ck_assert_str_eq(ik_line_array_get(array, 0), "line 0");
    ck_assert_str_eq(ik_line_array_get(array, 1), "inserted");
    ck_assert_str_eq(ik_line_array_get(array, 2), "line 1");

    talloc_free(ctx);
}

END_TEST
// Test OOM during insert
START_TEST(test_line_array_insert_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 2);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Fill to capacity
    for (size_t i = 0; i < 2; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Try insert with OOM
    char *new_line = talloc_strdup(ctx, "inserted");
    oom_test_fail_next_alloc();
    res = ik_line_array_insert(array, 0, new_line);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    // Array unchanged
    ck_assert_uint_eq(ik_line_array_size(array), 2);
    ck_assert_uint_eq(ik_line_array_capacity(array), 2);

    talloc_free(ctx);
}

END_TEST
// Test delete from beginning
START_TEST(test_line_array_delete_from_beginning)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add [line 0, line 1, line 2, line 3]
    for (size_t i = 0; i < 4; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Delete first element
    ik_line_array_delete(array, 0);

    ck_assert_uint_eq(ik_line_array_size(array), 3);

    // Verify remaining: [line 1, line 2, line 3]
    ck_assert_str_eq(ik_line_array_get(array, 0), "line 1");
    ck_assert_str_eq(ik_line_array_get(array, 1), "line 2");
    ck_assert_str_eq(ik_line_array_get(array, 2), "line 3");

    talloc_free(ctx);
}

END_TEST
// Test delete from middle
START_TEST(test_line_array_delete_from_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add [line 0, line 1, line 2, line 3]
    for (size_t i = 0; i < 4; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Delete element at index 2
    ik_line_array_delete(array, 2);

    ck_assert_uint_eq(ik_line_array_size(array), 3);

    // Verify remaining: [line 0, line 1, line 3]
    ck_assert_str_eq(ik_line_array_get(array, 0), "line 0");
    ck_assert_str_eq(ik_line_array_get(array, 1), "line 1");
    ck_assert_str_eq(ik_line_array_get(array, 2), "line 3");

    talloc_free(ctx);
}

END_TEST
// Test delete from end
START_TEST(test_line_array_delete_from_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add [line 0, line 1, line 2, line 3]
    for (size_t i = 0; i < 4; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Delete last element
    ik_line_array_delete(array, 3);

    ck_assert_uint_eq(ik_line_array_size(array), 3);

    // Verify remaining: [line 0, line 1, line 2]
    for (size_t i = 0; i < 3; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "line %" PRIuMAX, (uintmax_t)i);
        ck_assert_str_eq(ik_line_array_get(array, i), expected);
    }

    talloc_free(ctx);
}

END_TEST
// Test set line
START_TEST(test_line_array_set)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add [line 0, line 1, line 2]
    for (size_t i = 0; i < 3; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Set middle element
    char *new_line = talloc_strdup(ctx, "replaced");
    ik_line_array_set(array, 1, new_line);

    // Verify: [line 0, replaced, line 2]
    ck_assert_str_eq(ik_line_array_get(array, 0), "line 0");
    ck_assert_str_eq(ik_line_array_get(array, 1), "replaced");
    ck_assert_str_eq(ik_line_array_get(array, 2), "line 2");

    talloc_free(ctx);
}

END_TEST
// Test clear
START_TEST(test_line_array_clear)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    // Add some lines
    for (size_t i = 0; i < 5; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    // Clear array
    ik_line_array_clear(array);

    ck_assert_uint_eq(ik_line_array_size(array), 0);
    ck_assert_uint_eq(ik_line_array_capacity(array), 10); // Capacity unchanged

    talloc_free(ctx);
}

END_TEST
// Test size and capacity queries
START_TEST(test_line_array_size_capacity)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 5);
    ck_assert(is_ok(&res));
    ik_line_array_t *array = res.ok;

    ck_assert_uint_eq(ik_line_array_size(array), 0);
    ck_assert_uint_eq(ik_line_array_capacity(array), 0);

    // Add elements
    for (size_t i = 0; i < 7; i++) {
        char *line = talloc_asprintf(ctx, "line %" PRIuMAX, (uintmax_t)i);
        res = ik_line_array_append(array, line);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(ik_line_array_size(array), 7);
    ck_assert_uint_eq(ik_line_array_capacity(array), 10); // 5 -> 10

    talloc_free(ctx);
}

END_TEST

#ifndef NDEBUG
// Test assertion on NULL array for size
START_TEST(test_line_array_size_null_asserts)
{
    ik_line_array_size(NULL);
}

END_TEST
// Test assertion on NULL array for capacity
START_TEST(test_line_array_capacity_null_asserts)
{
    ik_line_array_capacity(NULL);
}

END_TEST
// Test assertion on get with out of bounds index
START_TEST(test_line_array_get_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    ik_line_array_get(array, 0); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on delete with out of bounds index
START_TEST(test_line_array_delete_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    ik_line_array_delete(array, 0); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on set with out of bounds index
START_TEST(test_line_array_set_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    char *line = talloc_strdup(ctx, "test");
    ik_line_array_set(array, 0, line); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on insert with out of bounds index
START_TEST(test_line_array_insert_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    char *line = talloc_strdup(ctx, "test");
    ik_line_array_insert(array, 2, line); // Can only insert at 0 for empty array - should assert

    talloc_free(ctx);
}

END_TEST
#endif

static Suite *line_array_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("LineArray");
    tc_core = tcase_create("Core");

    // Creation tests
    tcase_add_test(tc_core, test_line_array_create_success);
    tcase_add_test(tc_core, test_line_array_create_invalid_increment);
    tcase_add_test(tc_core, test_line_array_create_oom);

    // Append tests
    tcase_add_test(tc_core, test_line_array_append_first);
    tcase_add_test(tc_core, test_line_array_append_no_growth);
    tcase_add_test(tc_core, test_line_array_append_with_growth);
    tcase_add_test(tc_core, test_line_array_append_oom_first_alloc);
    tcase_add_test(tc_core, test_line_array_append_oom_growth);

    // Insert tests
    tcase_add_test(tc_core, test_line_array_insert_at_beginning);
    tcase_add_test(tc_core, test_line_array_insert_in_middle);
    tcase_add_test(tc_core, test_line_array_insert_at_end);
    tcase_add_test(tc_core, test_line_array_insert_with_growth);
    tcase_add_test(tc_core, test_line_array_insert_oom);

    // Delete tests
    tcase_add_test(tc_core, test_line_array_delete_from_beginning);
    tcase_add_test(tc_core, test_line_array_delete_from_middle);
    tcase_add_test(tc_core, test_line_array_delete_from_end);

    // Set test
    tcase_add_test(tc_core, test_line_array_set);

    // Clear test
    tcase_add_test(tc_core, test_line_array_clear);

    // Query tests
    tcase_add_test(tc_core, test_line_array_size_capacity);

#ifndef NDEBUG
    // Assertion tests (debug mode only)
    tcase_add_test_raise_signal(tc_core, test_line_array_size_null_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_line_array_capacity_null_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_line_array_get_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_line_array_delete_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_line_array_set_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_line_array_insert_out_of_bounds_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = line_array_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
