#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../../src/byte_array.h"
#include "../../test_utils.h"

// Test byte array creation with invalid increment (0)
START_TEST(test_byte_array_create_invalid_increment) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 0);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST
// Test clear
START_TEST(test_byte_array_clear) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_byte_array_create(ctx, 10);
    ck_assert(is_ok(&res));
    ik_byte_array_t *array = res.ok;

    // Add some bytes
    for (uint8_t i = 0; i < 5; i++) {
        res = ik_byte_array_append(array, i);
        ck_assert(is_ok(&res));
    }

    // Clear array
    ik_byte_array_clear(array);

    ck_assert_uint_eq(ik_byte_array_size(array), 0);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test assertion on NULL array for size
START_TEST(test_byte_array_size_null_asserts) {
    ik_byte_array_size(NULL);
}

END_TEST
#endif

static Suite *byte_array_basic_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ByteArray_Basic");
    tc_core = tcase_create("Core");

    // Creation tests
    tcase_add_test(tc_core, test_byte_array_create_invalid_increment);

    // Clear test
    tcase_add_test(tc_core, test_byte_array_clear);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    // Assertion tests (debug mode only)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_byte_array_size_null_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = byte_array_basic_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
