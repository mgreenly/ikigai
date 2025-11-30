#include "openai/client.h"

#include "error.h"
#include "json_allocator.h"
#include "openai/http_handler.h"
#include "openai/tool_choice.h"
#include "tool.h"
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

char *ik_openai_serialize_request(void *parent, const ik_openai_request_t *request, ik_tool_choice_t tool_choice) {
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

        /* Check if this is a tool_call message */
        if (strcmp(msg->role, "tool_call") == 0) {
            /* Transform canonical tool_call to OpenAI wire format */
            /* OpenAI expects role="assistant" with tool_calls array */
            if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", "assistant")) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add role field to message"); // LCOV_EXCL_LINE
            }

            /* Ensure data_json exists */
            if (!msg->data_json) PANIC("tool_call message missing data_json"); // LCOV_EXCL_BR_LINE

            /* Parse data_json to extract tool call details */
            /* Use a temporary talloc context for copying strings */
            TALLOC_CTX *tmp_ctx = talloc_new(NULL);
            if (!tmp_ctx) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            yyjson_doc *data_doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
            if (!data_doc) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to parse tool call data_json"); // LCOV_EXCL_LINE
            }

            yyjson_val *data_root = yyjson_doc_get_root(data_doc);
            if (!data_root || !yyjson_is_obj(data_root)) { // LCOV_EXCL_BR_LINE
                yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Invalid tool call data_json structure"); // LCOV_EXCL_LINE
            }

            /* Extract tool call fields and copy to talloc strings */
            const char *call_id_src = yyjson_get_str(yyjson_obj_get(data_root, "id")); // LCOV_EXCL_BR_LINE
            const char *call_type_src = yyjson_get_str(yyjson_obj_get(data_root, "type")); // LCOV_EXCL_BR_LINE
            yyjson_val *function_val = yyjson_obj_get(data_root, "function");

            if (!call_id_src || !call_type_src || !function_val || !yyjson_is_obj(function_val)) { // LCOV_EXCL_BR_LINE
                yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Missing required fields in tool call data_json"); // LCOV_EXCL_LINE
            }

            const char *func_name_src = yyjson_get_str(yyjson_obj_get(function_val, "name")); // LCOV_EXCL_BR_LINE
            const char *func_args_src = yyjson_get_str(yyjson_obj_get(function_val, "arguments")); // LCOV_EXCL_BR_LINE

            if (!func_name_src || !func_args_src) { // LCOV_EXCL_BR_LINE
                yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Missing function fields in tool call data_json"); // LCOV_EXCL_LINE
            }

            /* Copy strings to tmp_ctx so they survive yyjson_doc_free */
            char *call_id = talloc_strdup(tmp_ctx, call_id_src);
            char *call_type = talloc_strdup(tmp_ctx, call_type_src);
            char *func_name = talloc_strdup(tmp_ctx, func_name_src);
            char *func_args = talloc_strdup(tmp_ctx, func_args_src);

            /* Free data_doc now that we've copied the strings */
            yyjson_doc_free(data_doc);

            if (!call_id || !call_type || !func_name || !func_args) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Out of memory copying tool call strings"); // LCOV_EXCL_LINE
            }

            /* Create tool_calls array */
            yyjson_mut_val *tool_calls_arr = yyjson_mut_arr(doc);
            if (tool_calls_arr == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }

            /* Create tool call object */
            yyjson_mut_val *tool_call_obj = yyjson_mut_arr_add_obj(doc, tool_calls_arr);
            if (tool_call_obj == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }

            /* Add tool call fields */
            if (!yyjson_mut_obj_add_str(doc, tool_call_obj, "id", call_id)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to add id to tool call"); // LCOV_EXCL_LINE
            }
            if (!yyjson_mut_obj_add_str(doc, tool_call_obj, "type", call_type)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to add type to tool call"); // LCOV_EXCL_LINE
            }

            /* Create function object in tool call */
            yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
            if (func_obj == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }

            if (!yyjson_mut_obj_add_str(doc, func_obj, "name", func_name)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to add function name"); // LCOV_EXCL_LINE
            }
            if (!yyjson_mut_obj_add_str(doc, func_obj, "arguments", func_args)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to add function arguments"); // LCOV_EXCL_LINE
            }

            if (!yyjson_mut_obj_add_val(doc, tool_call_obj, "function", func_obj)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to add function to tool call"); // LCOV_EXCL_LINE
            }

            /* Add tool_calls array to message */
            if (!yyjson_mut_obj_add_val(doc, msg_obj, "tool_calls", tool_calls_arr)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp_ctx); // LCOV_EXCL_LINE
                PANIC("Failed to add tool_calls array to message"); // LCOV_EXCL_LINE
            }

            /* Clean up tmp_ctx (strings are now copied into doc) */
            talloc_free(tmp_ctx);
        } else {
            /* Regular text message */
            if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", msg->role)) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add role field to message"); // LCOV_EXCL_LINE
            }
            if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", msg->content)) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add content field to message"); // LCOV_EXCL_LINE
            }
        }
    }

    /* Add messages array to root */
    if (!yyjson_mut_obj_add_val(doc, root, "messages", messages_arr)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add messages array to JSON"); // LCOV_EXCL_LINE
    }

    /* Build and add tools array */
    yyjson_mut_val *tools_arr = ik_tool_build_all(doc);
    if (tools_arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add tools array to JSON"); // LCOV_EXCL_LINE
    }

    /* Add tool_choice field using serializer */
    ik_tool_choice_serialize(doc, root, "tool_choice", tool_choice);

    /* Add stream field */
    if (!yyjson_mut_obj_add_bool(doc, root, "stream", request->stream)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add stream field to JSON"); // LCOV_EXCL_LINE
    }

    /* Add temperature field */
    if (!yyjson_mut_obj_add_real(doc, root, "temperature", request->temperature)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add temperature field to JSON"); // LCOV_EXCL_LINE
    }

    /* Add max_completion_tokens field */
    if (!yyjson_mut_obj_add_int(doc, root, "max_completion_tokens", (int64_t)request->max_completion_tokens)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add max_completion_tokens field to JSON"); // LCOV_EXCL_LINE
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

    /* Serialize request to JSON with auto tool_choice */
    ik_tool_choice_t tool_choice = ik_tool_choice_auto();
    char *json_body = ik_openai_serialize_request(parent, request, tool_choice);

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
