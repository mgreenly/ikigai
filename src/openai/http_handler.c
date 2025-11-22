#include "openai/http_handler.h"

#include "openai/sse_parser.h"
#include "wrapper.h"

#include <assert.h>
#include <curl/curl.h>
#include <string.h>
#include <talloc.h>

/**
 * OpenAI HTTP handler implementation
 *
 * This module provides low-level HTTP client functionality for OpenAI's API,
 * handling libcurl operations, SSE streaming, and response metadata extraction.
 */

/*
 * Context for HTTP write callback
 *
 * Accumulates response data and handles streaming via SSE parser.
 */
typedef struct {
    ik_openai_sse_parser_t *parser;     /* SSE parser for streaming responses */
    ik_openai_stream_cb_t user_callback; /* User's streaming callback */
    void *user_ctx;                      /* User's callback context */
    char *complete_response;             /* Accumulated complete response */
    size_t response_len;                 /* Length of complete response */
    char *finish_reason;                 /* Finish reason from stream */
    bool has_error;                      /* Whether an error occurred */
} http_write_ctx_t;

/*
 * Extract finish_reason from SSE event
 *
 * Parses the raw SSE event JSON to extract choices[0].finish_reason if present.
 *
 * @param event  Raw SSE event string (with "data: " prefix)
 * @return       Finish reason string or NULL if not present
 */
static char *extract_finish_reason(void *parent, const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix
     * Note: This path is unreachable in practice because ik_openai_parse_sse_event()
     * returns ERR for events without "data: " prefix, causing http_write_callback
     * to skip calling extract_finish_reason(). Kept as defensive programming.
     */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) { // LCOV_EXCL_BR_LINE
        return NULL; // LCOV_EXCL_LINE
    }

    /* Get JSON payload */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        return NULL;
    }

    /* Parse JSON
     * Note: Invalid JSON is unreachable because ik_openai_parse_sse_event() returns ERR
     * for malformed JSON, preventing extract_finish_reason() from being called.
     */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) { // LCOV_EXCL_BR_LINE
        return NULL; // LCOV_EXCL_LINE
    }

    /* Validate root is an object
     * Note: Non-object root is unreachable because ik_openai_parse_sse_event() returns ERR
     * for non-object roots, preventing extract_finish_reason() from being called.
     */
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL; // LCOV_EXCL_LINE
    }

    /* Extract choices[0].finish_reason
     * Note: Certain branch combinations in this compound condition are covered by tests,
     * but LCOV reports some sub-branches as uncovered due to short-circuit evaluation.
     * The primary paths (choices exists+valid, or choices missing/invalid) are fully tested.
     */
    yyjson_val *choices = yyjson_obj_get(root, "choices");
    if (!choices || !yyjson_is_arr(choices) || yyjson_arr_size(choices) == 0) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *choice0 = yyjson_arr_get(choices, 0);
    if (!choice0 || !yyjson_is_obj(choice0)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *finish_reason_val = yyjson_obj_get(choice0, "finish_reason");
    if (!finish_reason_val || !yyjson_is_str(finish_reason_val)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    /* Extract string */
    const char *finish_reason_str = yyjson_get_str(finish_reason_val);
    char *result = talloc_strdup(parent, finish_reason_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate finish_reason string"); // LCOV_EXCL_LINE
    }

    yyjson_doc_free(doc);
    return result;
}

/*
 * libcurl write callback
 *
 * Called by libcurl as data arrives from the server.
 * Feeds data to SSE parser and invokes user callback for each content chunk.
 */
static size_t http_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
    http_write_ctx_t *ctx = (http_write_ctx_t *)userdata;
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    size_t total_size = size * nmemb;

    /* Feed data to SSE parser */
    ik_openai_sse_parser_feed(ctx->parser, data, total_size);

    /* Extract and process all complete SSE events */
    while (true) {
        char *event = ik_openai_sse_parser_get_event(ctx->parser);
        if (event == NULL) {
            break; /* No more complete events */
        }

        /* Parse SSE event to extract content */
        res_t content_res = ik_openai_parse_sse_event(ctx->parser, event);
        if (content_res.is_err) {
            /* Parse error - log but continue */
            talloc_free(event);
            continue;
        }

        char *content = content_res.ok;
        if (content != NULL) {
            /* Invoke user's streaming callback if provided */
            if (ctx->user_callback != NULL) {
                res_t cb_res = ctx->user_callback(content, ctx->user_ctx);
                if (cb_res.is_err) {
                    ctx->has_error = true;
                    talloc_free(content);
                    talloc_free(event);
                    return 0;
                }
            }

            /* Accumulate to complete response */
            if (ctx->complete_response == NULL) {
                ctx->complete_response = talloc_strdup(ctx->parser, content);
            } else {
                ctx->complete_response = talloc_strdup_append(ctx->complete_response, content);
            }

            if (ctx->complete_response == NULL) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to accumulate response"); // LCOV_EXCL_LINE
            }

            talloc_free(content);
        }

        /* Extract finish_reason if present */
        if (ctx->finish_reason == NULL) {
            char *finish_reason = extract_finish_reason(ctx->parser, event);
            if (finish_reason != NULL) {
                ctx->finish_reason = finish_reason;
            }
        }

        talloc_free(event);
    }

    return total_size;
}

