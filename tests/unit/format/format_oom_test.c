#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <talloc.h>

#include "../../../src/format.h"
#include "../../test_utils.h"

// Test: OOM during buffer creation
START_TEST(test_format_buffer_create_oom) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_next_alloc();

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST
// Test: OOM during byte array creation in buffer
START_TEST(test_format_buffer_create_oom_array)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail on second allocation (after buffer struct, during byte_array_create)
    // ik_talloc_zero_wrapper is called once for buffer, then again for byte_array
    oom_test_fail_after_n_calls(2);

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test: OOM during format_append (during growth)
START_TEST(test_format_append_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    // Fill up initial capacity (32 bytes)
    char str[33];
    for (int32_t i = 0; i < 32; i++) {
        str[i] = 'A';
    }
    str[32] = '\0';

    res = ik_format_append(buf, str);
    ck_assert(is_ok(&res));

    // Now try to append more, which will require growth
    oom_test_fail_next_alloc();
    res = ik_format_append(buf, "X");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test: OOM during format_appendf temp buffer allocation
START_TEST(test_format_appendf_oom_temp)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    // Fail temp buffer allocation
    oom_test_fail_next_alloc();
    res = ik_format_appendf(buf, "Hello %s", "World");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test: OOM during format_appendf character-by-character append loop
START_TEST(test_format_appendf_oom_append)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    // Fill up initial capacity (32 bytes) exactly
    char str[33];
    for (int32_t i = 0; i < 32; i++) {
        str[i] = 'A';
    }
    str[32] = '\0';

    res = ik_format_append(buf, str);
    ck_assert(is_ok(&res));

    // Now format_appendf will need to grow during the character loop
    // We need to fail the reallocation during byte_array growth, not the temp buffer
    // Create a longer string to ensure we hit growth mid-loop
    // Fail after 2 allocations: temp buffer (1), then array growth (2 - this one fails)
    oom_test_fail_after_n_calls(2);
    res = ik_format_appendf(buf, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");  // 36 characters

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test: OOM during format_indent
START_TEST(test_format_indent_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    // Fill up initial capacity (32 bytes)
    char str[33];
    for (int32_t i = 0; i < 32; i++) {
        str[i] = 'A';
    }
    str[32] = '\0';

    res = ik_format_append(buf, str);
    ck_assert(is_ok(&res));

    // Now try to indent, which will require growth
    oom_test_fail_next_alloc();
    res = ik_format_indent(buf, 10);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test: OOM during get_string null termination
START_TEST(test_get_string_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = NULL;
    res_t res = ik_format_buffer_create(ctx, &buf);
    ck_assert(is_ok(&res));

    // Fill up initial capacity (32 bytes) exactly
    char str[33];
    for (int32_t i = 0; i < 32; i++) {
        str[i] = 'A';
    }
    str[32] = '\0';

    res = ik_format_append(buf, str);
    ck_assert(is_ok(&res));

    // Now get_string needs to add null terminator, which requires growth
    oom_test_fail_next_alloc();
    const char *result = ik_format_get_string(buf);

    ck_assert_ptr_null(result);  // Should return NULL on OOM

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST

static Suite *format_oom_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Format OOM");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_format_buffer_create_oom);
    tcase_add_test(tc_core, test_format_buffer_create_oom_array);
    tcase_add_test(tc_core, test_format_append_oom);
    tcase_add_test(tc_core, test_format_appendf_oom_temp);
    tcase_add_test(tc_core, test_format_appendf_oom_append);
    tcase_add_test(tc_core, test_format_indent_oom);
    tcase_add_test(tc_core, test_get_string_oom);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = format_oom_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
