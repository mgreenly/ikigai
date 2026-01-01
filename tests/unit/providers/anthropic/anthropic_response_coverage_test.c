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

START_TEST(test_parse_response_null_fields) {
    const char *jsons[] = {
        "{\"type\":null,\"model\":\"claude\",\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20},\"content\":[]}",
        "{\"type\":\"message\",\"model\":null,\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20},\"content\":[]}",
        "{\"type\":\"message\",\"model\":\"claude\",\"stop_reason\":null,\"usage\":{\"input_tokens\":10,\"output_tokens\":20},\"content\":[]}",
        "{\"type\":\"message\",\"model\":\"claude\",\"stop_reason\":123,\"usage\":{\"input_tokens\":10,\"output_tokens\":20},\"content\":[]}"
    };
    for (size_t i = 0; i < 4; i++) {
        ik_response_t *resp = NULL;
        res_t r = ik_anthropic_parse_response(test_ctx, jsons[i], strlen(jsons[i]), &resp);
        ck_assert(!is_err(&r));
        ck_assert_ptr_nonnull(resp);
        talloc_free(resp);
    }
}
END_TEST

START_TEST(test_parse_response_errors) {
    const char *jsons[] = {
        "{\"type\":\"error\",\"model\":\"claude\"}",
        "{\"type\":\"error\",\"error\":{\"message\":null}}",
        "{\"type\":\"error\",\"error\":{\"type\":\"invalid_request_error\",\"message\":null}}",
        "{\"type\":\"error\",\"error\":{\"type\":\"server_error\",\"message\":\"Error occurred\"}}",
        "{\"type\":\"error\",\"error\":{\"type\":\"invalid_request_error\"}}"
    };
    for (size_t i = 0; i < 5; i++) {
        ik_response_t *resp = NULL;
        res_t r = ik_anthropic_parse_response(test_ctx, jsons[i], strlen(jsons[i]), &resp);
        ck_assert(is_err(&r));
    }
}
END_TEST

START_TEST(test_parse_response_type_mismatches) {
    // Fields with wrong types (covers yyjson_get_str returning NULL)
    struct { const char *json; bool should_error; } cases[] = {
        {"{\"type\":\"message\",\"model\":123,\"usage\":{},\"content\":[]}", false},
        {"{\"type\":\"message\",\"stop_reason\":true,\"usage\":{}}", false},
        {"{\"type\":\"error\",\"error\":{\"message\":789}}", true}
    };
    for (size_t i = 0; i < 3; i++) {
        ik_response_t *resp = NULL;
        res_t r = ik_anthropic_parse_response(test_ctx, cases[i].json, strlen(cases[i].json), &resp);
        ck_assert(cases[i].should_error == is_err(&r));
        if (!cases[i].should_error) talloc_free(resp);
    }
}

END_TEST

START_TEST(test_parse_response_edge_cases) {
    struct { const char *json; bool should_error; } cases[] = {
        {
            "{\"type\":\"message\",\"model\":\"claude\",\"stop_reason\":\"end_turn\",\"usage\":{},\"content\":\"not array\"}",
            false
        },
        {"{ invalid json }", true},
        {"[1, 2, 3]", true},
        {
            "{\"type\":\"message\",\"model\":\"claude\",\"stop_reason\":\"end_turn\",\"usage\":{},\"content\":[{\"invalid\":true}]}",
            true
        },
        {"{\"type\":\"message\",\"stop_reason\":\"end_turn\",\"usage\":{},\"content\":[]}", false},
        {"{\"type\":\"message\",\"model\":\"claude\",\"usage\":{},\"content\":[]}", false},
        {"{\"type\":\"message\",\"model\":\"claude\",\"stop_reason\":\"end_turn\",\"usage\":{}}", false},
        {"{\"model\":\"claude\",\"stop_reason\":\"end_turn\",\"usage\":{},\"content\":[]}", false}
    };
    for (size_t i = 0; i < 8; i++) {
        ik_response_t *resp = NULL;
        res_t r = ik_anthropic_parse_response(test_ctx, cases[i].json, strlen(cases[i].json), &resp);
        if (cases[i].should_error) {
            ck_assert(is_err(&r));
        } else {
            ck_assert(!is_err(&r));
            talloc_free(resp);
        }
    }
}
END_TEST

/* ================================================================
 * Finish Reason Mapping Coverage Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_all) {
    struct { const char *input; ik_finish_reason_t expected; } test_cases[] = {
        {"end_turn", IK_FINISH_STOP}, {"stop_sequence", IK_FINISH_STOP},
        {"max_tokens", IK_FINISH_LENGTH}, {"tool_use", IK_FINISH_TOOL_USE},
        {"refusal", IK_FINISH_CONTENT_FILTER}, {"unknown_reason", IK_FINISH_UNKNOWN},
        {NULL, IK_FINISH_UNKNOWN}, {"", IK_FINISH_UNKNOWN}
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        ik_finish_reason_t reason = ik_anthropic_map_finish_reason(test_cases[i].input);
        ck_assert_int_eq(reason, test_cases[i].expected);
    }
}

END_TEST

/* ================================================================
 * Additional Error Parsing Coverage Tests
 * ================================================================ */



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

START_TEST(test_start_request_stub) {
    ik_request_t req = {0};
    int32_t dummy_ctx = 42;

    res_t r = ik_anthropic_start_request(&dummy_ctx, &req, dummy_completion_cb, NULL);

    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_start_stream_stub) {
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
    tcase_add_test(tc_parse, test_parse_response_null_fields);
    tcase_add_test(tc_parse, test_parse_response_errors);
    tcase_add_test(tc_parse, test_parse_response_type_mismatches);
    tcase_add_test(tc_parse, test_parse_response_edge_cases);
    suite_add_tcase(s, tc_parse);

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish, 30);
    tcase_add_test(tc_finish, test_map_finish_reason_all);
    suite_add_tcase(s, tc_finish);

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
