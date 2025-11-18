#include "openai/client.h"
#include "error.h"
#include "wrapper.h"
#include "json_allocator.h"
#include "vendor/yyjson/yyjson.h"
#include <talloc.h>
#include <string.h>
#include <assert.h>

/**
 * OpenAI API client implementation
 *
 * This module provides HTTP client functionality for OpenAI's Chat Completions API
 * with support for streaming responses via Server-Sent Events (SSE).
 */

/*
 * yyjson wrapper functions
 *
 * These wrappers consolidate yyjson inline functions into single testable
 * locations. The inline functions contain defensive ternaries (e.g.,
 * doc ? doc->root : NULL) that create branches at every call site.
 * By wrapping them, we can test both branches once in unit tests.
 */

yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc)
{
    return yyjson_doc_get_root(doc);
}

yyjson_val *yyjson_arr_get_wrapper(yyjson_val *arr, size_t idx)
{
    return yyjson_arr_get(arr, idx);
}

bool yyjson_is_obj_wrapper(yyjson_val *val)
{
    return yyjson_is_obj(val);
}

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

/*
 * Request/Response functions
 */

res_t ik_openai_request_create(void *parent, const ik_cfg_t *cfg,
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
    req->max_tokens = cfg->openai_max_tokens;
    req->stream = true;  /* Always enable streaming */

    return OK(req);
}

res_t ik_openai_response_create(void *parent) {
    ik_openai_response_t *resp = talloc_zero(parent, ik_openai_response_t);
    if (!resp) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate response"); // LCOV_EXCL_LINE
    }

    resp->content = NULL;
    resp->finish_reason = NULL;
    resp->prompt_tokens = 0;
    resp->completion_tokens = 0;
    resp->total_tokens = 0;

    return OK(resp);
}

/*
 * JSON serialization
 */

res_t ik_openai_serialize_request(void *parent, const ik_openai_request_t *request) {
    assert(request != NULL); // LCOV_EXCL_BR_LINE
    assert(request->conv != NULL); // LCOV_EXCL_BR_LINE

    /* Create yyjson document with talloc allocator */
    yyjson_alc alc = ik_make_talloc_allocator(parent);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
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

    /* Add max_tokens field */
    if (!yyjson_mut_obj_add_int(doc, root, "max_tokens", (int64_t)request->max_tokens)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add max_tokens field to JSON"); // LCOV_EXCL_LINE
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

    /* Free yyjson-allocated string */
    free(json_str);

    return OK(result);
}

/*
 * SSE parser
 */

#define SSE_INITIAL_BUFFER_SIZE 4096

res_t ik_openai_sse_parser_create(void *parent) {
    ik_openai_sse_parser_t *parser = talloc_zero(parent, ik_openai_sse_parser_t);
    if (!parser) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE parser"); // LCOV_EXCL_LINE
    }

    /* Allocate initial buffer */
    parser->buffer = talloc_array(parser, char, SSE_INITIAL_BUFFER_SIZE);
    if (!parser->buffer) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE parser buffer"); // LCOV_EXCL_LINE
    }

    parser->buffer[0] = '\0';
    parser->buffer_len = 0;
    parser->buffer_cap = SSE_INITIAL_BUFFER_SIZE - 1; /* Reserve space for null terminator */

    return OK(parser);
}

res_t ik_openai_sse_parser_feed(ik_openai_sse_parser_t *parser,
                                  const char *data, size_t len) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL || len == 0); // LCOV_EXCL_BR_LINE

    if (len == 0) {
        return OK(NULL);
    }

    /* Check if we need to grow the buffer */
    if (parser->buffer_len + len > parser->buffer_cap) {
        /* Grow buffer to accommodate new data (double capacity or fit data, whichever is larger) */
        size_t new_cap = parser->buffer_cap * 2;
        while (new_cap < parser->buffer_len + len) {
            new_cap *= 2;
        }

        size_t alloc_size = new_cap + 1;
        char *new_buffer = talloc_realloc(parser, parser->buffer, char, (unsigned int)alloc_size);
        if (!new_buffer) { // LCOV_EXCL_BR_LINE
            PANIC("Failed to grow SSE parser buffer"); // LCOV_EXCL_LINE
        }

        parser->buffer = new_buffer;
        parser->buffer_cap = new_cap;
    }

    /* Append data to buffer */
    memcpy(parser->buffer + parser->buffer_len, data, len);
    parser->buffer_len += len;
    parser->buffer[parser->buffer_len] = '\0';

    return OK(NULL);
}

