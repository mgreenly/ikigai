/**
 * @file anthropic_response_test.c
 * @brief Unit tests for Anthropic response parsing
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/response.h"
#include "providers/provider.h"

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

START_TEST(test_map_finish_reason_null) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_finish_reason_end_turn) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("end_turn");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_finish_reason_max_tokens) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("max_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_finish_reason_tool_use) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("tool_use");
    ck_assert_int_eq(reason, IK_FINISH_TOOL_USE);
}

END_TEST

START_TEST(test_map_finish_reason_stop_sequence) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("stop_sequence");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_finish_reason_refusal) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("refusal");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_unknown) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_response_basic) {
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"model\": \"claude-3-5-sonnet-20241022\","
        "  \"stop_reason\": \"end_turn\","
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20},"
        "  \"content\": [{\"type\": \"text\", \"text\": \"Hello\"}]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "claude-3-5-sonnet-20241022");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_parse_response_invalid_json) {
    const char *json = "not valid json";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST

START_TEST(test_parse_response_not_object) {
    const char *json = "[1, 2, 3]";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST

START_TEST(test_parse_response_error_type) {
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"invalid_request_error\","
        "    \"message\": \"Invalid model specified\""
        "  }"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST

START_TEST(test_parse_response_no_model) {
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"stop_reason\": \"end_turn\","
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20},"
        "  \"content\": []"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
}

END_TEST

START_TEST(test_parse_response_no_content) {
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"model\": \"claude-3-5-sonnet-20241022\","
        "  \"stop_reason\": \"end_turn\","
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20}"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert(resp->content_count == 0);
}

END_TEST
/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_response_suite(void)
{
    Suite *s = suite_create("Anthropic Response");

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish, 30);
    tcase_add_unchecked_fixture(tc_finish, setup, teardown);
    tcase_add_test(tc_finish, test_map_finish_reason_null);
    tcase_add_test(tc_finish, test_map_finish_reason_end_turn);
    tcase_add_test(tc_finish, test_map_finish_reason_max_tokens);
    tcase_add_test(tc_finish, test_map_finish_reason_tool_use);
    tcase_add_test(tc_finish, test_map_finish_reason_stop_sequence);
    tcase_add_test(tc_finish, test_map_finish_reason_refusal);
    tcase_add_test(tc_finish, test_map_finish_reason_unknown);
    suite_add_tcase(s, tc_finish);

    TCase *tc_parse = tcase_create("Response Parsing");
    tcase_set_timeout(tc_parse, 30);
    tcase_add_unchecked_fixture(tc_parse, setup, teardown);
    tcase_add_test(tc_parse, test_parse_response_basic);
    tcase_add_test(tc_parse, test_parse_response_invalid_json);
    tcase_add_test(tc_parse, test_parse_response_not_object);
    tcase_add_test(tc_parse, test_parse_response_error_type);
    tcase_add_test(tc_parse, test_parse_response_no_model);
    tcase_add_test(tc_parse, test_parse_response_no_content);
    suite_add_tcase(s, tc_parse);

    return s;
}

int main(void)
{
    Suite *s = anthropic_response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
