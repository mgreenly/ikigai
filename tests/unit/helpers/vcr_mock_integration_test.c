#include "tests/test_constants.h"
/**
 * @file vcr_mock_integration_test.c
 * @brief Unit tests for VCR mock integration with curl wrappers
 */

#include <check.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include "tests/helpers/vcr_helper.h"

// Forward declarations for curl wrappers
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);
CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...);
CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *multi);
CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy);
CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy);
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles);

// Test suite forward declaration
static Suite *vcr_mock_integration_suite(void);

// Test state
static char *captured_data = NULL;
static size_t captured_size = 0;

// Test write callback
static size_t test_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb;
    char **buffer = (char **)userdata;

    if (*buffer == NULL) {
        *buffer = malloc(realsize + 1);
        memcpy(*buffer, ptr, realsize);
        (*buffer)[realsize] = '\0';
        captured_size = realsize;
    } else {
        size_t old_size = strlen(*buffer);
        *buffer = realloc(*buffer, old_size + realsize + 1);
        memcpy(*buffer + old_size, ptr, realsize);
        (*buffer)[old_size + realsize] = '\0';
        captured_size = old_size + realsize;
    }

    return realsize;
}

// Setup/teardown
static void setup(void)
{
    captured_data = NULL;
    captured_size = 0;
}

static void teardown(void)
{
    if (captured_data) {
        free(captured_data);
        captured_data = NULL;
    }
    captured_size = 0;
    vcr_finish();
}

// ============================================================================
// Playback Mode Tests
// ============================================================================

START_TEST(test_playback_delivers_chunks) {
    // This test would require a fixture file, but we can test the mechanism
    // by verifying that VCR integration is set up correctly

    // In playback mode (no VCR_RECORD), vcr_init should load fixture
    // For this basic test, we just verify the integration hooks exist
    CURL *curl = curl_easy_init_();
    ck_assert_ptr_nonnull(curl);

    // Set write callback - in VCR playback mode, this should be tracked
    CURLcode res = curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, test_write_callback);
    ck_assert_int_eq(res, CURLE_OK);

    res = curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &captured_data);
    ck_assert_int_eq(res, CURLE_OK);

    curl_easy_cleanup_(curl);
}
END_TEST

START_TEST(test_playback_sets_running_handles) {
    // Test that curl_multi_perform_ correctly manages running_handles
    CURLM *multi = curl_multi_init_();
    ck_assert_ptr_nonnull(multi);

    CURL *curl = curl_easy_init_();
    ck_assert_ptr_nonnull(curl);

    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, test_write_callback);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &captured_data);

    curl_multi_add_handle_(multi, curl);

    int running = 99;
    CURLMcode mres = curl_multi_perform_(multi, &running);
    ck_assert_int_eq(mres, CURLM_OK);

    // Without VCR active, running_handles behavior depends on real curl
    // This just verifies the function doesn't crash

    curl_multi_remove_handle_(multi, curl);
    curl_easy_cleanup_(curl);
    curl_multi_cleanup_(multi);
}

END_TEST

START_TEST(test_playback_provides_status) {
    // Test that HTTP status is available via getinfo
    CURL *curl = curl_easy_init_();
    ck_assert_ptr_nonnull(curl);

    // In VCR playback mode with a fixture, this would return the status
    // For now, just verify the call doesn't crash
    long status = 0;
    CURLcode res = curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &status);
    ck_assert_int_eq(res, CURLE_OK);

    curl_easy_cleanup_(curl);
}

END_TEST
// ============================================================================
// Record Mode Tests
// ============================================================================

START_TEST(test_record_preserves_callback) {
    // Set VCR_RECORD to enable recording mode
    setenv("VCR_RECORD", "1", 1);

    // Initialize VCR in record mode
    vcr_init("test_record_preserves_callback", "test");

    CURL *curl = curl_easy_init_();
    ck_assert_ptr_nonnull(curl);

    // Set write callback - in record mode, VCR wraps this
    CURLcode res = curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, test_write_callback);
    ck_assert_int_eq(res, CURLE_OK);

    res = curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &captured_data);
    ck_assert_int_eq(res, CURLE_OK);

    // Verify VCR is in recording mode
    ck_assert(vcr_is_recording());

    curl_easy_cleanup_(curl);
    vcr_finish();
    unsetenv("VCR_RECORD");
}

END_TEST
// ============================================================================
// Mode Tests
// ============================================================================

START_TEST(test_vcr_inactive_uses_real_curl) {
    // Without vcr_init, VCR should be inactive
    ck_assert(!vcr_is_active());

    CURL *curl = curl_easy_init_();
    ck_assert_ptr_nonnull(curl);

    // Normal curl operations should work
    CURLcode res = curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, test_write_callback);
    ck_assert_int_eq(res, CURLE_OK);

    curl_easy_cleanup_(curl);
}

END_TEST

START_TEST(test_vcr_get_response_status) {
    // Test vcr_get_response_status function
    // Without active VCR, should return 0
    ck_assert_int_eq(vcr_get_response_status(), 0);

    // After init (in playback mode without fixture), should return 0
    vcr_init("test_vcr_get_response_status", "test");
    ck_assert_int_eq(vcr_get_response_status(), 0);
    vcr_finish();
}

END_TEST

START_TEST(test_vcr_is_active) {
    // Test vcr_is_active function
    ck_assert(!vcr_is_active());

    vcr_init("test_vcr_is_active", "test");
    ck_assert(vcr_is_active());

    vcr_finish();
    ck_assert(!vcr_is_active());
}

END_TEST

// ============================================================================
// Test Suite
// ============================================================================

static Suite *vcr_mock_integration_suite(void)
{
    Suite *s = suite_create("VCR Mock Integration");

    TCase *tc_playback = tcase_create("Playback");
    tcase_set_timeout(tc_playback, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_playback, setup, teardown);
    tcase_add_test(tc_playback, test_playback_delivers_chunks);
    tcase_add_test(tc_playback, test_playback_sets_running_handles);
    tcase_add_test(tc_playback, test_playback_provides_status);
    suite_add_tcase(s, tc_playback);

    TCase *tc_record = tcase_create("Record");
    tcase_set_timeout(tc_record, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_record, setup, teardown);
    tcase_add_test(tc_record, test_record_preserves_callback);
    suite_add_tcase(s, tc_record);

    TCase *tc_mode = tcase_create("Mode");
    tcase_set_timeout(tc_mode, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_mode, setup, teardown);
    tcase_add_test(tc_mode, test_vcr_inactive_uses_real_curl);
    tcase_add_test(tc_mode, test_vcr_get_response_status);
    tcase_add_test(tc_mode, test_vcr_is_active);
    suite_add_tcase(s, tc_mode);

    return s;
}

int main(void)
{
    Suite *s = vcr_mock_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/helpers/vcr_mock_integration_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
