/**
 * @file response_test.c
 * @brief Unit tests for Google response parsing
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/response.h"
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
 * Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_simple_text_response)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-2.5-flash\","
        "\"candidates\":[{"
            "\"content\":{\"parts\":[{\"text\":\"Hello world\"}]},"
            "\"finishReason\":\"STOP\""
        "}],"
        "\"usageMetadata\":{"
            "\"promptTokenCount\":10,"
            "\"candidatesTokenCount\":5,"
            "\"thoughtsTokenCount\":0,"
            "\"totalTokenCount\":15"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "gemini-2.5-flash");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Hello world");
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 5);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}
END_TEST

START_TEST(test_parse_thinking_response)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-3\","
        "\"candidates\":[{"
            "\"content\":{\"parts\":["
                "{\"text\":\"Let me think...\",\"thought\":true},"
                "{\"text\":\"The answer is 42\"}"
            "]},"
            "\"finishReason\":\"STOP\""
        "}],"
        "\"usageMetadata\":{"
            "\"promptTokenCount\":10,"
            "\"candidatesTokenCount\":20,"
            "\"thoughtsTokenCount\":8,"
            "\"totalTokenCount\":30"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 2);

    // First block is thinking
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(resp->content_blocks[0].data.thinking.text, "Let me think...");

    // Second block is text
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[1].data.text.text, "The answer is 42");

    // Verify token calculation: output = candidates - thoughts = 20 - 8 = 12
    ck_assert_int_eq(resp->usage.thinking_tokens, 8);
    ck_assert_int_eq(resp->usage.output_tokens, 12);
}
END_TEST

START_TEST(test_parse_function_call_response)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-2.5-pro\","
        "\"candidates\":[{"
            "\"content\":{\"parts\":[{"
                "\"functionCall\":{"
                    "\"name\":\"get_weather\","
                    "\"args\":{\"city\":\"London\",\"units\":\"metric\"}"
                "}"
            "}]},"
            "\"finishReason\":\"STOP\""
        "}],"
        "\"usageMetadata\":{"
            "\"promptTokenCount\":15,"
            "\"candidatesTokenCount\":10,"
            "\"totalTokenCount\":25"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);

    // Tool call has generated ID (22 chars)
    ck_assert_ptr_nonnull(resp->content_blocks[0].data.tool_call.id);
    ck_assert_uint_eq((unsigned int)strlen(resp->content_blocks[0].data.tool_call.id), 22);

    // Tool name and args
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_weather");
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "London"));
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "metric"));
}
END_TEST

START_TEST(test_parse_error_response)
{
    const char *json = "{"
        "\"error\":{"
            "\"code\":403,"
            "\"message\":\"API key invalid\","
            "\"status\":\"PERMISSION_DENIED\""
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "API key invalid"));
}
END_TEST

START_TEST(test_parse_blocked_prompt)
{
    const char *json = "{"
        "\"promptFeedback\":{"
            "\"blockReason\":\"SAFETY\""
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "SAFETY"));
}
END_TEST

START_TEST(test_parse_empty_candidates)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-2.5-flash\","
        "\"candidates\":[],"
        "\"usageMetadata\":{\"totalTokenCount\":0}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_no_candidates)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-2.5-flash\","
        "\"usageMetadata\":{\"totalTokenCount\":5}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_invalid_json)
{
    const char *json = "not valid json";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}
END_TEST

START_TEST(test_parse_thought_signature)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-3\","
        "\"candidates\":[{"
            "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
            "\"finishReason\":\"STOP\""
        "}],"
        "\"thoughtSignature\":\"enc_sig_abc123\","
        "\"usageMetadata\":{\"totalTokenCount\":10}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp->provider_data);
    ck_assert_ptr_nonnull(strstr(resp->provider_data, "thought_signature"));
    ck_assert_ptr_nonnull(strstr(resp->provider_data, "enc_sig_abc123"));
}
END_TEST

START_TEST(test_parse_no_thought_signature)
{
    const char *json = "{"
        "\"modelVersion\":\"gemini-2.5-flash\","
        "\"candidates\":[{"
            "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
            "\"finishReason\":\"STOP\""
        "}],"
        "\"usageMetadata\":{\"totalTokenCount\":10}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(resp->provider_data);
}
END_TEST

/* ================================================================
 * Finish Reason Mapping Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_stop)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("STOP");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_map_finish_reason_max_tokens)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("MAX_TOKENS");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_map_finish_reason_safety)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("SAFETY");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_blocklist)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("BLOCKLIST");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_prohibited)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("PROHIBITED_CONTENT");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_recitation)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("RECITATION");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_malformed_function_call)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("MALFORMED_FUNCTION_CALL");
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}
END_TEST

START_TEST(test_map_finish_reason_unexpected_tool_call)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("UNEXPECTED_TOOL_CALL");
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}
END_TEST

START_TEST(test_map_finish_reason_null)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_map_finish_reason_unknown)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("UNKNOWN");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * Error Parsing Tests
 * ================================================================ */

