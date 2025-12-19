#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdarg.h>
#include <curl/curl.h>
#include "openai/client.h"
#include "config.h"

/* Forward declarations - we'll override the weak symbols from wrapper.c */
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_perform_(CURL *curl);
const char *curl_easy_strerror_(CURLcode code);
struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
void curl_slist_free_all_(struct curl_slist *list);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);

/*
 * HTTP SSE finish_reason extraction tests
 *
 * Tests finish_reason field extraction from SSE streaming responses.
 * Uses weak symbols to override curl calls and inject test scenarios.
 */

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;

/* Mock state */
static bool mock_init_should_fail = false;
static bool mock_perform_should_fail = false;
static CURLcode mock_perform_error_code = CURLE_OK;

/* Callback capture state */
typedef size_t (*curl_write_callback)(char *data, size_t size, size_t nmemb, void *userdata);
static curl_write_callback g_write_callback = NULL;
static void *g_write_data = NULL;

/* Mock response data */
static const char *mock_response_data = NULL;
static size_t mock_response_len = 0;

/* Callback invocation counter */
static int g_callback_invocations = 0;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Reset mock state */
    mock_init_should_fail = false;
    mock_perform_should_fail = false;
    mock_perform_error_code = CURLE_OK;

    /* Reset callback capture state */
    g_write_callback = NULL;
    g_write_data = NULL;

    /* Reset mock response data */
    mock_response_data = NULL;
    mock_response_len = 0;

    /* Reset callback invocations */
    g_callback_invocations = 0;
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

/* Mock libcurl functions - override weak symbols from wrapper.c */

CURL *curl_easy_init_(void)
{
    if (mock_init_should_fail) {
        return NULL;
    }
    /* Return a REAL curl handle so curl_easy_setopt works */
    return curl_easy_init();
}

void curl_easy_cleanup_(CURL *curl)
{
    /* Clean up the real curl handle */
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl; /* Unused in mock */

    if (mock_perform_should_fail) {
        return mock_perform_error_code;
    }

    /* Invoke captured write callback with mock response data */
    if (g_write_callback && mock_response_data) {
        /* Disable cast-qual warning - callback expects char* but we have const data */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        size_t result = g_write_callback(
            (char *)mock_response_data,
            1,
            mock_response_len,
            g_write_data
            );
#pragma GCC diagnostic pop
        /* Check if callback signaled error */
        if (result != mock_response_len) {
            return CURLE_WRITE_ERROR;
        }
    }

    return CURLE_OK;
}

const char *curl_easy_strerror_(CURLcode code)
{
    /* Simple mock - just return a generic message */
    switch (code) {
        case CURLE_OK:
            return "No error";
        case CURLE_COULDNT_CONNECT:
            return "Couldn't connect";
        case CURLE_WRITE_ERROR:
            return "Write error";
        default:
            return "Unknown error";
    }
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    /* Use real curl_slist_append */
    return curl_slist_append(list, string);
}

void curl_slist_free_all_(struct curl_slist *list)
{
    /* Use real curl_slist_free_all */
    if (list != NULL) {
        curl_slist_free_all(list);
    }
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
    /* Capture callbacks we need for testing */
    /* Disable cast-qual warning - we're intentionally capturing pointers */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_write_callback = (curl_write_callback)val;
    } else if (opt == CURLOPT_WRITEDATA) {
        g_write_data = (void *)val;
    }
#pragma GCC diagnostic pop

    /* Still call real curl_easy_setopt */
    return curl_easy_setopt(curl, opt, val);
}

/*
 * Test: SSE streaming response with finish_reason
 *
 * Note: finish_reason is not stored in canonical format (ik_msg_t).
 * This test verifies that responses with finish_reason are properly converted.
 */
START_TEST(test_http_callback_with_finish_reason) {
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create(conv, "user", "Hello");

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with finish_reason */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n"
        "data: [DONE]\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(!result.is_err);

    /* Verify canonical message was created with content */
    ik_msg_t *response_msg = result.ok;
    ck_assert_ptr_nonnull(response_msg);
    ck_assert_ptr_nonnull(response_msg->kind);
    ck_assert_str_eq(response_msg->kind, "assistant");
    ck_assert_ptr_nonnull(response_msg->content);
    ck_assert_str_eq(response_msg->content, "Hello World");
}

END_TEST
/*
 * Test: SSE event without finish_reason (should not crash)
 */
START_TEST(test_http_callback_without_finish_reason)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create(conv, "user", "Hello");

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response without finish_reason */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: [DONE]\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(!result.is_err);

    /* Verify canonical message was created with content */
    ik_msg_t *response_msg = result.ok;
    ck_assert_ptr_nonnull(response_msg);
    ck_assert_ptr_nonnull(response_msg->kind);
    ck_assert_str_eq(response_msg->kind, "assistant");
    ck_assert_ptr_nonnull(response_msg->content);
    ck_assert_str_eq(response_msg->content, "Hello");
}

END_TEST
/*
 * Test: Malformed SSE events (should handle gracefully)
 */
START_TEST(test_http_callback_malformed_finish_reason)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create(conv, "user", "Hello");

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock response with malformed events */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n\n"
        "data: {\"invalid\":\"json\"}\n\n"
        "data: []\n\n"
        "data: {\"choices\":[]}\n\n"
        "data: {\"choices\":[\"not_an_object\"]}\n\n"
        "data: [DONE]\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create - should handle malformed events gracefully */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(!result.is_err);

    /* Verify canonical message was created (should have at least the valid content) */
    ik_msg_t *response_msg = result.ok;
    ck_assert_ptr_nonnull(response_msg);
    ck_assert_ptr_nonnull(response_msg->kind);
    ck_assert_str_eq(response_msg->kind, "assistant");
    ck_assert_ptr_nonnull(response_msg->content);
    ck_assert_str_eq(response_msg->content, "Hi");
}

END_TEST
/*
 * Test: Edge cases for finish_reason extraction
 */
START_TEST(test_http_callback_finish_reason_edge_cases)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create(conv, "user", "Hello");

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock response with edge cases:
     * - Event without "data: " prefix
     * - Event with invalid JSON
     * - Event with root not being object
     */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n\n"
        "invalid event\n\n"
        "data: not valid json\n\n"
        "data: \"string_root\"\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n"
        "data: [DONE]\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(!result.is_err);

    /* Verify canonical message was created */
    ik_msg_t *response_msg = result.ok;
    ck_assert_ptr_nonnull(response_msg);
    ck_assert_ptr_nonnull(response_msg->kind);
    ck_assert_str_eq(response_msg->kind, "assistant");
    ck_assert_ptr_nonnull(response_msg->content);
}

END_TEST

/*
 * Test suite
 */
static Suite *client_http_sse_finish_suite(void)
{
    Suite *s = suite_create("OpenAI Client HTTP SSE Finish Reason");

    TCase *tc_finish = tcase_create("Finish Reason");
    tcase_add_checked_fixture(tc_finish, setup, teardown);
    tcase_add_test(tc_finish, test_http_callback_with_finish_reason);
    tcase_add_test(tc_finish, test_http_callback_without_finish_reason);
    tcase_add_test(tc_finish, test_http_callback_malformed_finish_reason);
    tcase_add_test(tc_finish, test_http_callback_finish_reason_edge_cases);
    suite_add_tcase(s, tc_finish);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = client_http_sse_finish_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
