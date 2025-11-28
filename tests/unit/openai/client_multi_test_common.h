/* Common test infrastructure for OpenAI multi-handle tests */

#ifndef CLIENT_MULTI_TEST_COMMON_H
#define CLIENT_MULTI_TEST_COMMON_H

#include "openai/client_multi.h"
#include "openai/client.h"
#include "config.h"
#include "error.h"
#include "wrapper.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <sys/select.h>
#include <stdarg.h>
#include <string.h>

/* Test context */
static void *ctx;

/* Mock control flags */
static bool fail_curl_multi_init = false;
static bool fail_curl_easy_init = false;
static bool fail_curl_multi_add_handle = false;
static bool fail_curl_multi_perform = false;
static bool fail_curl_multi_fdset = false;
static bool fail_curl_multi_timeout = false;
static bool fail_snprintf = false;
static CURLMsg *mock_curl_msg = NULL;
static long mock_http_response_code = 0;

/* Callback capture state for testing http_write_callback */
typedef size_t (*curl_write_callback)(char *data, size_t size, size_t nmemb, void *userdata);
static curl_write_callback g_write_callback = NULL;
static void *g_write_data = NULL;
static const char *mock_response_data = NULL;
static size_t mock_response_len = 0;
static bool invoke_write_callback = false;
static CURL *g_last_easy_handle = NULL;  /* Track last created easy handle */

static void setup(void)
{
    ctx = talloc_new(NULL);
    fail_curl_multi_init = false;
    fail_curl_easy_init = false;
    fail_curl_multi_add_handle = false;
    fail_curl_multi_perform = false;
    fail_curl_multi_fdset = false;
    fail_curl_multi_timeout = false;
    fail_snprintf = false;
    mock_curl_msg = NULL;
    mock_http_response_code = 0;
    g_write_callback = NULL;
    g_write_data = NULL;
    mock_response_data = NULL;
    mock_response_len = 0;
    invoke_write_callback = false;
    g_last_easy_handle = NULL;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/*
 * Mock curl functions (weak symbol overrides)
 */

/* Override curl_multi_init_ to inject failures */
CURLM *curl_multi_init_(void)
{
    if (fail_curl_multi_init) {
        return NULL;
    }
    return curl_multi_init();
}

/* Override curl_easy_init_ to inject failures */
CURL *curl_easy_init_(void)
{
    if (fail_curl_easy_init) {
        return NULL;
    }
    CURL *handle = curl_easy_init();
    g_last_easy_handle = handle;  /* Capture for testing */
    return handle;
}

/* Override curl_multi_add_handle_ to inject failures */
CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy)
{
    if (fail_curl_multi_add_handle) {
        return CURLM_BAD_EASY_HANDLE;
    }
    return curl_multi_add_handle(multi, easy);
}

/* Override curl_multi_perform_ to inject failures and invoke write callback */
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    if (fail_curl_multi_perform) {
        return CURLM_BAD_HANDLE;
    }

    /* Invoke write callback if requested for testing */
    if (invoke_write_callback && g_write_callback && mock_response_data) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        g_write_callback((char *)mock_response_data, 1, mock_response_len, g_write_data);
#pragma GCC diagnostic pop
    }

    return curl_multi_perform(multi, running_handles);
}

/* Override curl_multi_fdset_ to inject failures */
CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set,
                            int *max_fd)
{
    if (fail_curl_multi_fdset) {
        return CURLM_BAD_HANDLE;
    }
    return curl_multi_fdset(multi, read_fd_set, write_fd_set, exc_fd_set, max_fd);
}

/* Override curl_multi_timeout_ to inject failures */
CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    if (fail_curl_multi_timeout) {
        return CURLM_BAD_HANDLE;
    }
    return curl_multi_timeout(multi, timeout);
}

/* Override curl_multi_info_read_ to inject test messages */
CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    (void)multi;
    if (mock_curl_msg != NULL) {
        *msgs_in_queue = 0;
        CURLMsg *msg = mock_curl_msg;
        mock_curl_msg = NULL;  /* Return it only once */
        return msg;
    }
    *msgs_in_queue = 0;
    return NULL;
}

/* Override curl_easy_setopt_ to capture write callback */
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
    /* Capture callbacks we need for testing */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_write_callback = (curl_write_callback)val;
    } else if (opt == CURLOPT_WRITEDATA) {
        g_write_data = (void *)val;
    }
#pragma GCC diagnostic pop

    /* Call real curl_easy_setopt */
    return curl_easy_setopt(curl, opt, val);
}

/* Override curl_easy_getinfo_ to inject mock HTTP response code */
#undef curl_easy_getinfo_
CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...);
CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    va_list args;
    va_start(args, info);

    if (info == CURLINFO_RESPONSE_CODE) {
        long *response_code_ptr = va_arg(args, long *);
        *response_code_ptr = mock_http_response_code;
        va_end(args);
        return CURLE_OK;
    }

    /* For other info types, call real curl_easy_getinfo */
    void *param = va_arg(args, void *);
    va_end(args);
    return curl_easy_getinfo(curl, info, param);
}

/* Override snprintf_ to inject failures */
int snprintf_(char *str, size_t size, const char *format, ...)
{
    if (fail_snprintf) {
        return -1;
    }
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

/* Test callback that returns an error */
static inline res_t error_stream_callback(const char *content, void *user_ctx)
{
    (void)content;
    return ERR(user_ctx, IO, "Callback error");
}

/* Test callback that succeeds */
static inline res_t success_stream_callback(const char *content, void *user_ctx)
{
    (void)content;
    (void)user_ctx;
    return OK(NULL);
}

#endif /* CLIENT_MULTI_TEST_COMMON_H */
