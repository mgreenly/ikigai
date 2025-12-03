/* Comprehensive unit tests to achieve 100% branch coverage for ik_openai_curl_debug_output */

#include "client_multi_test_common.h"
#include "openai/client_multi_callbacks.h"

#include <string.h>
#include <stdio.h>

/*
 * Coverage gap: Line 47 - CURLINFO_SSL_DATA_OUT branch
 */
START_TEST(test_curl_debug_filters_ssl_data_out) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    char ssl_data[] = "SSL handshake data outgoing";

    /* CURLINFO_SSL_DATA_OUT = 6 - should be filtered */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_SSL_DATA_OUT,
                                                 ssl_data, sizeof(ssl_data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output - should be empty (filtered) */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);

    ck_assert_int_eq((int)bytes_read, 0);

    fclose(output);
}
END_TEST
/*
 * Coverage gap: Line 60 - CURLINFO_DATA_IN branch in else-if chain
 */
START_TEST(test_curl_debug_data_in_branch)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    char data[] = "Response body content";

    /* CURLINFO_DATA_IN = 3 - should use << prefix */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_DATA_IN,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should have response prefix */
    ck_assert(strstr(buffer, "<< ") != NULL);
    ck_assert(strstr(buffer, "Response body content") != NULL);

    fclose(output);
}

END_TEST
/*
 * Coverage gap: Line 72 - Loop with both \n and \r detection
 */
START_TEST(test_curl_debug_multiline_with_carriage_return)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Multi-line data with \r\n line endings */
    char data[] = "Line 1\r\nLine 2\r\nLine 3";

    /* CURLINFO_HEADER_OUT = 2 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_HEADER_OUT,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[512] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should have all three lines with >> prefix */
    ck_assert(strstr(buffer, ">> Line 1") != NULL);
    ck_assert(strstr(buffer, ">> Line 2") != NULL);
    ck_assert(strstr(buffer, ">> Line 3") != NULL);

    fclose(output);
}

END_TEST
/*
 * Coverage gap: Line 72 - Loop with only \n detection
 */
START_TEST(test_curl_debug_multiline_with_newline_only)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Multi-line data with \n line endings only */
    char data[] = "First line\nSecond line\nThird line";

    /* CURLINFO_DATA_OUT = 4 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_DATA_OUT,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[512] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should have all three lines with >> prefix */
    ck_assert(strstr(buffer, ">> First line") != NULL);
    ck_assert(strstr(buffer, ">> Second line") != NULL);
    ck_assert(strstr(buffer, ">> Third line") != NULL);

    fclose(output);
}

END_TEST
/*
 * Coverage gap: Line 79 - CURLINFO_HEADER_IN in Authorization check
 */
START_TEST(test_curl_debug_redacts_authorization_header_in)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Authorization header in response (rare but possible) */
    char auth_data[] = "Authorization: Bearer sk-incoming-secret\r\n";

    /* CURLINFO_HEADER_IN = 1 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_HEADER_IN,
                                                 auth_data, sizeof(auth_data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should be redacted */
    ck_assert(strstr(buffer, "[REDACTED]") != NULL);
    ck_assert(strstr(buffer, "sk-incoming-secret") == NULL);
    ck_assert(strstr(buffer, "<< Authorization") != NULL);

    fclose(output);
}

END_TEST
/*
 * Coverage gap: Line 84 - line_len == 0 (empty line, false branch)
 */
START_TEST(test_curl_debug_empty_line)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Empty line followed by content */
    char data[] = "\nActual content";

    /* CURLINFO_HEADER_OUT = 2 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_HEADER_OUT,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Empty line should NOT produce output, only "Actual content" */
    ck_assert(strstr(buffer, ">> Actual content") != NULL);

    /* Count newlines - should only be one (from "Actual content") */
    int newline_count = 0;
    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\n') newline_count++;
    }
    ck_assert_int_eq(newline_count, 1);

    fclose(output);
}

END_TEST
/*
 * Coverage gap: Line 91 - ptr < end loop and newline character checks
 * Test data that ends without a newline
 */
