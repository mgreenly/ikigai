#include <check.h>
#include <talloc.h>
#include <signal.h>
#include <string.h>
#include <inttypes.h>
#include "../../../src/line_array.h"
#include "../../test_utils.h"

// Test appending to empty array (first allocation)
START_TEST(test_line_array_append_first) {
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
START_TEST(test_line_array_append_no_growth) {
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
START_TEST(test_line_array_append_with_growth) {
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
// Test insert at beginning
START_TEST(test_line_array_insert_at_beginning) {
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
START_TEST(test_line_array_insert_in_middle) {
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
START_TEST(test_line_array_insert_at_end) {
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
START_TEST(test_line_array_insert_with_growth) {
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

static Suite *line_array_append_insert_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("LineArray_AppendInsert");
    tc_core = tcase_create("Core");

    // Append tests
    tcase_add_test(tc_core, test_line_array_append_first);
    tcase_add_test(tc_core, test_line_array_append_no_growth);
    tcase_add_test(tc_core, test_line_array_append_with_growth);

    // Insert tests
    tcase_add_test(tc_core, test_line_array_insert_at_beginning);
    tcase_add_test(tc_core, test_line_array_insert_in_middle);
    tcase_add_test(tc_core, test_line_array_insert_at_end);
    tcase_add_test(tc_core, test_line_array_insert_with_growth);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = line_array_append_insert_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