START_TEST(test_parse_error_400)
{
    const char *json = "{\"error\":{\"message\":\"Invalid argument\"}}";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 400, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_ptr_nonnull(strstr(message, "Invalid argument"));
}
END_TEST

START_TEST(test_parse_error_401)
{
    const char *json = "{\"error\":{\"message\":\"Unauthorized\"}}";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 401, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_parse_error_404)
{
    const char *json = "{\"error\":{\"message\":\"Model not found\"}}";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 404, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}
END_TEST

START_TEST(test_parse_error_429)
{
    const char *json = "{\"error\":{\"message\":\"Rate limit exceeded\"}}";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 429, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

START_TEST(test_parse_error_500)
{
    const char *json = "{\"error\":{\"message\":\"Internal error\"}}";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 500, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

START_TEST(test_parse_error_504)
{
    const char *json = "{\"error\":{\"message\":\"Gateway timeout\"}}";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 504, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_TIMEOUT);
}
END_TEST

START_TEST(test_parse_error_no_json)
{
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 500, NULL, 0,
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(strstr(message, "HTTP 500"));
}
END_TEST

START_TEST(test_parse_error_invalid_json)
{
    const char *json = "not json";
    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 500, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(strstr(message, "HTTP 500"));
}
END_TEST

/* ================================================================
 * Tool ID Generation Tests
 * ================================================================ */

START_TEST(test_generate_tool_id_length)
{
    char *id = ik_google_generate_tool_id(test_ctx);
    ck_assert_ptr_nonnull(id);
    ck_assert_uint_eq((unsigned int)strlen(id), 22);
}
END_TEST

START_TEST(test_generate_tool_id_charset)
{
    char *id = ik_google_generate_tool_id(test_ctx);
    ck_assert_ptr_nonnull(id);

    // Verify all characters are in base64url alphabet
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 22; i++) {
        ck_assert_ptr_nonnull(strchr(alphabet, id[i]));
    }
}
END_TEST

START_TEST(test_generate_tool_id_unique)
{
    char *id1 = ik_google_generate_tool_id(test_ctx);
    char *id2 = ik_google_generate_tool_id(test_ctx);

    // IDs should be different (with very high probability)
    ck_assert_str_ne(id1, id2);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_suite(void)
{
    Suite *s = suite_create("Google Response Parsing");

    TCase *tc_parse = tcase_create("Response Parsing");
    tcase_add_unchecked_fixture(tc_parse, setup, teardown);
    tcase_add_test(tc_parse, test_parse_simple_text_response);
    tcase_add_test(tc_parse, test_parse_thinking_response);
    tcase_add_test(tc_parse, test_parse_function_call_response);
    tcase_add_test(tc_parse, test_parse_error_response);
    tcase_add_test(tc_parse, test_parse_blocked_prompt);
    tcase_add_test(tc_parse, test_parse_empty_candidates);
    tcase_add_test(tc_parse, test_parse_no_candidates);
    tcase_add_test(tc_parse, test_parse_invalid_json);
    tcase_add_test(tc_parse, test_parse_thought_signature);
    tcase_add_test(tc_parse, test_parse_no_thought_signature);
    suite_add_tcase(s, tc_parse);

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
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
    suite_add_tcase(s, tc_finish);

    TCase *tc_error = tcase_create("Error Parsing");
    tcase_add_unchecked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_parse_error_400);
    tcase_add_test(tc_error, test_parse_error_401);
    tcase_add_test(tc_error, test_parse_error_404);
    tcase_add_test(tc_error, test_parse_error_429);
    tcase_add_test(tc_error, test_parse_error_500);
    tcase_add_test(tc_error, test_parse_error_504);
    tcase_add_test(tc_error, test_parse_error_no_json);
    tcase_add_test(tc_error, test_parse_error_invalid_json);
    suite_add_tcase(s, tc_error);

    TCase *tc_id = tcase_create("Tool ID Generation");
    tcase_add_unchecked_fixture(tc_id, setup, teardown);
    tcase_add_test(tc_id, test_generate_tool_id_length);
    tcase_add_test(tc_id, test_generate_tool_id_charset);
    tcase_add_test(tc_id, test_generate_tool_id_unique);
    suite_add_tcase(s, tc_id);

    return s;
}

int main(void)
{
    Suite *s = google_response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
