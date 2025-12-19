#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdarg.h>
#include <curl/curl.h>
#include "openai/client.h"
#include "config.h"

/*
 * HTTP handler error path tests
 *
 * Tests error handling paths in http_handler.c including:
 * - API key buffer overflow protection
 * - Streaming callback error propagation
 */

/* Forward declarations */
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_perform_(CURL *curl);
const char *curl_easy_strerror_(CURLcode code);
struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
void curl_slist_free_all_(struct curl_slist *list);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;

/* Global context for error callback */
static TALLOC_CTX *callback_ctx = NULL;

/* Mock state */
static bool mock_perform_should_return_ok = false;
static CURLcode mock_perform_error_code = CURLE_OK;

/* Callback capture state */
typedef size_t (*curl_write_callback)(char *data, size_t size, size_t nmemb, void *userdata);
static curl_write_callback g_write_callback = NULL;
static void *g_write_data = NULL;

/* Mock response data */
static const char *mock_response_data = NULL;
static size_t mock_response_len = 0;

static void setup(void)
{
    ctx = talloc_new(NULL);
    callback_ctx = ctx;

    /* Reset mock state */
    mock_perform_should_return_ok = false;
    mock_perform_error_code = CURLE_OK;

    /* Reset callback capture state */
    g_write_callback = NULL;
    g_write_data = NULL;

    /* Reset mock response data */
    mock_response_data = NULL;
    mock_response_len = 0;
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
    callback_ctx = NULL;
}

/* Mock libcurl functions */

CURL *curl_easy_init_(void)
{
    return curl_easy_init();
}

void curl_easy_cleanup_(CURL *curl)
{
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;

    if (mock_perform_should_return_ok) {
        /* Invoke write callback with mock response */
        if (g_write_callback && mock_response_data) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
            size_t result = g_write_callback(
                (char *)mock_response_data,
                1,
                mock_response_len,
                g_write_data
                );
#pragma GCC diagnostic pop
            if (result != mock_response_len) {
                /* Callback signaled error but we're mocking curl to return OK anyway
                 * This tests the defensive check in http_handler.c line 242 */
                return CURLE_OK;  /* Return OK despite callback error */
            }
        }
        return CURLE_OK;
    }

    /* Normal path: invoke callback and check result */
    if (g_write_callback && mock_response_data) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        size_t result = g_write_callback(
            (char *)mock_response_data,
            1,
            mock_response_len,
            g_write_data
            );
#pragma GCC diagnostic pop
        if (result != mock_response_len) {
            return CURLE_WRITE_ERROR;
        }
    }

    return mock_perform_error_code;
}

const char *curl_easy_strerror_(CURLcode code)
{
    switch (code) {
        case CURLE_OK:
            return "No error";
        case CURLE_WRITE_ERROR:
            return "Write error";
        default:
            return "Unknown error";
    }
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    return curl_slist_append(list, string);
}

void curl_slist_free_all_(struct curl_slist *list)
{
    if (list != NULL) {
        curl_slist_free_all(list);
    }
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_write_callback = (curl_write_callback)val;
    } else if (opt == CURLOPT_WRITEDATA) {
        g_write_data = (void *)val;
    }
#pragma GCC diagnostic pop

    return curl_easy_setopt(curl, opt, val);
}

/*
 * Test: API key too long
 *
 * Tests line 220: if (written < 0 || (size_t)written >= sizeof(auth_header))
 * Buffer is 256 bytes, "Authorization: Bearer " is 22 bytes
 * So API key must be >= 234 bytes to trigger overflow
 */
START_TEST(test_api_key_too_long) {
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    /* Create API key that will overflow the 256-byte buffer */
    char *long_key = talloc_array(cfg, char, 250);
    memset(long_key, 'x', 249);
    long_key[249] = '\0';
    cfg->openai_api_key = long_key;
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(conv, "user", "Test");
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(!add_res.is_err);

    /* Should fail with INVALID_ARG error */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);
    ck_assert(result.is_err);
    ck_assert(result.err->code == ERR_INVALID_ARG);
}
END_TEST

/*
 * Streaming callback that returns an error
 */
static res_t failing_stream_callback(const char *content, void *user_ctx)
{
    (void)content;
    (void)user_ctx;
    return ERR(callback_ctx, IO, "Callback intentionally failing");
}

/*
 * Test: Streaming callback error propagation
 *
 * Tests line 242: if (write_ctx->has_error)
 * This tests the defensive check where curl returns OK but callback had error
 */
START_TEST(test_callback_error_propagation_defensive) {
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(conv, "user", "Test");
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(!add_res.is_err);

    /* Set up mock response */
    const char *response = "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Mock curl to return OK despite callback error */
    mock_perform_should_return_ok = true;

    /* Use a callback that will fail */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, failing_stream_callback, NULL);

    /* Should fail with IO error */
    ck_assert(result.is_err);
    ck_assert(result.err->code == ERR_IO);
}
END_TEST
/*
 * Test: Streaming callback error (normal curl error path)
 *
 * This tests the normal case where callback error causes CURLE_WRITE_ERROR
 */
START_TEST(test_callback_error_normal_path)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(conv, "user", "Test");
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(!add_res.is_err);

    /* Set up mock response */
    const char *response = "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Normal path: curl will return CURLE_WRITE_ERROR */
    mock_perform_should_return_ok = false;
    mock_perform_error_code = CURLE_OK;

    /* Use a callback that will fail */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, failing_stream_callback, NULL);

    /* Should fail with IO error */
    ck_assert(result.is_err);
    ck_assert(result.err->code == ERR_IO);
}

END_TEST

/*
 * Test suite
 */
static Suite *http_handler_error_paths_suite(void)
{
    Suite *s = suite_create("HTTP Handler Error Paths");

    TCase *tc_buffer = tcase_create("Buffer Overflow");
    tcase_add_checked_fixture(tc_buffer, setup, teardown);
    tcase_add_test(tc_buffer, test_api_key_too_long);
    suite_add_tcase(s, tc_buffer);

    TCase *tc_callback = tcase_create("Callback Errors");
    tcase_add_checked_fixture(tc_callback, setup, teardown);
    tcase_add_test(tc_callback, test_callback_error_propagation_defensive);
    tcase_add_test(tc_callback, test_callback_error_normal_path);
    suite_add_tcase(s, tc_callback);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = http_handler_error_paths_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
