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

END_TEST START_TEST(test_map_finish_reason_end_turn)
{
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("end_turn");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST START_TEST(test_map_finish_reason_max_tokens)
{
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("max_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST START_TEST(test_map_finish_reason_tool_use)
{
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("tool_use");
    ck_assert_int_eq(reason, IK_FINISH_TOOL_USE);
}

END_TEST START_TEST(test_map_finish_reason_stop_sequence)
{
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("stop_sequence");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST START_TEST(test_map_finish_reason_refusal)
{
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("refusal");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST START_TEST(test_map_finish_reason_unknown)
{
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_response_basic)
{
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

END_TEST START_TEST(test_parse_response_invalid_json)
{
    const char *json = "not valid json";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_parse_response_not_object)
{
    const char *json = "[1, 2, 3]";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_parse_response_error_type)
{
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

END_TEST START_TEST(test_parse_response_no_model)
{
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

END_TEST START_TEST(test_parse_response_no_content)
{
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
 * Error Parsing Tests
 * ================================================================ */

START_TEST(test_parse_error_400)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"invalid_request_error\","
        "    \"message\": \"Invalid model\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 400, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_401)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 401, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_403)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 403, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST START_TEST(test_parse_error_404)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 404, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}

END_TEST START_TEST(test_parse_error_429)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 429, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST START_TEST(test_parse_error_500)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST START_TEST(test_parse_error_502)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 502, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST START_TEST(test_parse_error_503)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 503, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST START_TEST(test_parse_error_529)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 529, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST START_TEST(test_parse_error_unknown_status)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 418, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
}

END_TEST START_TEST(test_parse_error_with_message_only)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"message\": \"Something went wrong\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(message);
    ck_assert(strstr(message, "Something went wrong") != NULL);
}

END_TEST START_TEST(test_parse_error_with_type_only)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"server_error\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_empty_error_obj)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {}"
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(message);
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

    TCase *tc_error = tcase_create("Error Parsing");
    tcase_set_timeout(tc_error, 30);
    tcase_add_unchecked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_parse_error_400);
    tcase_add_test(tc_error, test_parse_error_401);
    tcase_add_test(tc_error, test_parse_error_403);
    tcase_add_test(tc_error, test_parse_error_404);
    tcase_add_test(tc_error, test_parse_error_429);
    tcase_add_test(tc_error, test_parse_error_500);
    tcase_add_test(tc_error, test_parse_error_502);
    tcase_add_test(tc_error, test_parse_error_503);
    tcase_add_test(tc_error, test_parse_error_529);
    tcase_add_test(tc_error, test_parse_error_unknown_status);
    tcase_add_test(tc_error, test_parse_error_with_message_only);
    tcase_add_test(tc_error, test_parse_error_with_type_only);
    tcase_add_test(tc_error, test_parse_error_empty_error_obj);
    suite_add_tcase(s, tc_error);

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
