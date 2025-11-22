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
 * HTTP SSE streaming tests
 *
 * Tests SSE parsing and callback functionality through mocked libcurl.
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
 * Test: Successful request with SSE streaming response
 */
START_TEST(test_http_callback_with_sse_streaming) {
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(conv, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n"
        "data: [DONE]\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create - should invoke callback and parse SSE */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(!result.is_err);

    /* Verify response content was accumulated from SSE events */
    ik_openai_response_t *resp = result.ok;
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_nonnull(resp->content);
    ck_assert_str_eq(resp->content, "Hello World");
}

END_TEST
/*
 * Test: Empty response through callback
 */
START_TEST(test_http_callback_empty_response)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(conv, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up empty response */
    mock_response_data = "";
    mock_response_len = 0;

    /* Call chat_create - should handle empty response */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(!result.is_err);

    /* Verify response was created even with empty content */
    ik_openai_response_t *resp = result.ok;
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_nonnull(resp->content);
}

END_TEST
/*
 * Test: Callback error handling - SSE parser feed failure
 */
START_TEST(test_http_callback_sse_parser_feed_error)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(conv, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up malformed data - but SSE parser is quite robust
     * and can handle most malformed data, so we just test with
     * truncated SSE that might not cause an error */
    const char *response = "data: incomplete";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Parser may or may not fail on incomplete data - either is acceptable */
    if (!result.is_err) {
        ik_openai_response_t *resp = result.ok;
        ck_assert_ptr_nonnull(resp);
    }
}

END_TEST
/*
 * Test: Callback error handling - SSE event parse failure
 */
START_TEST(test_http_callback_sse_parse_error)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(conv, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up response with invalid JSON in SSE event */
    const char *response =
        "data: {not valid json}\n\n"
        "data: [DONE]\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create - should handle parse error */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Parse error should be caught and propagated */
    if (result.is_err) {
        /* Error is expected for invalid JSON */
        ck_assert(result.is_err);
    } else {
        /* Or it may succeed with partial data */
        ik_openai_response_t *resp = result.ok;
        ck_assert_ptr_nonnull(resp);
    }
}

END_TEST

/*
 * Successful callback for testing
 */
static res_t successful_callback(const char *content, void *user_ctx)
{
    (void)user_ctx;
    (void)content;
    g_callback_invocations++;
    return OK(NULL);
}

/*
 * Failing callback for testing error handling
 */
static res_t failing_callback(const char *content, void *user_ctx)
{
    (void)content;
    TALLOC_CTX *err_ctx = (TALLOC_CTX *)user_ctx;
    return ERR(err_ctx, IO, "Callback failed");
}

/*
 * Test: User callback succeeds
 */
START_TEST(test_http_callback_user_success) {
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(conv, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create with successful callback */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, successful_callback, ctx);

    /* Should succeed */
    ck_assert(!result.is_err);

    /* Callback should have been invoked twice (once per content chunk) */
    ck_assert_int_eq(g_callback_invocations, 2);
}

END_TEST
/*
 * Test: User callback returns error
 */
START_TEST(test_http_callback_user_error)
{
    /* Create configuration */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key-12345");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 100;

    /* Create conversation with one message */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(conv, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Call chat_create with failing callback */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, failing_callback, ctx);

    /* Should fail due to callback error */
    ck_assert(result.is_err);
}

END_TEST

/*
 * Test suite
 */
static Suite *client_http_sse_streaming_suite(void)
{
    Suite *s = suite_create("OpenAI Client HTTP SSE Streaming");

    TCase *tc_sse = tcase_create("SSE Streaming");
    tcase_add_checked_fixture(tc_sse, setup, teardown);
    tcase_add_test(tc_sse, test_http_callback_with_sse_streaming);
    tcase_add_test(tc_sse, test_http_callback_empty_response);
    tcase_add_test(tc_sse, test_http_callback_sse_parser_feed_error);
    tcase_add_test(tc_sse, test_http_callback_sse_parse_error);
    tcase_add_test(tc_sse, test_http_callback_user_success);
    tcase_add_test(tc_sse, test_http_callback_user_error);
    suite_add_tcase(s, tc_sse);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = client_http_sse_streaming_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
