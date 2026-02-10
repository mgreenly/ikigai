#include "tests/test_constants.h"
/**
 * @file response_errors_test.c
 * @brief Unit tests for Google error parsing and utilities
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Finish Reason Mapping Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_stop) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("STOP");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_finish_reason_max_tokens) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("MAX_TOKENS");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_finish_reason_safety) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("SAFETY");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_blocklist) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("BLOCKLIST");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_prohibited) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("PROHIBITED_CONTENT");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_recitation) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("RECITATION");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_malformed_function_call) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("MALFORMED_FUNCTION_CALL");
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}

END_TEST

START_TEST(test_map_finish_reason_unexpected_tool_call) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("UNEXPECTED_TOOL_CALL");
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}

END_TEST

START_TEST(test_map_finish_reason_null) {
    ik_finish_reason_t reason = ik_google_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_finish_reason_unknown) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("UNKNOWN");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_finish_reason_image_safety) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("IMAGE_SAFETY");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_image_prohibited_content) {
    ik_finish_reason_t reason = ik_google_map_finish_reason("IMAGE_PROHIBITED_CONTENT");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST
/* ================================================================
 * Tool ID Generation Tests
 * ================================================================ */

START_TEST(test_generate_tool_id_length) {
    char *id = ik_google_generate_tool_id(test_ctx);
    ck_assert_ptr_nonnull(id);
    ck_assert_uint_eq((unsigned int)strlen(id), 22);
}

END_TEST

START_TEST(test_generate_tool_id_charset) {
    char *id = ik_google_generate_tool_id(test_ctx);
    ck_assert_ptr_nonnull(id);

    // Verify all characters are in base64url alphabet
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 22; i++) {
        ck_assert_ptr_nonnull(strchr(alphabet, id[i]));
    }
}

END_TEST

START_TEST(test_generate_tool_id_unique) {
    char *id1 = ik_google_generate_tool_id(test_ctx);
    char *id2 = ik_google_generate_tool_id(test_ctx);

    // IDs should be different (with very high probability)
    ck_assert_str_ne(id1, id2);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_errors_suite(void)
{
    Suite *s = suite_create("Google Response Errors & Utilities");

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish, IK_TEST_TIMEOUT);
    tcase_add_test(tc_finish, test_map_finish_reason_stop);
    tcase_add_test(tc_finish, test_map_finish_reason_max_tokens);
    tcase_add_test(tc_finish, test_map_finish_reason_safety);
    tcase_add_test(tc_finish, test_map_finish_reason_blocklist);
    tcase_add_test(tc_finish, test_map_finish_reason_prohibited);
    tcase_add_test(tc_finish, test_map_finish_reason_recitation);
    tcase_add_test(tc_finish, test_map_finish_reason_malformed_function_call);
    tcase_add_test(tc_finish, test_map_finish_reason_unexpected_tool_call);
    tcase_add_test(tc_finish, test_map_finish_reason_null);
    tcase_add_test(tc_finish, test_map_finish_reason_unknown);
    tcase_add_test(tc_finish, test_map_finish_reason_image_safety);
    tcase_add_test(tc_finish, test_map_finish_reason_image_prohibited_content);
    suite_add_tcase(s, tc_finish);

    TCase *tc_id = tcase_create("Tool ID Generation");
    tcase_set_timeout(tc_id, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_id, setup, teardown);
    tcase_add_test(tc_id, test_generate_tool_id_length);
    tcase_add_test(tc_id, test_generate_tool_id_charset);
    tcase_add_test(tc_id, test_generate_tool_id_unique);
    suite_add_tcase(s, tc_id);

    return s;
}

int main(void)
{
    Suite *s = google_response_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/response_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
