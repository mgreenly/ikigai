#include "openai/client.h"
#include "openai/sse_parser.h"
#include "error.h"
#include "wrapper.h"
#include "json_allocator.h"
#include "vendor/yyjson/yyjson.h"
#include <talloc.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>
#include <sys/select.h>

/**
 * OpenAI API client implementation
 *
 * This module provides HTTP client functionality for OpenAI's Chat Completions API
 * with support for streaming responses via Server-Sent Events (SSE).
 */

/*
 * Internal wrapper function
 */

ik_openai_msg_t *get_message_at_index(ik_openai_msg_t **messages, size_t idx)
{
    return messages[idx];
}

/*
 * Message functions
 */

res_t ik_openai_msg_create(void *parent, const char *role, const char *content) {
    assert(role != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_msg_t *msg = talloc_zero(parent, ik_openai_msg_t);
    if (!msg) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate message"); // LCOV_EXCL_LINE
    }

    msg->role = talloc_strdup(msg, role);
    if (!msg->role) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate role string"); // LCOV_EXCL_LINE
    }

    msg->content = talloc_strdup(msg, content);
    if (!msg->content) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate content string"); // LCOV_EXCL_LINE
    }

    return OK(msg);
}

/*
 * Conversation functions
 */

res_t ik_openai_conversation_create(void *parent) {
    ik_openai_conversation_t *conv = talloc_zero(parent, ik_openai_conversation_t);
    if (!conv) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate conversation"); // LCOV_EXCL_LINE
    }

    conv->messages = NULL;
    conv->message_count = 0;

    return OK(conv);
}

res_t ik_openai_conversation_add_msg(ik_openai_conversation_t *conv, ik_openai_msg_t *msg) {
    assert(conv != NULL); // LCOV_EXCL_BR_LINE
    assert(msg != NULL); // LCOV_EXCL_BR_LINE

    /* Resize messages array */
    ik_openai_msg_t **new_messages = talloc_realloc_(conv, conv->messages,
                                                      sizeof(ik_openai_msg_t *) * (conv->message_count + 1));
    if (!new_messages) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to resize messages array"); // LCOV_EXCL_LINE
    }

    conv->messages = new_messages;
    conv->messages[conv->message_count] = msg;
    conv->message_count++;

    /* Reparent message to conversation */
    talloc_steal(conv, msg);

    return OK(NULL);
}

void ik_openai_conversation_clear(ik_openai_conversation_t *conv) {
    assert(conv != NULL); // LCOV_EXCL_BR_LINE

    /* Free all messages */
    for (size_t i = 0; i < conv->message_count; i++) {  // LCOV_EXCL_BR_LINE
        talloc_free(conv->messages[i]);
    }

    /* Free messages array */
    if (conv->messages != NULL) {  // LCOV_EXCL_BR_LINE
        talloc_free(conv->messages);
    }

    /* Reset to empty state */
    conv->messages = NULL;
    conv->message_count = 0;
}

/*
 * Request/Response functions
 */

ik_openai_request_t *ik_openai_request_create(void *parent, const ik_cfg_t *cfg,
                                               ik_openai_conversation_t *conv) {
    assert(cfg != NULL); // LCOV_EXCL_BR_LINE
    assert(conv != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_request_t *req = talloc_zero(parent, ik_openai_request_t);
    if (!req) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate request"); // LCOV_EXCL_LINE
    }

    req->model = talloc_strdup(req, cfg->openai_model);
    if (!req->model) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate model string"); // LCOV_EXCL_LINE
    }

    req->conv = conv;  /* Borrowed reference, not owned */
    req->temperature = cfg->openai_temperature;
    req->max_completion_tokens = cfg->openai_max_completion_tokens;
    req->stream = true;  /* Always enable streaming */

    return req;
}

ik_openai_response_t *ik_openai_response_create(void *parent) {
    ik_openai_response_t *resp = talloc_zero(parent, ik_openai_response_t);
    if (!resp) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate response"); // LCOV_EXCL_LINE
    }

    resp->content = NULL;
    resp->finish_reason = NULL;
    resp->prompt_tokens = 0;
    resp->completion_tokens = 0;
    resp->total_tokens = 0;

    return resp;
}

/*
 * JSON serialization
 */

