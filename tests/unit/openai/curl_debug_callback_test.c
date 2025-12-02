/* Unit tests for curl debug callback filtering HTTP/2 noise and redacting secrets */

#include "client_multi_test_common.h"
#include "openai/client_multi_callbacks.h"

#include <string.h>
#include <stdio.h>

/*
 * Test that Authorization header is redacted
 */
START_TEST(test_curl_debug_callback_redacts_authorization) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate Authorization header from curl
    char auth_data[] = "Authorization: Bearer sk-proj-secret-key-here\r\n";

    // CURLINFO_HEADER_OUT = 2
    int32_t result = ik_openai_curl_debug_output(NULL, 2, auth_data, sizeof(auth_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    ck_assert(strstr(buffer, "[REDACTED]") != NULL);
    ck_assert(strstr(buffer, "sk-proj-secret-key-here") == NULL);
    ck_assert(strstr(buffer, ">> Authorization") != NULL);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_passes_through_other_headers) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate Content-Type header from curl
    char content_type_data[] = "Content-Type: application/json\r\n";

    // CURLINFO_HEADER_OUT = 2
    int32_t result = ik_openai_curl_debug_output(NULL, 2, content_type_data, sizeof(content_type_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    ck_assert(strstr(buffer, "Content-Type: application/json") != NULL);
    ck_assert(strstr(buffer, ">> ") != NULL);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_filters_ssl_data) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate SSL data from curl
    char ssl_data[] = "SSL: Some SSL protocol data";

    // CURLINFO_SSL_DATA_IN = 5
    int32_t result = ik_openai_curl_debug_output(NULL, 5, ssl_data, sizeof(ssl_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output - should be empty (filtered)
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);

    ck_assert_int_eq((int)bytes_read, 0);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_filters_text_info) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate informational text from curl
    char text_data[] = "* Trying 104.18.7.192...\r\n";

    // CURLINFO_TEXT = 0
    int32_t result = ik_openai_curl_debug_output(NULL, 0, text_data, sizeof(text_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output - should be empty (filtered)
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);

    ck_assert_int_eq((int)bytes_read, 0);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_case_insensitive_auth_match) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Authorization with lowercase 'a'
    char auth_data[] = "authorization: Bearer sk-secret\r\n";

    // CURLINFO_HEADER_OUT = 2
    int32_t result = ik_openai_curl_debug_output(NULL, 2, auth_data, sizeof(auth_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    ck_assert(strstr(buffer, "[REDACTED]") != NULL);
    ck_assert(strstr(buffer, "sk-secret") == NULL);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_response_headers_with_prefix) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate response header from curl
    char resp_header[] = "content-type: application/json\r\n";

    // CURLINFO_HEADER_IN = 1 (incoming/response)
    int32_t result = ik_openai_curl_debug_output(NULL, 1, resp_header, sizeof(resp_header) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    // Should have response prefix
    ck_assert(strstr(buffer, "<< ") != NULL);
    ck_assert(strstr(buffer, "content-type") != NULL);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_data_out_prefix) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate request body data
    char body_data[] = "{\"model\":\"gpt-4\"}";

    // CURLINFO_DATA_OUT = 4
    int32_t result = ik_openai_curl_debug_output(NULL, 4, body_data, sizeof(body_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    ck_assert(strstr(buffer, ">> ") != NULL);
    ck_assert(strstr(buffer, "{\"model\":\"gpt-4\"}") != NULL);

    fclose(output);
}
END_TEST

START_TEST(test_curl_debug_callback_data_in_prefix) {
    FILE *output = tmpfile();
    ck_assert(output != NULL);

    // Simulate response body data
    char body_data[] = "{\"id\":\"chatcmpl-123\"}";

    // CURLINFO_DATA_IN = 3
    int32_t result = ik_openai_curl_debug_output(NULL, 3, body_data, sizeof(body_data) - 1, output);

    ck_assert_int_eq(result, 0);

    // Read output
    rewind(output);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[bytes_read] = '\0';

    ck_assert(strstr(buffer, "<< ") != NULL);
    ck_assert(strstr(buffer, "{\"id\":\"chatcmpl-123\"}") != NULL);

    fclose(output);
}
END_TEST

/*
 * Test suite
 */

static Suite *curl_debug_callback_suite(void)
{
    Suite *s = suite_create("curl_debug_callback");

    TCase *tc_callback = tcase_create("callback");
    tcase_add_checked_fixture(tc_callback, setup, teardown);
    tcase_add_test(tc_callback, test_curl_debug_callback_redacts_authorization);
    tcase_add_test(tc_callback, test_curl_debug_callback_passes_through_other_headers);
    tcase_add_test(tc_callback, test_curl_debug_callback_filters_ssl_data);
    tcase_add_test(tc_callback, test_curl_debug_callback_filters_text_info);
    tcase_add_test(tc_callback, test_curl_debug_callback_case_insensitive_auth_match);
    tcase_add_test(tc_callback, test_curl_debug_callback_response_headers_with_prefix);
    tcase_add_test(tc_callback, test_curl_debug_callback_data_out_prefix);
    tcase_add_test(tc_callback, test_curl_debug_callback_data_in_prefix);
    suite_add_tcase(s, tc_callback);

    return s;
}

int main(void)
{
    Suite *s = curl_debug_callback_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
