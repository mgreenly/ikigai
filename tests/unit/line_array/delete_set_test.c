#include <check.h>
#include <talloc.h>
#include <signal.h>
#include <string.h>
#include <inttypes.h>
#include "../../../src/line_array.h"
#include "../../test_utils.h"

// Test delete from beginning
START_TEST(test_line_array_delete_from_beginning) {
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
START_TEST(test_line_array_delete_from_middle) {
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
START_TEST(test_line_array_delete_from_end) {
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
START_TEST(test_line_array_set) {
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

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test assertion on get with out of bounds index
START_TEST(test_line_array_get_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    ik_line_array_get(array, 0); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on delete with out of bounds index
START_TEST(test_line_array_delete_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    ik_line_array_delete(array, 0); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on set with out of bounds index
START_TEST(test_line_array_set_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    char *line = talloc_strdup(ctx, "test");
    ik_line_array_set(array, 0, line); // Empty array - should assert

    talloc_free(ctx);
}

END_TEST
// Test assertion on insert with out of bounds index
START_TEST(test_line_array_insert_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_line_array_create(ctx, 10);
    ik_line_array_t *array = res.ok;

    char *line = talloc_strdup(ctx, "test");
    ik_line_array_insert(array, 2, line); // Can only insert at 0 for empty array - should assert

    talloc_free(ctx);
}

END_TEST
#endif

static Suite *line_array_delete_set_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("LineArray_DeleteSet");
    tc_core = tcase_create("Core");

    // Delete tests
    tcase_add_test(tc_core, test_line_array_delete_from_beginning);
    tcase_add_test(tc_core, test_line_array_delete_from_middle);
    tcase_add_test(tc_core, test_line_array_delete_from_end);

    // Set test
    tcase_add_test(tc_core, test_line_array_set);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    // Assertion tests (debug mode only)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_line_array_get_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_line_array_delete_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_line_array_set_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_line_array_insert_out_of_bounds_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = line_array_delete_set_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
