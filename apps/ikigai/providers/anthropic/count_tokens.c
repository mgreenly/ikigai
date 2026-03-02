/**
 * @file count_tokens.c
 * @brief Anthropic count_tokens vtable method implementation
 *
 * Makes a synchronous blocking HTTP POST to /v1/messages/count_tokens
 * and returns the input_tokens integer. Falls back to bytes estimator
 * on any error.
 */

#include "apps/ikigai/providers/anthropic/count_tokens.h"
#include "apps/ikigai/providers/anthropic/anthropic_internal.h"
#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/token_count.h"
#include "apps/ikigai/debug_log.h"
#include "shared/panic.h"
#include "shared/wrapper.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
#include <curl/curl.h>


#include "shared/poison.h"

/* ================================================================
 * Response buffer for curl write callback
 * ================================================================ */

typedef struct {
    TALLOC_CTX *ctx;
    char *buf;
    size_t len;
    size_t cap;
} resp_buf_t;

static size_t count_tokens_write_cb(const char *data, size_t size, size_t nmemb, void *userdata)
{
    resp_buf_t *resp = (resp_buf_t *)userdata;
    size_t total = size * nmemb;
    size_t new_len = resp->len + total;

    if (new_len + 1 > resp->cap) {
        size_t new_cap = resp->cap * 2;
        if (new_cap < new_len + 1) {
            new_cap = new_len + 1;
        }
        char *new_buf = talloc_realloc_(resp->ctx, resp->buf, new_cap);
        if (new_buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        resp->buf = new_buf;
        resp->cap = new_cap;
    }

    memcpy(resp->buf + resp->len, data, total);
    resp->len = new_len;
    resp->buf[resp->len] = '\0';

    return total;
}

/* ================================================================
 * HTTP implementation (real curl - wrapped for testability)
 * ================================================================ */

res_t ik_anthropic_count_tokens_http(TALLOC_CTX *ctx,
                                     const char *url,
                                     const char *api_key,
                                     const char *body,
                                     char **response_out,
                                     long *http_status_out)
{
    assert(ctx != NULL);             // LCOV_EXCL_BR_LINE
    assert(url != NULL);             // LCOV_EXCL_BR_LINE
    assert(api_key != NULL);         // LCOV_EXCL_BR_LINE
    assert(body != NULL);            // LCOV_EXCL_BR_LINE
    assert(response_out != NULL);    // LCOV_EXCL_BR_LINE
    assert(http_status_out != NULL); // LCOV_EXCL_BR_LINE

    *response_out = NULL;
    *http_status_out = 0;

    TALLOC_CTX *tmp = talloc_new(ctx);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Initialize response buffer */
    resp_buf_t resp = {0};
    resp.ctx = tmp;
    resp.cap = 4096;
    resp.buf = talloc_zero_array(tmp, char, (unsigned int)resp.cap);
    if (resp.buf == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp);   // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    /* Build header list */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append_(headers, "Content-Type: application/json");
    headers = curl_slist_append_(headers, "anthropic-version: 2023-06-01");

    char *auth_header = talloc_asprintf(tmp, "x-api-key: %s", api_key);
    if (auth_header == NULL) { // LCOV_EXCL_BR_LINE
        curl_slist_free_all_(headers); // LCOV_EXCL_LINE
        talloc_free(tmp);              // LCOV_EXCL_LINE
        PANIC("Out of memory");        // LCOV_EXCL_LINE
    }
    headers = curl_slist_append_(headers, auth_header);

    /* Initialize curl */
    CURL *curl = curl_easy_init_();
    if (curl == NULL) {
        curl_slist_free_all_(headers);
        talloc_free(tmp);
        return ERR(ctx, IO, "Failed to initialize curl easy handle");
    }

    curl_easy_setopt_(curl, CURLOPT_URL, url);
#ifdef NDEBUG
    curl_easy_setopt_(curl, CURLOPT_POST, 1L);
    curl_easy_setopt_(curl, CURLOPT_TIMEOUT, 30L);
#else
    curl_easy_setopt_(curl, CURLOPT_POST, (const void *)1L);
    curl_easy_setopt_(curl, CURLOPT_TIMEOUT, (const void *)30L);
#endif
    curl_easy_setopt_(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt_(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt_(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, count_tokens_write_cb);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode cres = curl_easy_perform_(curl);
    if (cres != CURLE_OK) {
        curl_easy_cleanup_(curl);
        curl_slist_free_all_(headers);
        talloc_free(tmp);
        return ERR(ctx, IO, "curl_easy_perform failed: %s", curl_easy_strerror_(cres));
    }

    long http_code = 0;
    curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup_(curl);
    curl_slist_free_all_(headers);

    *http_status_out = http_code;
    *response_out = talloc_strdup(ctx, resp.buf);
    talloc_free(tmp);

    if (*response_out == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(NULL);
}

/* ================================================================
 * Response parsing
 * ================================================================ */

static bool parse_input_tokens(const char *json_body, int32_t *token_count_out)
{
    if (json_body == NULL || json_body[0] == '\0') {
        return false;
    }

    yyjson_doc *doc = yyjson_read_(json_body, strlen(json_body), 0);
    if (doc == NULL) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL) {
        yyjson_doc_free(doc);
        return false;
    }

    yyjson_val *tok_val = yyjson_obj_get_(root, "input_tokens");
    if (tok_val == NULL || !yyjson_is_int(tok_val)) {
        yyjson_doc_free(doc);
        return false;
    }

    int64_t count = yyjson_get_sint_(tok_val);
    yyjson_doc_free(doc);

    if (count < 0) {
        return false;
    }

    *token_count_out = (int32_t)count;
    return true;
}

/* ================================================================
 * Bytes estimator fallback helper
 * ================================================================ */

static size_t estimate_request_bytes(const ik_request_t *req)
{
    size_t total = 0;

    if (req->system_prompt != NULL) {
        total += strlen(req->system_prompt);
    }

    for (size_t i = 0; i < req->message_count; i++) {
        const ik_message_t *msg = &req->messages[i];
        for (size_t j = 0; j < msg->content_count; j++) {
            const ik_content_block_t *blk = &msg->content_blocks[j];
            if (blk->type == IK_CONTENT_TEXT && blk->data.text.text != NULL) {
                total += strlen(blk->data.text.text);
            }
        }
    }

    return total;
}

/* ================================================================
 * Vtable method
 * ================================================================ */

res_t ik_anthropic_count_tokens(void *ctx,
                                const ik_request_t *req,
                                int32_t *token_count_out)
{
    assert(ctx != NULL);             // LCOV_EXCL_BR_LINE
    assert(req != NULL);             // LCOV_EXCL_BR_LINE
    assert(token_count_out != NULL); // LCOV_EXCL_BR_LINE

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)ctx;

    TALLOC_CTX *tmp = talloc_new(ctx);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Serialize request (no max_tokens, no stream) */
    char *body = NULL;
    res_t r = ik_anthropic_serialize_request_count_tokens(tmp, req, &body);
    if (is_err(&r)) {
        DEBUG_LOG("[count_tokens] serialization failed, using bytes estimator");
        size_t bytes = estimate_request_bytes(req);
        *token_count_out = ik_token_count_from_bytes(bytes);
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Build endpoint URL */
    char *url = talloc_asprintf(tmp, "%s/v1/messages/count_tokens", impl_ctx->base_url);
    if (url == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp);       // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    /* Make HTTP request via mockable wrapper */
    char *response = NULL;
    long http_status = 0;
    r = ik_anthropic_count_tokens_http_(tmp, url, impl_ctx->api_key,
                                        body, &response, &http_status);

    if (is_err(&r)) {
        DEBUG_LOG("[count_tokens] HTTP request failed, using bytes estimator");
        size_t bytes = strlen(body);
        *token_count_out = ik_token_count_from_bytes(bytes);
        talloc_free(tmp);
        return OK(NULL);
    }

    if (http_status < 200 || http_status >= 300) {
        DEBUG_LOG("[count_tokens] HTTP %ld, using bytes estimator", http_status);
        size_t bytes = strlen(body);
        *token_count_out = ik_token_count_from_bytes(bytes);
        talloc_free(tmp);
        return OK(NULL);
    }

    /* Parse response */
    int32_t count = 0;
    if (!parse_input_tokens(response, &count)) {
        DEBUG_LOG("[count_tokens] response parse failed, using bytes estimator");
        size_t bytes = strlen(body);
        *token_count_out = ik_token_count_from_bytes(bytes);
        talloc_free(tmp);
        return OK(NULL);
    }

    *token_count_out = count;
    talloc_free(tmp);
    return OK(NULL);
}
