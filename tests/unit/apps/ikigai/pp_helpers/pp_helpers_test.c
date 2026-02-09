#include "tests/test_constants.h"
#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <string.h>
#include "apps/ikigai/pp_helpers.h"
#include "apps/ikigai/format.h"

// Test: ik_pp_header with valid inputs
START_TEST(test_pp_header_valid) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    void *ptr = (void *)0xDEADBEEF;
    ik_pp_header(buf, 0, "TestType", ptr);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "TestType @ 0xdeadbeef\n");

    talloc_free(tmp_ctx);
}
END_TEST
// Test: ik_pp_header with indentation
START_TEST(test_pp_header_indented) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    void *ptr = (void *)0x12345678;
    ik_pp_header(buf, 4, "IndentedType", ptr);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "    IndentedType @ 0x12345678\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_pointer with valid pointer
START_TEST(test_pp_pointer_valid) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    void *ptr = (void *)0xCAFEBABE;
    ik_pp_pointer(buf, 2, "test_ptr", ptr);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "  test_ptr: 0xcafebabe\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_pointer with NULL pointer
START_TEST(test_pp_pointer_null) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_pointer(buf, 2, "null_ptr", NULL);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "  null_ptr: NULL\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_size_t with various values
START_TEST(test_pp_size_t_values) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_size_t(buf, 0, "zero", 0);
    ik_pp_size_t(buf, 0, "small", 42);
    ik_pp_size_t(buf, 0, "large", 1234567890);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "zero: 0\nsmall: 42\nlarge: 1234567890\n");

    talloc_free(tmp_ctx);
}

END_TEST

// Test: ik_pp_uint32 with various values
START_TEST(test_pp_uint32_values) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_uint32(buf, 0, "zero", 0);
    ik_pp_uint32(buf, 0, "small", 42);
    ik_pp_uint32(buf, 0, "max", UINT32_MAX);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "zero: 0\nsmall: 42\nmax: 4294967295\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_string with simple string
START_TEST(test_pp_string_simple) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    const char *str = "Hello World";
    ik_pp_string(buf, 0, "message", str, strlen(str));

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "message: \"Hello World\"\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_string with special characters
START_TEST(test_pp_string_special_chars) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    const char *str = "Line1\nLine2\tTab\rReturn\\Backslash\"Quote";
    ik_pp_string(buf, 0, "special", str, strlen(str));

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "special: \"Line1\\nLine2\\tTab\\rReturn\\\\Backslash\\\"Quote\"\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_string with NULL string
START_TEST(test_pp_string_null) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_string(buf, 0, "null_str", NULL, 0);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "null_str: NULL\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_string with empty string
START_TEST(test_pp_string_empty) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_string(buf, 0, "empty", "", 0);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "empty: \"\"\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_string with control characters
START_TEST(test_pp_string_control_chars) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    // Control characters: NUL, BEL, DEL
    const char str[] = {0x00, 0x07, 0x7F, 'X', '\0'};
    ik_pp_string(buf, 0, "ctrl", str, 4);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "ctrl: \"\\x00\\x07\\x7fX\"\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: ik_pp_bool with true and false
START_TEST(test_pp_bool_values) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_bool(buf, 0, "flag_true", true);
    ik_pp_bool(buf, 0, "flag_false", false);

    const char *output = ik_format_get_string(buf);
    ck_assert_str_eq(output, "flag_true: true\nflag_false: false\n");

    talloc_free(tmp_ctx);
}

END_TEST
// Test: Indentation respected across all helpers
START_TEST(test_indentation_consistent) {
    void *tmp_ctx = talloc_new(NULL);
    ik_format_buffer_t *buf = ik_format_buffer_create(tmp_ctx);

    ik_pp_header(buf, 0, "Root", (void *)0x1000);
    ik_pp_size_t(buf, 2, "field1", 42);
    ik_pp_pointer(buf, 2, "field2", (void *)0x2000);
    ik_pp_bool(buf, 2, "field3", true);

    const char *output = ik_format_get_string(buf);
    const char *expected =
        "Root @ 0x1000\n"
        "  field1: 42\n"
        "  field2: 0x2000\n"
        "  field3: true\n";
    ck_assert_str_eq(output, expected);

    talloc_free(tmp_ctx);
}

END_TEST

// Test suite setup
static Suite *pp_helpers_suite(void)
{
    Suite *s = suite_create("pp_helpers");

    TCase *tc_header = tcase_create("header");
    tcase_set_timeout(tc_header, IK_TEST_TIMEOUT);
    tcase_add_test(tc_header, test_pp_header_valid);
    tcase_add_test(tc_header, test_pp_header_indented);
    suite_add_tcase(s, tc_header);

    TCase *tc_pointer = tcase_create("pointer");
    tcase_set_timeout(tc_pointer, IK_TEST_TIMEOUT);
    tcase_add_test(tc_pointer, test_pp_pointer_valid);
    tcase_add_test(tc_pointer, test_pp_pointer_null);
    suite_add_tcase(s, tc_pointer);

    TCase *tc_numeric = tcase_create("numeric");
    tcase_set_timeout(tc_numeric, IK_TEST_TIMEOUT);
    tcase_add_test(tc_numeric, test_pp_size_t_values);
    tcase_add_test(tc_numeric, test_pp_uint32_values);
    suite_add_tcase(s, tc_numeric);

    TCase *tc_string = tcase_create("string");
    tcase_set_timeout(tc_string, IK_TEST_TIMEOUT);
    tcase_add_test(tc_string, test_pp_string_simple);
    tcase_add_test(tc_string, test_pp_string_special_chars);
    tcase_add_test(tc_string, test_pp_string_null);
    tcase_add_test(tc_string, test_pp_string_empty);
    tcase_add_test(tc_string, test_pp_string_control_chars);
    suite_add_tcase(s, tc_string);

    TCase *tc_bool = tcase_create("bool");
    tcase_set_timeout(tc_bool, IK_TEST_TIMEOUT);
    tcase_add_test(tc_bool, test_pp_bool_values);
    suite_add_tcase(s, tc_bool);

    TCase *tc_integration = tcase_create("integration");
    tcase_set_timeout(tc_integration, IK_TEST_TIMEOUT);
    tcase_add_test(tc_integration, test_indentation_consistent);
    suite_add_tcase(s, tc_integration);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = pp_helpers_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/pp_helpers/pp_helpers_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