char *ik_openai_serialize_request(void *parent, const ik_openai_request_t *request) {
    assert(request != NULL); // LCOV_EXCL_BR_LINE
    assert(request->conv != NULL); // LCOV_EXCL_BR_LINE

    /* Create yyjson document with talloc allocator */
    yyjson_alc allocator = ik_make_talloc_allocator(parent);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Create root object */
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    /* Add model field */
    if (!yyjson_mut_obj_add_str(doc, root, "model", request->model)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add model field to JSON"); // LCOV_EXCL_LINE
    }

    /* Create messages array */
    yyjson_mut_val *messages_arr = yyjson_mut_arr(doc);
    if (messages_arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Add each message to the array */
    for (size_t i = 0; i < request->conv->message_count; i++) {
        ik_openai_msg_t *msg = get_message_at_index(request->conv->messages, i);

        /* Create message object */
        yyjson_mut_val *msg_obj = yyjson_mut_arr_add_obj(doc, messages_arr);
        if (msg_obj == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        /* Add role and content fields */
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", msg->role)) { // LCOV_EXCL_BR_LINE
            PANIC("Failed to add role field to message"); // LCOV_EXCL_LINE
        }
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", msg->content)) { // LCOV_EXCL_BR_LINE
            PANIC("Failed to add content field to message"); // LCOV_EXCL_LINE
        }
    }

    /* Add messages array to root */
    if (!yyjson_mut_obj_add_val(doc, root, "messages", messages_arr)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add messages array to JSON"); // LCOV_EXCL_LINE
    }

    /* Add temperature field */
    if (!yyjson_mut_obj_add_real(doc, root, "temperature", request->temperature)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add temperature field to JSON"); // LCOV_EXCL_LINE
    }

    /* Add max_completion_tokens field */
    if (!yyjson_mut_obj_add_int(doc, root, "max_completion_tokens", (int64_t)request->max_completion_tokens)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add max_completion_tokens field to JSON"); // LCOV_EXCL_LINE
    }

    /* Add stream field */
    if (!yyjson_mut_obj_add_bool(doc, root, "stream", request->stream)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add stream field to JSON"); // LCOV_EXCL_LINE
    }

    /* Serialize to JSON string */
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    /* Copy JSON string to talloc context */
    char *result = talloc_strdup(parent, json_str);
    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Free yyjson-allocated string (not managed by talloc) */
    free(json_str);

    return result;
}

/*
 * HTTP client
 */

/*
 * Internal HTTP response structure
 *
 * Holds both content and metadata from streaming response.
 */
typedef struct {
    char *content;          /* Complete response content */
    char *finish_reason;    /* Finish reason (may be NULL) */
} http_response_t;

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

/*
 * Perform HTTP POST request with libcurl
 *
 * @param parent      Talloc context parent
 * @param url         API endpoint URL
 * @param api_key     OpenAI API key
 * @param request_body JSON request body
 * @param stream_cb   Streaming callback (or NULL)
 * @param cb_ctx      Callback context
 * @return            OK(http_response_t*) or ERR(...)
 */
static res_t http_post(void *parent, const char *url, const char *api_key,
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
    http_response_t *http_resp = talloc_zero(parent, http_response_t);
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

res_t ik_openai_chat_create(void *parent, const ik_cfg_t *cfg,
                             ik_openai_conversation_t *conv,
                             ik_openai_stream_cb_t stream_cb, void *cb_ctx) {
    assert(cfg != NULL); // LCOV_EXCL_BR_LINE
    assert(conv != NULL); // LCOV_EXCL_BR_LINE

    /* Validate inputs */
    if (conv->message_count == 0) {
        return ERR(parent, INVALID_ARG, "Conversation must contain at least one message");
    }

    if (cfg->openai_api_key == NULL || strlen(cfg->openai_api_key) == 0) {
        return ERR(parent, INVALID_ARG, "OpenAI API key is required");
    }

    /* Create request */
    ik_openai_request_t *request = ik_openai_request_create(parent, cfg, conv);

    /* Serialize request to JSON */
    char *json_body = ik_openai_serialize_request(parent, request);

    /* Perform HTTP POST */
    const char *url = "https://api.openai.com/v1/chat/completions";
    res_t http_res = http_post(parent, url, cfg->openai_api_key, json_body, stream_cb, cb_ctx);
    if (http_res.is_err) {
        return http_res;
    }

    /* Extract HTTP response data */
    http_response_t *http_resp = http_res.ok;
    ik_openai_response_t *response = ik_openai_response_create(parent);
    response->content = talloc_steal(response, http_resp->content);

    if (http_resp->finish_reason != NULL) {
        response->finish_reason = talloc_steal(response, http_resp->finish_reason);
    }

    talloc_free(http_resp);

    return OK(response);
}