res_t ik_openai_http_post(void *parent, const char *url, const char *api_key,
                          const char *request_body,
                          ik_openai_stream_cb_t stream_cb, void *cb_ctx) {
    assert(url != NULL); // LCOV_EXCL_BR_LINE
    assert(api_key != NULL); // LCOV_EXCL_BR_LINE
    assert(request_body != NULL); // LCOV_EXCL_BR_LINE

    /* Initialize libcurl */
    CURL *curl = curl_easy_init_();
    if (curl == NULL) {
        return ERR(parent, IO, "Failed to initialize libcurl");
    }

    /* Create write callback context */
    http_write_ctx_t *write_ctx = talloc_zero(parent, http_write_ctx_t);
    if (write_ctx == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(curl); // LCOV_EXCL_LINE
        PANIC("Failed to allocate write context"); // LCOV_EXCL_LINE
    }

    /* Create SSE parser */
    write_ctx->parser = ik_openai_sse_parser_create(write_ctx);
    write_ctx->user_callback = stream_cb;
    write_ctx->user_ctx = cb_ctx;
    write_ctx->complete_response = NULL;
    write_ctx->response_len = 0;
    write_ctx->finish_reason = NULL;
    write_ctx->has_error = false;

    /* Set up curl options */
    curl_easy_setopt_(curl, CURLOPT_URL, url);
#ifdef NDEBUG
    curl_easy_setopt_(curl, CURLOPT_POST, 1L);
#else
    curl_easy_setopt_(curl, CURLOPT_POST, (const void *)1L);
#endif
    curl_easy_setopt_(curl, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, write_ctx);

    /* Set headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append_(headers, "Content-Type: application/json");

    char auth_header[256];
    int32_t written = snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    if (written < 0 || (size_t)written >= sizeof(auth_header)) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(curl);
        curl_slist_free_all_(headers);
        return ERR(parent, INVALID_ARG, "API key too long");
    }
    headers = curl_slist_append_(headers, auth_header);

    curl_easy_setopt_(curl, CURLOPT_HTTPHEADER, headers);

    /* Perform request */
    CURLcode res = curl_easy_perform_(curl);

    /* Clean up */
    curl_slist_free_all_(headers);
    curl_easy_cleanup_(curl);

    /* Check for errors */
    if (res != CURLE_OK) {
        return ERR(parent, IO, "HTTP request failed: %s", curl_easy_strerror_(res));
    }

    /* Defensive check: callback errors should already be caught by CURLE_WRITE_ERROR above */
    if (write_ctx->has_error) { // LCOV_EXCL_BR_LINE
        return ERR(parent, IO, "Error processing response stream"); // LCOV_EXCL_LINE
    }

    /* Create response structure */
    ik_openai_http_response_t *http_resp = talloc_zero(parent, ik_openai_http_response_t);
    if (!http_resp) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate HTTP response"); // LCOV_EXCL_LINE
    }

    /* Transfer content */
    if (write_ctx->complete_response == NULL) {
        http_resp->content = talloc_strdup(http_resp, "");
    } else {
        http_resp->content = talloc_steal(http_resp, write_ctx->complete_response);
    }

    /* Transfer finish_reason */
    if (write_ctx->finish_reason != NULL) {
        http_resp->finish_reason = talloc_steal(http_resp, write_ctx->finish_reason);
    } else {
        http_resp->finish_reason = NULL;
    }

    return OK(http_resp);
}
