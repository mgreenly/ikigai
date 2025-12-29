/**
 * @file request_helpers_thought_test.c
 * @brief Unit tests for Google thought signature helpers
 */

// Disable cast-qual for test literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/request_helpers.h"
#include "providers/google/thinking.h"
#include "providers/provider.h"
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
 * Thought Signature Extraction Tests
 * ================================================================ */

START_TEST(test_extract_thought_signature_null) {
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature(NULL, &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}
END_TEST START_TEST(test_extract_thought_signature_empty)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("", &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_extract_thought_signature_invalid_json)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("not json", &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_extract_thought_signature_not_object)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("[]", &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_extract_thought_signature_missing_field)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("{\"other\":\"value\"}", &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_extract_thought_signature_not_string)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("{\"thought_signature\":123}", &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_extract_thought_signature_empty_string)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("{\"thought_signature\":\"\"}", &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_extract_thought_signature_valid)
{
    yyjson_doc *doc = NULL;
    const char *sig = ik_google_extract_thought_signature("{\"thought_signature\":\"sig-123\"}", &doc);
    ck_assert_str_eq(sig, "sig-123");
    ck_assert(doc != NULL);
    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Find Latest Thought Signature Tests
 * ================================================================ */

START_TEST(test_find_latest_thought_signature_not_gemini_3)
{
    ik_message_t msg = {0};
    msg.role = IK_ROLE_ASSISTANT;
    msg.provider_metadata = (char *)"{\"thought_signature\":\"sig-123\"}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.5-pro";
    req.messages = &msg;
    req.message_count = 1;

    yyjson_doc *doc = NULL;
    const char *sig = ik_google_find_latest_thought_signature(&req, &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_find_latest_thought_signature_no_assistant)
{
    ik_message_t msg = {0};
    msg.role = IK_ROLE_USER;
    msg.provider_metadata = (char *)"{\"thought_signature\":\"sig-123\"}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-3-pro";
    req.messages = &msg;
    req.message_count = 1;

    yyjson_doc *doc = NULL;
    const char *sig = ik_google_find_latest_thought_signature(&req, &doc);
    ck_assert(sig == NULL);
    ck_assert(doc == NULL);
}

END_TEST START_TEST(test_find_latest_thought_signature_valid)
{
    ik_message_t messages[4];
    memset(messages, 0, sizeof(messages));

    messages[0].role = IK_ROLE_USER;
    messages[1].role = IK_ROLE_ASSISTANT;
    messages[1].provider_metadata = (char *)"{\"thought_signature\":\"sig-old\"}";
    messages[2].role = IK_ROLE_USER;
    messages[3].role = IK_ROLE_ASSISTANT;
    messages[3].provider_metadata = (char *)"{\"thought_signature\":\"sig-new\"}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-3-pro";
    req.messages = messages;
    req.message_count = 4;

    yyjson_doc *doc = NULL;
    const char *sig = ik_google_find_latest_thought_signature(&req, &doc);
    ck_assert_str_eq(sig, "sig-new");
    ck_assert(doc != NULL);
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_helpers_thought_suite(void)
{
    Suite *s = suite_create("Google Request Helpers - Thought Signatures");

    TCase *tc_extract = tcase_create("Thought Signature Extraction");
    tcase_set_timeout(tc_extract, 30);
    tcase_add_checked_fixture(tc_extract, setup, teardown);
    tcase_add_test(tc_extract, test_extract_thought_signature_null);
    tcase_add_test(tc_extract, test_extract_thought_signature_empty);
    tcase_add_test(tc_extract, test_extract_thought_signature_invalid_json);
    tcase_add_test(tc_extract, test_extract_thought_signature_not_object);
    tcase_add_test(tc_extract, test_extract_thought_signature_missing_field);
    tcase_add_test(tc_extract, test_extract_thought_signature_not_string);
    tcase_add_test(tc_extract, test_extract_thought_signature_empty_string);
    tcase_add_test(tc_extract, test_extract_thought_signature_valid);
    suite_add_tcase(s, tc_extract);

    TCase *tc_find = tcase_create("Find Latest Thought Signature");
    tcase_set_timeout(tc_find, 30);
    tcase_add_checked_fixture(tc_find, setup, teardown);
    tcase_add_test(tc_find, test_find_latest_thought_signature_not_gemini_3);
    tcase_add_test(tc_find, test_find_latest_thought_signature_no_assistant);
    tcase_add_test(tc_find, test_find_latest_thought_signature_valid);
    suite_add_tcase(s, tc_find);

    return s;
}

int32_t main(void)
{
    Suite *s = request_helpers_thought_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