START_TEST(test_curl_debug_data_without_trailing_newline)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Data with no trailing newline */
    char data[] = "No newline at end";

    /* CURLINFO_DATA_OUT = 4 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_DATA_OUT,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should still output the line */
    ck_assert(strstr(buffer, ">> No newline at end") != NULL);

    fclose(output);
}

END_TEST
/*
 * Coverage gap: Line 91 - Test both \n and \r in newline skipping
 */
START_TEST(test_curl_debug_mixed_newline_types)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Mixed newline types: \r\n and \n */
    char data[] = "Line 1\r\nLine 2\nLine 3\r\n";

    /* CURLINFO_HEADER_IN = 1 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_HEADER_IN,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[512] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should have all three lines */
    ck_assert(strstr(buffer, "<< Line 1") != NULL);
    ck_assert(strstr(buffer, "<< Line 2") != NULL);
    ck_assert(strstr(buffer, "<< Line 3") != NULL);

    fclose(output);
}

END_TEST
/*
 * Edge case: Multiple empty lines
 */
START_TEST(test_curl_debug_multiple_empty_lines)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Multiple empty lines */
    char data[] = "\n\n\nContent\n\n";

    /* CURLINFO_DATA_IN = 3 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_DATA_IN,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should only see the "Content" line */
    ck_assert(strstr(buffer, "<< Content") != NULL);

    /* Count newlines - should only be one */
    int newline_count = 0;
    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\n') newline_count++;
    }
    ck_assert_int_eq(newline_count, 1);

    fclose(output);
}

END_TEST
/*
 * Edge case: Only \r line ending
 */
START_TEST(test_curl_debug_carriage_return_only)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Old Mac-style line ending (rare but valid) */
    char data[] = "Line 1\rLine 2\r";

    /* CURLINFO_HEADER_OUT = 2 */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_HEADER_OUT,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should have both lines */
    ck_assert(strstr(buffer, ">> Line 1") != NULL);
    ck_assert(strstr(buffer, ">> Line 2") != NULL);

    fclose(output);
}

END_TEST
/*
 * Edge case: Unknown/invalid curl info type (for 100% branch coverage)
 * This tests the false branch of line 60's "else if (type == CURLINFO_DATA_IN)"
 */
START_TEST(test_curl_debug_unknown_info_type)
{
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    /* Use an invalid type value (7 or higher, after CURLINFO_SSL_DATA_OUT = 6) */
    char data[] = "Some data";

    /* Type 7 is not a valid curl_infotype, but should not crash */
    int32_t result = ik_openai_curl_debug_output(NULL, (curl_infotype)7,
                                                 data, sizeof(data) - 1, output);

    ck_assert_int_eq(result, 0);

    /* Read output - should still output the line (with empty prefix since no match) */
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    /* Should have the data (with empty prefix) */
    ck_assert(strstr(buffer, "Some data") != NULL);

    fclose(output);
}

END_TEST

/*
 * Test suite
 */

static Suite *curl_debug_coverage_complete_suite(void)
{
    Suite *s = suite_create("curl_debug_coverage_complete");

    TCase *tc_coverage = tcase_create("complete_coverage");
    tcase_add_checked_fixture(tc_coverage, setup, teardown);

    /* Tests for specific coverage gaps */
    tcase_add_test(tc_coverage, test_curl_debug_filters_ssl_data_out);
    tcase_add_test(tc_coverage, test_curl_debug_data_in_branch);
    tcase_add_test(tc_coverage, test_curl_debug_multiline_with_carriage_return);
    tcase_add_test(tc_coverage, test_curl_debug_multiline_with_newline_only);
    tcase_add_test(tc_coverage, test_curl_debug_redacts_authorization_header_in);
    tcase_add_test(tc_coverage, test_curl_debug_empty_line);
    tcase_add_test(tc_coverage, test_curl_debug_data_without_trailing_newline);
    tcase_add_test(tc_coverage, test_curl_debug_mixed_newline_types);

    /* Additional edge cases */
    tcase_add_test(tc_coverage, test_curl_debug_multiple_empty_lines);
    tcase_add_test(tc_coverage, test_curl_debug_carriage_return_only);
    tcase_add_test(tc_coverage, test_curl_debug_unknown_info_type);

    suite_add_tcase(s, tc_coverage);

    return s;
}

int main(void)
{
    Suite *s = curl_debug_coverage_complete_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
