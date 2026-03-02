/**
 * @file count_tokens.c
 * @brief OpenAI token counting via POST /v1/responses/input_tokens
 */

#include "apps/ikigai/providers/openai/count_tokens.h"
#include "apps/ikigai/providers/openai/request.h"
#include "apps/ikigai/token_count.h"
#include "apps/ikigai/debug_log.h"
#include "shared/panic.h"
#include "shared/wrapper.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>


#include "shared/poison.h"

/* ================================================================
 * Response accumulation buffer for curl write callback
 * ================================================================ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} response_buf_t;

static size_t write_callback(const char *ptr, size_t size, size_t nmemb, void *userdata)
{
    response_buf_t *buf = (response_buf_t *)userdata;
    size_t incoming = size * nmemb;

    size_t new_len = buf->len + incoming;
    if (new_len + 1 > buf->cap) {
        size_t new_cap = buf->cap * 2;
        if (new_cap < new_len + 1) {
            new_cap = new_len + 1;
        }
        char *new_data = talloc_realloc_(NULL, buf->data, new_cap);
        if (!new_data) {
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }
        buf->data = new_data;
        buf->cap = new_cap;
    }

    memcpy(buf->data + buf->len, ptr, incoming);
    buf->len = new_len;
    buf->data[buf->len] = '\0';

    return incoming;
}

/* ================================================================
 * Public implementation
 * ================================================================ */

res_t ik_openai_count_tokens(TALLOC_CTX *ctx_talloc,
                              const char *base_url,
                              const char *api_key,
                              const ik_request_t *req,
                              int32_t *out)
{
    assert(ctx_talloc != NULL); // LCOV_EXCL_BR_LINE
    assert(base_url != NULL);   // LCOV_EXCL_BR_LINE
    assert(api_key != NULL);    // LCOV_EXCL_BR_LINE
    assert(req != NULL);        // LCOV_EXCL_BR_LINE
    assert(out != NULL);        // LCOV_EXCL_BR_LINE

    /* Serialize request in Responses API format (stream=false) */
    char *json_body = NULL;
    res_t ser_res = ik_openai_serialize_responses_request(ctx_talloc, req, false, &json_body);
    if (is_err(&ser_res)) {
        /* Fallback: bytes estimate */
        DEBUG_LOG("[count_tokens] serialize failed, falling back to bytes estimate");
        *out = ik_token_count_from_bytes(strlen(req->system_prompt ? req->system_prompt : ""));
        talloc_free(ser_res.err);
        return OK(NULL);
    }

    /* Build URL: {base_url}/v1/responses/input_tokens */
    char *url = talloc_asprintf(ctx_talloc, "%s/v1/responses/input_tokens", base_url);
    if (!url) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Build headers */
    char **headers = NULL;
    res_t hdr_res = ik_openai_build_headers(ctx_talloc, api_key, &headers);
    if (is_err(&hdr_res)) {
        /* Fallback: bytes estimate */
        DEBUG_LOG("[count_tokens] build_headers failed, falling back to bytes estimate");
        *out = ik_token_count_from_bytes(strlen(json_body));
        talloc_free(hdr_res.err);
        return OK(NULL);
    }

    /* Set up curl easy handle */
    CURL *easy = curl_easy_init_();
    if (!easy) {
        DEBUG_LOG("[count_tokens] curl_easy_init failed, falling back to bytes estimate");
        *out = ik_token_count_from_bytes(strlen(json_body));
        return OK(NULL);
    }

    /* Set up response buffer */
    response_buf_t buf = { NULL, 0, 256 };
    buf.data = talloc_zero_size(ctx_talloc, buf.cap);
    if (!buf.data) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Build curl_slist from headers array */
    struct curl_slist *header_list = NULL;
    for (size_t i = 0; headers[i] != NULL; i++) {
        struct curl_slist *new_list = curl_slist_append_(header_list, headers[i]);
        if (!new_list) {
            curl_slist_free_all_(header_list);
            curl_easy_cleanup_(easy);
            DEBUG_LOG("[count_tokens] curl_slist_append failed, falling back to bytes estimate");
            *out = ik_token_count_from_bytes(strlen(json_body));
            return OK(NULL);
        }
        header_list = new_list;
    }

    /* Configure curl */
    curl_easy_setopt_(easy, CURLOPT_URL, url);
    curl_easy_setopt_(easy, CURLOPT_POST, 1L);
    curl_easy_setopt_(easy, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt_(easy, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    curl_easy_setopt_(easy, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt_(easy, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt_(easy, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt_(easy, CURLOPT_TIMEOUT, 30L);

    /* Perform synchronous request */
    CURLcode curl_res = curl_easy_perform_(easy);

    if (curl_res != CURLE_OK) {
        DEBUG_LOG("[count_tokens] curl error: %s, falling back to bytes estimate",
                  curl_easy_strerror_(curl_res));
        curl_slist_free_all_(header_list);
        curl_easy_cleanup_(easy);
        *out = ik_token_count_from_bytes(strlen(json_body));
        return OK(NULL);
    }

    /* Check HTTP status code */
    long http_code = 0;
    curl_easy_getinfo_(easy, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all_(header_list);
    curl_easy_cleanup_(easy);

    if (http_code < 200 || http_code >= 300) {
        DEBUG_LOG("[count_tokens] HTTP %ld error, falling back to bytes estimate", http_code);
        *out = ik_token_count_from_bytes(strlen(json_body));
        return OK(NULL);
    }

    /* Parse response JSON: { "object": "response.input_tokens", "input_tokens": N } */
    yyjson_doc *doc = yyjson_read(buf.data, buf.len, 0);
    if (!doc) {
        DEBUG_LOG("[count_tokens] JSON parse failed, falling back to bytes estimate");
        *out = ik_token_count_from_bytes(strlen(json_body));
        return OK(NULL);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *input_tokens_val = yyjson_obj_get(root, "input_tokens");

    if (!input_tokens_val || !yyjson_is_int(input_tokens_val)) {
        DEBUG_LOG("[count_tokens] missing input_tokens field, falling back to bytes estimate");
        yyjson_doc_free(doc);
        *out = ik_token_count_from_bytes(strlen(json_body));
        return OK(NULL);
    }

    int64_t token_count = yyjson_get_int(input_tokens_val);
    yyjson_doc_free(doc);

    *out = (int32_t)token_count;
    return OK(NULL);
}