res_t ik_openai_sse_parser_get_event(ik_openai_sse_parser_t *parser) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE

    /* Look for \n\n delimiter */
    const char *delimiter = strstr(parser->buffer, "\n\n");
    if (!delimiter) {
        /* No complete event yet */
        return OK(NULL);
    }

    /* Calculate event length (excluding the \n\n) */
    size_t event_len = (size_t)(delimiter - parser->buffer);

    /* Extract event string */
    char *event = talloc_strndup(parser, parser->buffer, event_len);
    if (!event) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate event string"); // LCOV_EXCL_LINE
    }

    /* Remove event and delimiter from buffer */
    size_t consumed = event_len + 2; /* +2 for \n\n */
    size_t remaining = parser->buffer_len - consumed;

    if (remaining > 0) {
        /* Move remaining data to start of buffer */
        memmove(parser->buffer, parser->buffer + consumed, remaining);
    }

    parser->buffer_len = remaining;
    parser->buffer[parser->buffer_len] = '\0';

    return OK(event);
}

res_t ik_openai_parse_sse_event(void *parent, const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) {
        return ERR(parent, PARSE, "SSE event missing 'data: ' prefix");
    }

    /* Get JSON payload (after "data: ") */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        /* End of stream */
        return OK(NULL);
    }

    /* Parse JSON */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) {
        return ERR(parent, PARSE, "Failed to parse SSE event JSON");
    }

    yyjson_val *root = yyjson_doc_get_root_wrapper(doc);
    if (!yyjson_is_obj_wrapper(root)) {
        yyjson_doc_free(doc);
        return ERR(parent, PARSE, "SSE event JSON root is not an object");
    }

    /* Extract choices[0].delta.content */
    yyjson_val *choices = yyjson_obj_get(root, "choices");
    if (!choices || !yyjson_is_arr(choices) || yyjson_arr_size(choices) == 0) {
        /* No choices array or empty - no content */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *choice0 = yyjson_arr_get_wrapper(choices, 0);
    if (!choice0 || !yyjson_is_obj_wrapper(choice0)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *delta = yyjson_obj_get(choice0, "delta");
    if (!delta || !yyjson_is_obj_wrapper(delta)) {
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *content = yyjson_obj_get(delta, "content");
    if (!content || !yyjson_is_str(content)) {
        /* No content field or not a string - may be role or other delta */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    /* Extract content string */
    const char *content_str = yyjson_get_str(content);
    char *result = talloc_strdup(parent, content_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate content string"); // LCOV_EXCL_LINE
    }

    yyjson_doc_free(doc);
    return OK(result);
}

/*
 * HTTP client
 */

// LCOV_EXCL_START - Stub for future implementation (Tasks 5.9-5.11)
res_t ik_openai_chat_create(void *parent, const ik_cfg_t *cfg,
                             ik_openai_conversation_t *conv,
                             ik_openai_stream_cb_t stream_cb, void *cb_ctx) {
    assert(cfg != NULL); // LCOV_EXCL_BR_LINE
    assert(conv != NULL); // LCOV_EXCL_BR_LINE

    /* TODO: Implement HTTP client in Tasks 5.9-5.11 */
    (void)parent;
    (void)stream_cb;
    (void)cb_ctx;
    return OK(NULL);
}
// LCOV_EXCL_STOP
