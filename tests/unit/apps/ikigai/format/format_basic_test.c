#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <talloc.h>

#include "apps/ikigai/format.h"
#include "tests/helpers/test_utils_helper.h"

// Test: Create format buffer successfully
START_TEST(test_format_buffer_create_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    ck_assert_ptr_nonnull(buf);
    ck_assert_ptr_eq(buf->parent, ctx);
    ck_assert_ptr_nonnull(buf->array);

    talloc_free(ctx);
}
END_TEST
// Test: Append empty string
START_TEST(test_format_append_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_append(buf, "");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
    ck_assert_uint_eq(ik_format_get_length(buf), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Append short string
START_TEST(test_format_append_short) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_append(buf, "Hello");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Hello");
    ck_assert_uint_eq(ik_format_get_length(buf), 5);

    talloc_free(ctx);
}

END_TEST
// Test: Append long string (requiring multiple reallocations)
START_TEST(test_format_append_long) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    // Create a string longer than initial buffer (32 bytes)
    // Use 200 bytes to ensure multiple reallocations
    char long_str[201];
    for (int32_t i = 0; i < 200; i++) {
        long_str[i] = (char)('A' + (i % 26));
    }
    long_str[200] = '\0';

    res_t res = ik_format_append(buf, long_str);
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, long_str);
    ck_assert_uint_eq(ik_format_get_length(buf), 200);

    talloc_free(ctx);
}

END_TEST
// Test: Multiple appends
START_TEST(test_format_append_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_append(buf, "Hello");
    ck_assert(is_ok(&res));

    res = ik_format_append(buf, " ");
    ck_assert(is_ok(&res));

    res = ik_format_append(buf, "World");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Hello World");
    ck_assert_uint_eq(ik_format_get_length(buf), 11);

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_appendf with simple format string
START_TEST(test_format_appendf_simple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_appendf(buf, "Hello %s", "World");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Hello World");
    ck_assert_uint_eq(ik_format_get_length(buf), 11);

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_appendf with multiple format specifiers
START_TEST(test_format_appendf_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_appendf(buf, "Count: %" PRId32 ", Size: %" PRIu64, (int32_t)42, (uint64_t)1024);
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Count: 42, Size: 1024");

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_appendf with large output
START_TEST(test_format_appendf_large) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    // Create a format string that will produce 200+ bytes
    res_t res = ik_format_appendf(buf, "%-200s", "X");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_uint_ge(ik_format_get_length(buf), 200);

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_appendf with empty format
START_TEST(test_format_appendf_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_appendf(buf, "");
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
    ck_assert_uint_eq(ik_format_get_length(buf), 0);

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_indent with zero indent
START_TEST(test_format_indent_zero) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_indent(buf, 0);
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
    ck_assert_uint_eq(ik_format_get_length(buf), 0);

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_indent with small indent
START_TEST(test_format_indent_small) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_indent(buf, 4);
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "    ");
    ck_assert_uint_eq(ik_format_get_length(buf), 4);

    talloc_free(ctx);
}

END_TEST
// Test: ik_format_indent with large indent
START_TEST(test_format_indent_large) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_indent(buf, 120);
    ck_assert(is_ok(&res));

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_uint_eq(ik_format_get_length(buf), 120);

    // Verify all spaces
    for (size_t i = 0; i < 120; i++) {
        ck_assert_int_eq(result[i], ' ');
    }

    talloc_free(ctx);
}

END_TEST
// Test: Get string from empty buffer
START_TEST(test_get_string_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    const char *result = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");

    talloc_free(ctx);
}

END_TEST
// Test: Get length before get_string (no null terminator yet)
START_TEST(test_get_length_before_string) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_append(buf, "Hello");
    ck_assert(is_ok(&res));

    // Get length BEFORE get_string - buffer not null-terminated yet
    size_t len = ik_format_get_length(buf);
    ck_assert_uint_eq(len, 5);

    talloc_free(ctx);
}

END_TEST
// Test: Get string twice (tests idempotence of null termination)
START_TEST(test_get_string_twice) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);

    res_t res = ik_format_append(buf, "Hello");
    ck_assert(is_ok(&res));

    const char *result1 = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result1);
    ck_assert_str_eq(result1, "Hello");

    const char *result2 = ik_format_get_string(buf);
    ck_assert_ptr_nonnull(result2);
    ck_assert_str_eq(result2, "Hello");
    ck_assert_ptr_eq(result1, result2);  // Should return same pointer

    talloc_free(ctx);
}

END_TEST

static Suite *format_basic_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Format Basic");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_format_buffer_create_success);
    tcase_add_test(tc_core, test_format_append_empty);
    tcase_add_test(tc_core, test_format_append_short);
    tcase_add_test(tc_core, test_format_append_long);
    tcase_add_test(tc_core, test_format_append_multiple);
    tcase_add_test(tc_core, test_format_appendf_simple);
    tcase_add_test(tc_core, test_format_appendf_multiple);
    tcase_add_test(tc_core, test_format_appendf_large);
    tcase_add_test(tc_core, test_format_appendf_empty);
    tcase_add_test(tc_core, test_format_indent_zero);
    tcase_add_test(tc_core, test_format_indent_small);
    tcase_add_test(tc_core, test_format_indent_large);
    tcase_add_test(tc_core, test_get_string_empty);
    tcase_add_test(tc_core, test_get_length_before_string);
    tcase_add_test(tc_core, test_get_string_twice);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = format_basic_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/format/format_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
