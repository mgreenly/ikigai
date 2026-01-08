/**
 * @file vcr_test.c
 * @brief Unit tests for VCR HTTP recording/replay basic operations
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../helpers/vcr.h"

// Test fixture paths
#define TEST_FIXTURE_DIR "tests/fixtures/vcr/test"
#define TEST_FIXTURE_SINGLE "test_single_chunk"
#define TEST_FIXTURE_MULTI "test_multi_chunk"
#define TEST_FIXTURE_BODY "test_body"

// Helper to create test fixture directory
static void setup_fixture_dir(void)
{
    mkdir("tests/fixtures/vcr/test", 0755);
}

// Helper to clean up test fixtures
static void cleanup_test_fixtures(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.jsonl", TEST_FIXTURE_DIR, TEST_FIXTURE_SINGLE);
    unlink(path);
    snprintf(path, sizeof(path), "%s/%s.jsonl", TEST_FIXTURE_DIR, TEST_FIXTURE_MULTI);
    unlink(path);
    snprintf(path, sizeof(path), "%s/%s.jsonl", TEST_FIXTURE_DIR, TEST_FIXTURE_BODY);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_mode.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_lifecycle.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_cycles.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_record_request.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_record_response.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_record_chunk.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_record_body.jsonl", TEST_FIXTURE_DIR);
    unlink(path);
    snprintf(path, sizeof(path), "%s/test_record_multi.jsonl", TEST_FIXTURE_DIR);
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

START_TEST(test_vcr_mode_detection_recording) {
    setenv("VCR_RECORD", "1", 1);
    setup_fixture_dir();

    vcr_init("test_mode", "test");
    ck_assert(vcr_is_recording() == true);
    ck_assert(vcr_recording == true);
    vcr_finish();

    unsetenv("VCR_RECORD");
    cleanup_test_fixtures();
}
END_TEST

START_TEST(test_vcr_mode_detection_playback) {
    unsetenv("VCR_RECORD");
    setup_fixture_dir();
    create_test_fixture("test_mode",
                        "{\"_request\": {\"method\": \"GET\", \"url\": \"http://test.com\", \"headers\": \"\"}}\n");

    vcr_init("test_mode", "test");
    ck_assert(vcr_is_recording() == false);
    ck_assert(vcr_recording == false);
    vcr_finish();

    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_lifecycle_single) {
    setup_fixture_dir();
    create_test_fixture("test_lifecycle", "{\"_chunk\": \"test\"}\n");

    ck_assert(vcr_is_active() == false);

    vcr_init("test_lifecycle", "test");
    ck_assert(vcr_is_active() == true);

    vcr_finish();
    ck_assert(vcr_is_active() == false);

    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_lifecycle_multiple_cycles) {
    setup_fixture_dir();
    create_test_fixture("test_cycles", "{\"_chunk\": \"test\"}\n");

    // First cycle
    vcr_init("test_cycles", "test");
    ck_assert(vcr_is_active() == true);
    vcr_finish();
    ck_assert(vcr_is_active() == false);

    // Second cycle
    vcr_init("test_cycles", "test");
    ck_assert(vcr_is_active() == true);
    vcr_finish();
    ck_assert(vcr_is_active() == false);

    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_playback_single_chunk) {
    setup_fixture_dir();
    const char *fixture = "{\"_request\": {\"method\": \"GET\", \"url\": \"http://test.com\", \"headers\": \"\"}}\n"
                          "{\"_response\": {\"status\": 200, \"headers\": \"content-type: text/plain\"}}\n"
                          "{\"_chunk\": \"Hello world\"}\n";
    create_test_fixture(TEST_FIXTURE_SINGLE, fixture);

    vcr_init(TEST_FIXTURE_SINGLE, "test");

    const char *data;
    size_t len;
    ck_assert(vcr_has_more() == true);
    ck_assert(vcr_next_chunk(&data, &len) == true);
    ck_assert_str_eq(data, "Hello world");
    ck_assert(len == 11);
    ck_assert(vcr_has_more() == false);
    ck_assert(vcr_next_chunk(&data, &len) == false);

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_playback_multiple_chunks) {
    setup_fixture_dir();
    const char *fixture =
        "{\"_request\": {\"method\": \"POST\", \"url\": \"http://api.test.com\", \"headers\": \"\"}}\n"
        "{\"_response\": {\"status\": 200, \"headers\": \"\"}}\n"
        "{\"_chunk\": \"First\"}\n"
        "{\"_chunk\": \"Second\"}\n"
        "{\"_chunk\": \"Third\"}\n";
    create_test_fixture(TEST_FIXTURE_MULTI, fixture);

    vcr_init(TEST_FIXTURE_MULTI, "test");

    const char *data;
    size_t len;

    ck_assert(vcr_next_chunk(&data, &len) == true);
    ck_assert_str_eq(data, "First");
    ck_assert(len == 5);

    ck_assert(vcr_next_chunk(&data, &len) == true);
    ck_assert_str_eq(data, "Second");
    ck_assert(len == 6);

    ck_assert(vcr_next_chunk(&data, &len) == true);
    ck_assert_str_eq(data, "Third");
    ck_assert(len == 5);

    ck_assert(vcr_has_more() == false);
    ck_assert(vcr_next_chunk(&data, &len) == false);

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_playback_body) {
    setup_fixture_dir();
    const char *fixture = "{\"_request\": {\"method\": \"GET\", \"url\": \"http://test.com\", \"headers\": \"\"}}\n"
                          "{\"_response\": {\"status\": 200, \"headers\": \"\"}}\n"
                          "{\"_body\": \"{\\\"result\\\":\\\"success\\\"}\"}\n";
    create_test_fixture(TEST_FIXTURE_BODY, fixture);

    vcr_init(TEST_FIXTURE_BODY, "test");

    const char *data;
    size_t len;
    ck_assert(vcr_next_chunk(&data, &len) == true);
    ck_assert_str_eq(data, "{\"result\":\"success\"}");

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_playback_missing_fixture) {
    setup_fixture_dir();

    // Should not crash with missing fixture
    vcr_init("nonexistent_fixture", "test");
    ck_assert(vcr_is_active() == true);

    const char *data;
    size_t len;
    ck_assert(vcr_has_more() == false);
    ck_assert(vcr_next_chunk(&data, &len) == false);

    vcr_finish();
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_record_request) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_record_request", "test");
    vcr_record_request("POST", "https://api.example.com/v1/messages",
                       "x-api-key: sk-ant-test123\ncontent-type: application/json",
                       "{\"test\":\"data\"}");
    vcr_finish();

    unsetenv("VCR_RECORD");

    // Read back and verify
    char path[256];
    snprintf(path, sizeof(path), "%s/test_record_request.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), fp));
    ck_assert(strstr(line, "\"method\": \"POST\"") != NULL);
    ck_assert(strstr(line, "https://api.example.com/v1/messages") != NULL);
    ck_assert(strstr(line, "REDACTED") != NULL);
    ck_assert(strstr(line, "sk-ant-test123") == NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_record_response) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_record_response", "test");
    vcr_record_response(200, "content-type: application/json\ncontent-length: 42");
    vcr_finish();

    unsetenv("VCR_RECORD");

    // Read back and verify
    char path[256];
    snprintf(path, sizeof(path), "%s/test_record_response.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), fp));
    ck_assert(strstr(line, "\"status\": 200") != NULL);
    ck_assert(strstr(line, "content-type: application/json") != NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_record_chunk) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_record_chunk", "test");
    vcr_record_chunk("event: message_start\ndata: {\"id\":\"123\"}\n\n", 41);
    vcr_finish();

    unsetenv("VCR_RECORD");

    // Read back and verify
    char path[256];
    snprintf(path, sizeof(path), "%s/test_record_chunk.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), fp));
    ck_assert(strstr(line, "\"_chunk\"") != NULL);
    ck_assert(strstr(line, "event: message_start") != NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_record_body) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_record_body", "test");
    vcr_record_body("{\"result\":\"success\"}", 20);
    vcr_finish();

    unsetenv("VCR_RECORD");

    // Read back and verify
    char path[256];
    snprintf(path, sizeof(path), "%s/test_record_body.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), fp));
    ck_assert(strstr(line, "\"_body\"") != NULL);
    ck_assert(strstr(line, "result") != NULL);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

START_TEST(test_vcr_record_multiple_chunks) {
    setup_fixture_dir();
    setenv("VCR_RECORD", "1", 1);

    vcr_init("test_record_multi", "test");
    vcr_record_chunk("First", 5);
    vcr_record_chunk("Second", 6);
    vcr_record_chunk("Third", 5);
    vcr_finish();

    unsetenv("VCR_RECORD");

    // Read back and verify
    char path[256];
    snprintf(path, sizeof(path), "%s/test_record_multi.jsonl", TEST_FIXTURE_DIR);
    FILE *fp = fopen(path, "r");
    ck_assert_ptr_nonnull(fp);

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        count++;
    }
    ck_assert_int_eq(count, 3);

    fclose(fp);
    cleanup_test_fixtures();
}

END_TEST

static Suite *vcr_suite(void)
{
    Suite *s = suite_create("VCR");

    TCase *tc_mode = tcase_create("Mode Detection");
    tcase_set_timeout(tc_mode, IK_TEST_TIMEOUT);
    tcase_add_test(tc_mode, test_vcr_mode_detection_recording);
    tcase_add_test(tc_mode, test_vcr_mode_detection_playback);
    suite_add_tcase(s, tc_mode);

    TCase *tc_lifecycle = tcase_create("Lifecycle");
    tcase_set_timeout(tc_lifecycle, IK_TEST_TIMEOUT);
    tcase_add_test(tc_lifecycle, test_vcr_lifecycle_single);
    tcase_add_test(tc_lifecycle, test_vcr_lifecycle_multiple_cycles);
    suite_add_tcase(s, tc_lifecycle);

    TCase *tc_playback = tcase_create("Playback");
    tcase_set_timeout(tc_playback, IK_TEST_TIMEOUT);
    tcase_add_test(tc_playback, test_vcr_playback_single_chunk);
    tcase_add_test(tc_playback, test_vcr_playback_multiple_chunks);
    tcase_add_test(tc_playback, test_vcr_playback_body);
    tcase_add_test(tc_playback, test_vcr_playback_missing_fixture);
    suite_add_tcase(s, tc_playback);

    TCase *tc_record = tcase_create("Recording");
    tcase_set_timeout(tc_record, IK_TEST_TIMEOUT);
    tcase_add_test(tc_record, test_vcr_record_request);
    tcase_add_test(tc_record, test_vcr_record_response);
    tcase_add_test(tc_record, test_vcr_record_chunk);
    tcase_add_test(tc_record, test_vcr_record_body);
    tcase_add_test(tc_record, test_vcr_record_multiple_chunks);
    suite_add_tcase(s, tc_record);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = vcr_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
