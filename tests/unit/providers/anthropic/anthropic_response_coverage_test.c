/**
 * @file anthropic_response_coverage_test.c
 * @brief Additional coverage tests for Anthropic response parsing edge cases
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
 * Additional Response Parsing Coverage Tests
 * ================================================================ */

START_TEST(test_parse_response_type_null)
{
    const char *json =
        "{"
        "  \"type\": null,"
        "  \"model\": \"claude-3-5-sonnet-20241022\","
        "  \"stop_reason\": \"end_turn\","
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20},"
        "  \"content\": []"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
}

END_TEST START_TEST(test_parse_response_error_type_no_error_obj)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"model\": \"claude-3-5-sonnet-20241022\""
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_parse_response_error_type_null_message)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"message\": null"
        "  }"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_parse_response_error_type_null_message_str)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"invalid_request_error\","
        "    \"message\": null"
        "  }"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_parse_response_model_null)
{
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"model\": null,"
        "  \"stop_reason\": \"end_turn\","
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20},"
        "  \"content\": []"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
}

END_TEST START_TEST(test_parse_response_stop_reason_null)
{
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"model\": \"claude-3-5-sonnet-20241022\","
        "  \"stop_reason\": null,"
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20},"
        "  \"content\": []"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST START_TEST(test_parse_response_content_not_array)
{
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"model\": \"claude-3-5-sonnet-20241022\","
        "  \"stop_reason\": \"end_turn\","
        "  \"usage\": {\"input_tokens\": 10, \"output_tokens\": 20},"
        "  \"content\": \"not an array\""
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_anthropic_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert(resp->content_count == 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

/* ================================================================
 * Additional Error Parsing Coverage Tests
 * ================================================================ */

START_TEST(test_parse_error_invalid_json)
{
    const char *json = "not valid json";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_json_not_object)
{
    const char *json = "[1, 2, 3]";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_no_error_field)
{
    const char *json =
        "{"
        "  \"type\": \"message\","
        "  \"model\": \"claude-3-5-sonnet-20241022\""
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_type_null_no_message)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": null"
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(message);
}

END_TEST START_TEST(test_parse_error_message_null_no_type)
{
    const char *json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"message\": null"
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t r = ik_anthropic_parse_error(test_ctx, 500, json, strlen(json), &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(message);
}

END_TEST

/* ================================================================
 * Stub Function Coverage Tests
 * ================================================================ */

static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;
    (void)completion;
    return OK(NULL);
}

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;
    (void)event;
    return OK(NULL);
}

START_TEST(test_start_request_stub)
{
    ik_request_t req = {0};
    int32_t dummy_ctx = 42;

    res_t r = ik_anthropic_start_request(&dummy_ctx, &req, dummy_completion_cb, NULL);

    ck_assert(!is_err(&r));
}

END_TEST START_TEST(test_start_stream_stub)
{
    ik_request_t req = {0};
    int32_t dummy_ctx = 42;

    res_t r = ik_anthropic_start_stream(&dummy_ctx, &req, dummy_stream_cb, NULL, dummy_completion_cb, NULL);

    ck_assert(!is_err(&r));
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_response_coverage_suite(void)
{
    Suite *s = suite_create("Anthropic Response Coverage");

    TCase *tc_parse = tcase_create("Response Parsing Coverage");
    tcase_set_timeout(tc_parse, 30);
    tcase_add_unchecked_fixture(tc_parse, setup, teardown);
    tcase_add_test(tc_parse, test_parse_response_type_null);
    tcase_add_test(tc_parse, test_parse_response_error_type_no_error_obj);
    tcase_add_test(tc_parse, test_parse_response_error_type_null_message);
    tcase_add_test(tc_parse, test_parse_response_error_type_null_message_str);
    tcase_add_test(tc_parse, test_parse_response_model_null);
    tcase_add_test(tc_parse, test_parse_response_stop_reason_null);
    tcase_add_test(tc_parse, test_parse_response_content_not_array);
    suite_add_tcase(s, tc_parse);

    TCase *tc_error = tcase_create("Error Parsing Coverage");
    tcase_set_timeout(tc_error, 30);
    tcase_add_unchecked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_parse_error_invalid_json);
    tcase_add_test(tc_error, test_parse_error_json_not_object);
    tcase_add_test(tc_error, test_parse_error_no_error_field);
    tcase_add_test(tc_error, test_parse_error_type_null_no_message);
    tcase_add_test(tc_error, test_parse_error_message_null_no_type);
    suite_add_tcase(s, tc_error);

    TCase *tc_stubs = tcase_create("Stub Functions");
    tcase_set_timeout(tc_stubs, 30);
    tcase_add_unchecked_fixture(tc_stubs, setup, teardown);
    tcase_add_test(tc_stubs, test_start_request_stub);
    tcase_add_test(tc_stubs, test_start_stream_stub);
    suite_add_tcase(s, tc_stubs);

    return s;
}

int main(void)
{
    Suite *s = anthropic_response_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
