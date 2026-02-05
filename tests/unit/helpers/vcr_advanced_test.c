#include "tests/test_constants.h"
/**
 * @file vcr_advanced_test.c
 * @brief Unit tests for VCR advanced features (redaction, assertions, verification)
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tests/helpers/vcr_helper.h"

// Test fixture paths
#define TEST_FIXTURE_DIR "tests/fixtures/vcr/test"

// Helper to create test fixture directory
static void setup_fixture_dir(void)
{
    mkdir("tests/fixtures/vcr/test", 0755);
}

// Helper to clean up test fixtures
static void cleanup_test_fixtures(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/test_redact_bearer.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_redact_apikey.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_redact_case.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_redact_other.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_assert_playback.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_assert_recording.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_verify_match.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_skip_verify.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    rmdir(TEST_FIXTURE_DIR);
}

// Helper to create a test fixture file
static void create_test_fixture(const char *name, const char *content)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.jsonl", TEST_FIXTURE_DIR, name);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(content, fp);
        fclose(fp);
    }
}

START_TEST(test_vcr_credential_redaction_authorization_bearer) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_redact_bearer", "test");
    vcr_record_request("POST", "http://test.com",
                       "Authorization: Bearer sk-proj-test123", NULL);
    vcr_finish();

    unsetenv("VCR_RECORD");

    char path[256];
    snprintf(path, sizeof(path), "%s/test_redact_bearer.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    fgets(line, sizeof(line), fp);
    ck_assert(strstr(line, "Bearer REDACTED") != NULL);
    ck_assert(strstr(line, "sk-proj-test123") == NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_credential_redaction_x_api_key) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_redact_apikey", "test");
    vcr_record_request("POST", "http://test.com",
                       "x-api-key: sk-ant-secret123", NULL);
    vcr_finish();

    unsetenv("VCR_RECORD");

    char path[256];
    snprintf(path, sizeof(path), "%s/test_redact_apikey.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    fgets(line, sizeof(line), fp);
    ck_assert(strstr(line, "REDACTED") != NULL);
    ck_assert(strstr(line, "sk-ant-secret123") == NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_credential_redaction_case_insensitive) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_redact_case", "test");
    vcr_record_request("POST", "http://test.com",
                       "X-API-KEY: secret123", NULL);
    vcr_finish();

    unsetenv("VCR_RECORD");

    char path[256];
    snprintf(path, sizeof(path), "%s/test_redact_case.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    fgets(line, sizeof(line), fp);
    ck_assert(strstr(line, "REDACTED") != NULL);
    ck_assert(strstr(line, "secret123") == NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_credential_redaction_other_headers) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_redact_other", "test");
    vcr_record_request("POST", "http://test.com",
                       "content-type: application/json\nuser-agent: test", NULL);
    vcr_finish();

    unsetenv("VCR_RECORD");

    char path[256];
    snprintf(path, sizeof(path), "%s/test_redact_other.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    fgets(line, sizeof(line), fp);
    ck_assert(strstr(line, "content-type: application/json") != NULL);
    ck_assert(strstr(line, "user-agent: test") != NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_assertion_macros_playback) {
    setup_fixture_dir();
    create_test_fixture("test_assert_playback", "{\"_chunk\": \"test\"}\n");

    vcr_init("test_assert_playback", "test");

    // In playback mode, assertions should work normally
    vcr_ck_assert(1 == 1);
    vcr_ck_assert_int_eq(42, 42);
    vcr_ck_assert_str_eq("test", "test");
    vcr_ck_assert_ptr_nonnull("not null");
    vcr_ck_assert_ptr_null(NULL);

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_assertion_macros_recording) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_assert_recording", "test");

    // In record mode, these should be no-ops (test passes even with false conditions)
    vcr_ck_assert(1 == 1);  // Still works
    vcr_ck_assert_int_eq(42, 42);  // Still works
    vcr_ck_assert_str_eq("a", "a");  // Still works

    vcr_finish();
    unsetenv("VCR_RECORD");
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_request_verification_match) {
    setup_fixture_dir();
    const char *fixture =
        "{\"_request\": {\"method\": \"GET\", \"url\": \"http://test.com/api\", \"headers\": \"\", \"body\": \"{\\\"key\\\":\\\"value\\\"}\"}}\n"
        "{\"_response\": {\"status\": 200, \"headers\": \"\"}}\n"
        "{\"_chunk\": \"result\"}\n";
    create_test_fixture("test_verify_match", fixture);

    vcr_init("test_verify_match", "test");

    // Should not print warnings for matching request
    vcr_verify_request("GET", "http://test.com/api", "{\"key\":\"value\"}");

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_skip_verification) {
    setup_fixture_dir();
    const char *fixture = "{\"_request\": {\"method\": \"GET\", \"url\": \"http://test.com\", \"headers\": \"\"}}\n"
                          "{\"_response\": {\"status\": 200, \"headers\": \"\"}}\n"
                          "{\"_chunk\": \"result\"}\n";
    create_test_fixture("test_skip_verify", fixture);

    vcr_init("test_skip_verify", "test");
    vcr_skip_request_verification();

    // Should not print warnings even for mismatched request
    vcr_verify_request("POST", "http://different.com", NULL);

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

static Suite *vcr_advanced_suite(void)
{
    Suite *s = suite_create("VCR Advanced");

    TCase *tc_redact = tcase_create("Credential Redaction");
    tcase_set_timeout(tc_redact, IK_TEST_TIMEOUT);
    tcase_add_test(tc_redact, test_vcr_credential_redaction_authorization_bearer);
    tcase_add_test(tc_redact, test_vcr_credential_redaction_x_api_key);
    tcase_add_test(tc_redact, test_vcr_credential_redaction_case_insensitive);
    tcase_add_test(tc_redact, test_vcr_credential_redaction_other_headers);
    suite_add_tcase(s, tc_redact);

    TCase *tc_assert = tcase_create("Assertion Macros");
    tcase_set_timeout(tc_assert, IK_TEST_TIMEOUT);
    tcase_add_test(tc_assert, test_vcr_assertion_macros_playback);
    tcase_add_test(tc_assert, test_vcr_assertion_macros_recording);
    suite_add_tcase(s, tc_assert);

    TCase *tc_verify = tcase_create("Request Verification");
    tcase_set_timeout(tc_verify, IK_TEST_TIMEOUT);
    tcase_add_test(tc_verify, test_vcr_request_verification_match);
    tcase_add_test(tc_verify, test_vcr_skip_verification);
    suite_add_tcase(s, tc_verify);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = vcr_advanced_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/helpers/vcr_advanced_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
