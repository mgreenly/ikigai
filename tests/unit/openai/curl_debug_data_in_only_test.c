/* Unit test to ensure CURLINFO_DATA_IN branch is covered */

#include "openai/client_multi_internal.h"
#include "openai/client.h"
#include "config.h"
#include "error.h"
#include "wrapper.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Forward declaration of the function under test */
int32_t ik_openai_curl_debug_output(CURL *handle, curl_infotype type,
                                    char *data, size_t size, void *userptr);

/* Test context */
static void *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
}

/*
 * Test ONLY CURLINFO_DATA_IN to ensure branch 3 at line 66 is taken
 */
START_TEST(test_curlinfo_data_in_only) {
    char tmpfile[] = "/tmp/curl_debug_data_in_XXXXXX";
    int fd = mkstemp(tmpfile);
    ck_assert_int_ge(fd, 0);
    FILE *debug_output = fdopen(fd, "w+");
    ck_assert_ptr_ne(debug_output, NULL);

    char data[] = "response_body_data";

    /* Call with CURLINFO_DATA_IN - use the enum directly */
    int32_t result = ik_openai_curl_debug_output(NULL, CURLINFO_DATA_IN,
                                                  data, strlen(data),
                                                  debug_output);

    ck_assert_int_eq(result, 0);
    fflush(debug_output);

    /* Verify output */
    fseek(debug_output, 0, SEEK_SET);
    char buffer[256] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, debug_output);
    buffer[bytes_read] = '\0';

    /* Should have response prefix */
    ck_assert(strstr(buffer, "<< ") != NULL);
    ck_assert(strstr(buffer, "response_body_data") != NULL);

    fclose(debug_output);
    unlink(tmpfile);
}
END_TEST

/*
 * Test suite
 */

static Suite *curl_debug_data_in_only_suite(void)
{
    Suite *s = suite_create("curl_debug_data_in_only");

    TCase *tc_data_in = tcase_create("data_in_only");
    tcase_add_checked_fixture(tc_data_in, setup, teardown);
    tcase_add_test(tc_data_in, test_curlinfo_data_in_only);
    suite_add_tcase(s, tc_data_in);

    return s;
}

int main(void)
{
    Suite *s = curl_debug_data_in_only_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
