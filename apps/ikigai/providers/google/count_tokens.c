/**
 * @file count_tokens.c
 * @brief Google provider synchronous token counting via countTokens API
 *
 * Makes a blocking HTTP POST to:
 *   POST {base_url}/models/{model}:countTokens?key={api_key}
 *
 * Wraps the existing generateContent request body in a generateContentRequest
 * envelope as required by the countTokens API. Falls back to the bytes
 * estimator on any error.
 */

#include "apps/ikigai/providers/google/count_tokens.h"
#include "apps/ikigai/providers/google/request.h"
#include "apps/ikigai/debug_log.h"
#include "apps/ikigai/token_count.h"
#include "shared/panic.h"
#include "shared/wrapper_curl.h"
#include "vendor/yyjson/yyjson.h"

#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "shared/poison.h"

/* ================================================================
 * Response body accumulator
 * ================================================================ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    TALLOC_CTX *ctx;
} count_tokens_buf_t;

static size_t count_tokens_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    count_tokens_buf_t *b = (count_tokens_buf_t *)userdata;
    size_t total = size * nmemb;

    if (b->len + total + 1 > b->cap) {
        size_t new_cap = (b->len + total + 1) * 2;
        char *new_buf = talloc_realloc(b->ctx, b->buf, char, (unsigned)new_cap);
        if (!new_buf) {
            return 0; /* Signal error: curl will abort */
        }
        b->buf = new_buf;
        b->cap = new_cap;
    }

    memcpy(b->buf + b->len, ptr, total);
    b->len += total;
    b->buf[b->len] = '\0';

    return total;
}

/* ================================================================
 * Public API
 * ================================================================ */

res_t ik_google_count_tokens(void *ctx,
                              const char *base_url,
                              const char *api_key,
                              const ik_request_t *req,
                              int32_t *token_count_out)
{
    assert(ctx != NULL);             // LCOV_EXCL_BR_LINE
    assert(base_url != NULL);        // LCOV_EXCL_BR_LINE
    assert(api_key != NULL);         // LCOV_EXCL_BR_LINE
    assert(req != NULL);             // LCOV_EXCL_BR_LINE
    assert(token_count_out != NULL); // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = talloc_new(ctx);
    if (!tmp) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Serialize the request body (same as for generateContent) */
    char *body_json = NULL;
    res_t r = ik_google_serialize_request(tmp, req, &body_json);
    if (is_err(&r)) {
        DEBUG_LOG("[count_tokens] serialize failed, using bytes estimate");
        *token_count_out = ik_token_count_from_bytes(0);
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Wrap in generateContentRequest envelope.
     * body_json is always at least '{"contents":[...]}' from serialize_request.
     * body_json+1 strips the leading '{', leaving '"key":val,...}' including
     * the trailing '}' which closes the generateContentRequest object.
     * The outer '}' in the format string closes the root object. */
    size_t body_len = strlen(body_json);
    char *count_body = NULL;
    if (body_len <= 2) {
        /* Empty body: {"generateContentRequest":{"model":"models/X"}} */
        count_body = talloc_asprintf(tmp,
            "{\"generateContentRequest\":{\"model\":\"models/%s\"}}",
            req->model);
    } else {
        /* Normal case: inject model field and concatenate inner fields */
        count_body = talloc_asprintf(tmp,
            "{\"generateContentRequest\":{\"model\":\"models/%s\",%s}",
            req->model, body_json + 1);
    }
    if (!count_body) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Build countTokens URL */
    char *url = talloc_asprintf(tmp, "%s/models/%s:countTokens?key=%s",
                                base_url, req->model, api_key);
    if (!url) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Initialize curl for synchronous request */
    CURL *curl = curl_easy_init_();
    if (!curl) {
        DEBUG_LOG("[count_tokens] curl_easy_init failed, using bytes estimate");
        *token_count_out = ik_token_count_from_bytes(strlen(count_body));
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Set up response accumulator */
    count_tokens_buf_t response_buf = {0};
    response_buf.ctx = tmp;
    response_buf.cap = 512;
    response_buf.buf = talloc_size(tmp, response_buf.cap);
    if (!response_buf.buf) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    response_buf.buf[0] = '\0';

    /* Build request headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append_(headers, "Content-Type: application/json");
    if (!headers) {
        curl_easy_cleanup_(curl);
        DEBUG_LOG("[count_tokens] curl_slist_append failed, using bytes estimate");
        *token_count_out = ik_token_count_from_bytes(strlen(count_body));
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Configure curl options */
    curl_easy_setopt_(curl, CURLOPT_URL, url);
    curl_easy_setopt_(curl, CURLOPT_POST, (long)1);
    curl_easy_setopt_(curl, CURLOPT_POSTFIELDS, count_body);
    curl_easy_setopt_(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(count_body));
    curl_easy_setopt_(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, count_tokens_write_cb);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &response_buf);

    /* Perform blocking HTTP POST */
    CURLcode rc = curl_easy_perform_(curl);
    if (rc != CURLE_OK) {
        DEBUG_LOG("[count_tokens] network error (%s), using bytes estimate",
                  curl_easy_strerror_(rc));
        curl_slist_free_all_(headers);
        curl_easy_cleanup_(curl);
        *token_count_out = ik_token_count_from_bytes(strlen(count_body));
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Check HTTP status code */
    long http_status = 0;
    curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_status);
    curl_slist_free_all_(headers);
    curl_easy_cleanup_(curl);

    if (http_status < 200 || http_status >= 300) {
        DEBUG_LOG("[count_tokens] HTTP %ld from countTokens, using bytes estimate",
                  http_status);
        *token_count_out = ik_token_count_from_bytes(strlen(count_body));
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Parse response: extract totalTokens */
    yyjson_doc *resp_doc = yyjson_read(response_buf.buf, response_buf.len, 0);
    if (!resp_doc) {
        DEBUG_LOG("[count_tokens] failed to parse response JSON, using bytes estimate");
        *token_count_out = ik_token_count_from_bytes(strlen(count_body));
        talloc_free(tmp);
        return OK(NULL);
    }

    yyjson_val *root = yyjson_doc_get_root(resp_doc);
    yyjson_val *total_tokens_val = yyjson_obj_get(root, "totalTokens");
    if (!total_tokens_val || !yyjson_is_int(total_tokens_val)) {
        DEBUG_LOG("[count_tokens] totalTokens missing in response, using bytes estimate");
        yyjson_doc_free(resp_doc);
        *token_count_out = ik_token_count_from_bytes(strlen(count_body));
        talloc_free(tmp);
        return OK(NULL);
    }

    int64_t count = yyjson_get_int(total_tokens_val);
    yyjson_doc_free(resp_doc);

    *token_count_out = (count > (int64_t)INT32_MAX) ? INT32_MAX : (int32_t)count;
    talloc_free(tmp);
    return OK(NULL);
}
