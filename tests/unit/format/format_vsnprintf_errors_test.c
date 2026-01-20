/**
 * @file format_vsnprintf_errors_test.c
 * @brief Tests for format.c vsnprintf error paths
 */

#include <check.h>
#include <stdarg.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/format.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

// Test fixture
static TALLOC_CTX *ctx;
static ik_format_buffer_t *buf;

// Mock state
static bool mock_vsnprintf_should_fail = false;
static int mock_vsnprintf_call_count = 0;
static int mock_vsnprintf_fail_on_call = -1;  // Which call number to fail on (-1 = always fail if should_fail is true)
static bool mock_vsnprintf_truncate = false;

// Mock vsnprintf_ to inject failures
int vsnprintf_(char *str, size_t size, const char *format, va_list ap)
{
    mock_vsnprintf_call_count++;

    if (mock_vsnprintf_should_fail) {
        if (mock_vsnprintf_fail_on_call == -1) {
            // Always fail
            return -1;
        } else if (mock_vsnprintf_call_count == mock_vsnprintf_fail_on_call) {
            // Fail on specific call number
            return -1;
        }
    }

    if (mock_vsnprintf_truncate) {
        // Return a value >= size to simulate truncation
        // For both the size calculation pass (str==NULL) and the formatting pass
        int actual_size = vsnprintf(str, size, format, ap);
        if (mock_vsnprintf_call_count == 2 && str != NULL && size > 0) {
            // Second call is the actual formatting - report truncation
            return (int)size + 10;
        }
        return actual_size;
    }

    // Call real vsnprintf
    return vsnprintf(str, size, format, ap);
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    buf = ik_format_buffer_create(ctx);
    ck_assert_ptr_nonnull(buf);

    // Reset mock state
    mock_vsnprintf_should_fail = false;
    mock_vsnprintf_call_count = 0;
    mock_vsnprintf_fail_on_call = -1;
    mock_vsnprintf_truncate = false;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: vsnprintf size calculation failure (line 44)
START_TEST(test_vsnprintf_size_calc_failure) {
    // Mock vsnprintf to fail on first call only (call #1 is size calculation)
    mock_vsnprintf_should_fail = true;
    mock_vsnprintf_fail_on_call = 1;

    res_t res = ik_format_appendf(buf, "Hello %s", "World");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    const char *msg = error_message(res.err);
    ck_assert(strstr(msg, "vsnprintf size calculation failed") != NULL);
}
END_TEST
// Test: vsnprintf formatting failure (line 57)
START_TEST(test_vsnprintf_formatting_failure) {
    // Mock vsnprintf to fail on second call (call #2 is actual formatting)
    // First call (size calculation) should succeed
    mock_vsnprintf_should_fail = true;
    mock_vsnprintf_fail_on_call = 2;

    res_t res = ik_format_appendf(buf, "Hello %s", "World");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    const char *msg = error_message(res.err);
    ck_assert(strstr(msg, "vsnprintf formatting failed") != NULL);
}

END_TEST
// Test: vsnprintf truncation (line 61)
START_TEST(test_vsnprintf_truncation) {
    // Mock vsnprintf to return value >= buf_size (simulate truncation)
    mock_vsnprintf_truncate = true;

    res_t res = ik_format_appendf(buf, "Hello %s", "World");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    const char *msg = error_message(res.err);
    ck_assert(strstr(msg, "vsnprintf truncated output") != NULL);
}

END_TEST
// Test: Successful format after size calculation failure test
START_TEST(test_format_success_after_error) {
    // First, trigger an error
    mock_vsnprintf_should_fail = true;
    mock_vsnprintf_fail_on_call = 1;

    res_t res = ik_format_appendf(buf, "Error %s", "test");
    ck_assert(is_err(&res));

    // Reset mock state and try again with a fresh buffer
    talloc_free(ctx);
    ctx = talloc_new(NULL);
    buf = ik_format_buffer_create(ctx);
    mock_vsnprintf_should_fail = false;
    mock_vsnprintf_call_count = 0;
    mock_vsnprintf_fail_on_call = -1;

    // Should succeed now
    res = ik_format_appendf(buf, "Success %s", "test");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_str_eq(result, "Success test");
}

END_TEST

static Suite *format_vsnprintf_errors_suite(void)
{
    Suite *s = suite_create("Format vsnprintf Errors");
    TCase *tc = tcase_create("vsnprintf failures");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_vsnprintf_size_calc_failure);
    tcase_add_test(tc, test_vsnprintf_formatting_failure);
    tcase_add_test(tc, test_vsnprintf_truncation);
    tcase_add_test(tc, test_format_success_after_error);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = format_vsnprintf_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
