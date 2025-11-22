#include "openai/client.h"

#include "error.h"
#include "json_allocator.h"
#include "openai/http_handler.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

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
    res_t http_res = ik_openai_http_post(parent, url, cfg->openai_api_key, json_body, stream_cb, cb_ctx);
    if (http_res.is_err) {
        return http_res;
    }

    /* Extract HTTP response data */
    ik_openai_http_response_t *http_resp = http_res.ok;
    ik_openai_response_t *response = ik_openai_response_create(parent);
    response->content = talloc_steal(response, http_resp->content);

    if (http_resp->finish_reason != NULL) {
        response->finish_reason = talloc_steal(response, http_resp->finish_reason);
    }

    talloc_free(http_resp);

    return OK(response);
}
