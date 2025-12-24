#include "providers/openai/shim.h"
#include "config.h"
#include "db/message.h"
#include "msg.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>

/*
 * External declarations to avoid header conflicts
 *
 * We cannot include openai/client.h because it includes openai/tool_choice.h
 * which conflicts with provider.h's ik_tool_choice_t enum.
 * Instead, we forward declare the functions we need.
 */

/* Multi-handle functions */
extern res_t ik_openai_multi_create(void *parent);
extern res_t ik_openai_multi_fdset(ik_openai_multi_t *multi, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd);
extern res_t ik_openai_multi_perform(ik_openai_multi_t *multi, int *still_running);
extern res_t ik_openai_multi_timeout(ik_openai_multi_t *multi, long *timeout_ms);
extern void ik_openai_multi_info_read(ik_openai_multi_t *multi, ik_logger_t *logger);

/* Message creation functions */
extern ik_msg_t *ik_openai_msg_create(TALLOC_CTX *ctx, const char *role, const char *content);
extern ik_msg_t *ik_openai_msg_create_tool_call(void *parent, const char *id, const char *type,
                                                 const char *name, const char *arguments, const char *content);
extern ik_msg_t *ik_openai_msg_create_tool_result(void *parent, const char *tool_call_id, const char *content);

/* Conversation functions */
extern ik_openai_conversation_t *ik_openai_conversation_create(TALLOC_CTX *ctx);
extern res_t ik_openai_conversation_add_msg(ik_openai_conversation_t *conv, ik_msg_t *msg);

/* Type definitions needed for multi_add_request */
typedef res_t (*ik_openai_stream_cb_t)(const char *chunk, void *ctx);

/* Forward declare http completion types from openai/client_multi.h */
typedef struct ik_http_completion ik_http_completion_t;
typedef res_t (*ik_http_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);

/* Forward declare tool_call type from tool.h */
typedef struct {
    char *id;
    char *name;
    char *arguments;
} ik_tool_call_t;

/* HTTP completion structure from openai/client_multi.h */
typedef enum {
    IK_HTTP_SUCCESS = 0,
    IK_HTTP_CLIENT_ERROR = 1,
    IK_HTTP_SERVER_ERROR = 2,
    IK_HTTP_NETWORK_ERROR = 3,
} ik_http_status_type_t;

struct ik_http_completion {
    ik_http_status_type_t type;
    int32_t http_code;
    int32_t curl_code;
    char *error_message;
    char *model;
    char *finish_reason;
    int32_t completion_tokens;
    ik_tool_call_t *tool_call;
};

/* Struct definition for ik_openai_request_t from openai/client.h */
struct ik_openai_request {
    char *model;
    ik_openai_conversation_t *conv;
    double temperature;
    int32_t max_completion_tokens;
    bool stream;
};

/* Multi-handle add_request function (needs types defined above) */
extern res_t ik_openai_multi_add_request(ik_openai_multi_t *multi,
                                          const ik_config_t *cfg,
                                          ik_openai_conversation_t *conv,
                                          ik_openai_stream_cb_t stream_cb,
                                          void *stream_ctx,
                                          ik_http_completion_cb_t completion_cb,
                                          void *completion_ctx,
                                          bool limit_reached,
                                          ik_logger_t *logger);

/* ================================================================
 * Request Transformation Functions
 *
 * These functions convert from the normalized provider format
 * (ik_request_t, ik_message_t) to the legacy OpenAI client format
 * (ik_openai_request_t, ik_msg_t).
 * ================================================================ */

res_t ik_openai_shim_transform_message(TALLOC_CTX *ctx, const ik_message_t *msg, ik_msg_t **out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);  // LCOV_EXCL_BR_LINE

    /* Validate message has content */
    if (msg->content_count == 0) {
        return ERR(ctx, INVALID_ARG, "Message has no content blocks");
    }

    /* We only handle single content block messages for now */
    const ik_content_block_t *block = &msg->content_blocks[0];

    ik_msg_t *result = NULL;

    /* Transform based on content type */
    switch (block->type) {
        case IK_CONTENT_TEXT: {
            /* Simple text message - map role to kind */
            const char *kind = NULL;
            switch (msg->role) {
                case IK_ROLE_USER:
                    kind = "user";
                    break;
                case IK_ROLE_ASSISTANT:
                    kind = "assistant";
                    break;
                case IK_ROLE_TOOL:
                    kind = "system";  /* System prompt */
                    break;
                default:
                    return ERR(ctx, INVALID_ARG, "Unsupported message role: %d", msg->role);
            }
            result = ik_openai_msg_create(ctx, kind, block->data.text.text);
            break;
        }

        case IK_CONTENT_TOOL_CALL: {
            /* Tool call - use existing helper */
            result = ik_openai_msg_create_tool_call(
                ctx,
                block->data.tool_call.id,
                "function",  /* type is always "function" */
                block->data.tool_call.name,
                block->data.tool_call.arguments,
                block->data.tool_call.arguments  /* Use arguments as content summary */
            );
            break;
        }

        case IK_CONTENT_TOOL_RESULT: {
            /* Tool result - use existing helper */
            result = ik_openai_msg_create_tool_result(
                ctx,
                block->data.tool_result.tool_call_id,
                block->data.tool_result.content
            );
            break;
        }

        case IK_CONTENT_THINKING: {
            /* Thinking blocks are not sent to OpenAI - skip */
            return ERR(ctx, INVALID_ARG, "Thinking blocks not supported in OpenAI requests");
        }

        default:
            return ERR(ctx, INVALID_ARG, "Unsupported content block type: %d", block->type);
    }

    if (result == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    *out = result;
    return OK(result);
}

res_t ik_openai_shim_build_conversation(TALLOC_CTX *ctx, const ik_request_t *req, ik_openai_conversation_t **out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);  // LCOV_EXCL_BR_LINE

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);
    if (conv == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Add system prompt as first message if present */
    if (req->system_prompt != NULL && req->system_prompt[0] != '\0') {
        ik_msg_t *sys_msg = ik_openai_msg_create(conv, "system", req->system_prompt);
        res_t add_res = ik_openai_conversation_add_msg(conv, sys_msg);
        if (is_err(&add_res)) {  // LCOV_EXCL_BR_LINE
            return add_res;  // LCOV_EXCL_LINE
        }
    }

    /* Validate we have messages */
    if (req->message_count == 0) {
        return ERR(ctx, INVALID_ARG, "Request has no messages");
    }

    /* Transform each message */
    for (size_t i = 0; i < req->message_count; i++) {
        ik_msg_t *legacy_msg = NULL;
        res_t transform_res = ik_openai_shim_transform_message(conv, &req->messages[i], &legacy_msg);
        if (is_err(&transform_res)) {
            /* Reparent error before cleanup */
            talloc_steal(ctx, transform_res.err);
            talloc_free(conv);
            return transform_res;
        }

        res_t add_res = ik_openai_conversation_add_msg(conv, legacy_msg);
        if (is_err(&add_res)) {  // LCOV_EXCL_BR_LINE
            talloc_steal(ctx, add_res.err);  // LCOV_EXCL_LINE
            talloc_free(conv);  // LCOV_EXCL_LINE
            return add_res;  // LCOV_EXCL_LINE
        }
    }

    *out = conv;
    return OK(conv);
}

res_t ik_openai_shim_transform_request(TALLOC_CTX *ctx, const ik_request_t *req, ik_openai_request_t **out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);  // LCOV_EXCL_BR_LINE

    /* Build conversation from messages */
    ik_openai_conversation_t *conv = NULL;
    res_t conv_res = ik_openai_shim_build_conversation(ctx, req, &conv);
    if (is_err(&conv_res)) {
        return conv_res;
    }

    /* Create legacy request */
    ik_openai_request_t *legacy_req = talloc_zero(ctx, ik_openai_request_t);
    if (legacy_req == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Copy model */
    legacy_req->model = talloc_strdup(legacy_req, req->model);
    if (legacy_req->model == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Attach conversation */
    talloc_steal(legacy_req, conv);
    legacy_req->conv = conv;

    /* Set temperature (default 0.7) */
    legacy_req->temperature = 0.7;

    /* Set max_completion_tokens */
    legacy_req->max_completion_tokens = req->max_output_tokens;

    /* Enable streaming by default */
    legacy_req->stream = true;

    *out = legacy_req;
    return OK(legacy_req);
}

/* ================================================================
 * Response Transformation Functions
 *
 * These functions convert from the legacy OpenAI client format
 * (ik_msg_t) to the normalized provider format (ik_response_t).
 * ================================================================ */

ik_finish_reason_t ik_openai_shim_map_finish_reason(const char *openai_reason)
{
    if (openai_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(openai_reason, "stop") == 0) {
        return IK_FINISH_STOP;
    }
    if (strcmp(openai_reason, "length") == 0) {
        return IK_FINISH_LENGTH;
    }
    if (strcmp(openai_reason, "tool_calls") == 0) {
        return IK_FINISH_TOOL_USE;
    }
    if (strcmp(openai_reason, "content_filter") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    }

    return IK_FINISH_UNKNOWN;
}

res_t ik_openai_shim_transform_response(TALLOC_CTX *ctx, const ik_msg_t *msg, ik_response_t **out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);  // LCOV_EXCL_BR_LINE

    /* Allocate response structure */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    if (response == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Allocate single content block */
    response->content_blocks = talloc_zero(response, ik_content_block_t);
    if (response->content_blocks == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    response->content_count = 1;

    ik_content_block_t *block = &response->content_blocks[0];

    /* Transform based on message kind */
    if (strcmp(msg->kind, "assistant") == 0) {
        /* Text response */
        block->type = IK_CONTENT_TEXT;
        block->data.text.text = talloc_strdup(block, msg->content);
        if (block->data.text.text == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        /* Default finish reason for text responses */
        response->finish_reason = IK_FINISH_STOP;

    } else if (strcmp(msg->kind, "tool_call") == 0) {
        /* Tool call response - parse data_json */
        if (msg->data_json == NULL) {
            talloc_free(response);
            return ERR(ctx, PARSE, "tool_call message has NULL data_json");
        }

        /* Parse data_json to extract tool call fields */
        yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
        if (doc == NULL) {
            talloc_free(response);
            return ERR(ctx, PARSE, "Failed to parse tool_call data_json");
        }

        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            talloc_free(response);
            return ERR(ctx, PARSE, "tool_call data_json is not an object");
        }

        /* Extract fields - handle nested function object structure */
        yyjson_val *id_val = yyjson_obj_get(root, "id");
        if (!yyjson_is_str(id_val)) {
            yyjson_doc_free(doc);
            talloc_free(response);
            return ERR(ctx, PARSE, "tool_call data_json missing id field");
        }

        /* Check for nested function object (created by ik_openai_msg_create_tool_call) */
        yyjson_val *function_obj = yyjson_obj_get(root, "function");
        yyjson_val *name_val = NULL;
        yyjson_val *args_val = NULL;

        if (function_obj != NULL && yyjson_is_obj(function_obj)) {
            /* Nested structure: {"id": "...", "function": {"name": "...", "arguments": "..."}} */
            name_val = yyjson_obj_get(function_obj, "name");
            args_val = yyjson_obj_get(function_obj, "arguments");
        } else {
            /* Flat structure: {"id": "...", "name": "...", "arguments": "..."} */
            name_val = yyjson_obj_get(root, "name");
            args_val = yyjson_obj_get(root, "arguments");
        }

        if (!yyjson_is_str(name_val) || !yyjson_is_str(args_val)) {
            yyjson_doc_free(doc);
            talloc_free(response);
            return ERR(ctx, PARSE, "tool_call data_json missing required string fields");
        }

        /* Set content block type and data */
        block->type = IK_CONTENT_TOOL_CALL;

        block->data.tool_call.id = talloc_strdup(block, yyjson_get_str(id_val));
        if (block->data.tool_call.id == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        block->data.tool_call.name = talloc_strdup(block, yyjson_get_str(name_val));
        if (block->data.tool_call.name == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        block->data.tool_call.arguments = talloc_strdup(block, yyjson_get_str(args_val));
        if (block->data.tool_call.arguments == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        yyjson_doc_free(doc);

        /* Finish reason for tool calls */
        response->finish_reason = IK_FINISH_TOOL_USE;

    } else {
        /* Unknown kind - treat as text with empty content */
        block->type = IK_CONTENT_TEXT;
        block->data.text.text = talloc_strdup(block, "");
        if (block->data.text.text == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        response->finish_reason = IK_FINISH_UNKNOWN;
    }

    /* Set usage statistics - currently limited by what's available */
    response->usage.input_tokens = 0;      /* Not available in current implementation */
    response->usage.output_tokens = 0;     /* Not available in current implementation */
    response->usage.thinking_tokens = 0;   /* OpenAI doesn't expose for Chat Completions */
    response->usage.cached_tokens = 0;     /* Not available */
    response->usage.total_tokens = 0;

    /* Model is not available in msg structure */
    response->model = NULL;

    /* No provider-specific data */
    response->provider_data = NULL;

    *out = response;
    return OK(response);
}

/* ================================================================
 * Vtable Methods - Event Loop Integration
 *
 * These methods forward to the existing OpenAI multi-handle implementation.
 * ================================================================ */

static res_t openai_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    return ik_openai_multi_fdset(shim->multi, read_fds, write_fds, exc_fds, max_fd);
}

static res_t openai_perform(void *ctx, int *running_handles)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    return ik_openai_multi_perform(shim->multi, running_handles);
}

static res_t openai_timeout(void *ctx, long *timeout_ms)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    return ik_openai_multi_timeout(shim->multi, timeout_ms);
}

static void openai_info_read(void *ctx, ik_logger_t *logger)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    ik_openai_multi_info_read(shim->multi, logger);
}

/* ================================================================
 * Helper: Build temporary config for existing client
 * ================================================================ */

/**
 * Build temporary ik_config_t for existing OpenAI client
 *
 * The legacy client expects ik_config_t with specific fields.
 * We build a minimal config from the request and shim context.
 */
static res_t build_temp_config(TALLOC_CTX *ctx, ik_openai_shim_ctx_t *shim,
                                const ik_request_t *req, ik_config_t **out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(shim != NULL);  // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);  // LCOV_EXCL_BR_LINE

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    if (cfg == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Model from request */
    cfg->openai_model = talloc_strdup(cfg, req->model);
    if (cfg->openai_model == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Temperature - default 0.7 */
    cfg->openai_temperature = 0.7;

    /* Max completion tokens */
    cfg->openai_max_completion_tokens = req->max_output_tokens;

    /* System message is passed separately in conversation */
    cfg->openai_system_message = NULL;

    *out = cfg;
    return OK(cfg);
}

/* ================================================================
 * Completion Callback Wrapper
 * ================================================================ */

/**
 * Wrapper context for completion callback
 *
 * Captures user callback and context for response transformation.
 */
typedef struct {
    ik_provider_completion_cb_t user_cb;  /* User's completion callback */
    void *user_ctx;                       /* User's callback context */
} completion_wrapper_ctx_t;

/**
 * Completion callback wrapper
 *
 * Transforms legacy HTTP completion to provider completion format
 * before invoking user callback.
 */
static res_t completion_wrapper(const ik_http_completion_t *http_completion, void *ctx)
{
    assert(http_completion != NULL);  // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    completion_wrapper_ctx_t *wrapper = (completion_wrapper_ctx_t *)ctx;

    /* Allocate provider completion on wrapper context */
    ik_provider_completion_t *provider_completion = talloc_zero(wrapper, ik_provider_completion_t);
    if (provider_completion == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Check if HTTP request succeeded */
    if (http_completion->type == IK_HTTP_SUCCESS) {
        /* Success - need to build response from tool_call or text */
        provider_completion->success = true;
        provider_completion->http_status = http_completion->http_code;
        provider_completion->error_category = IK_ERR_CAT_UNKNOWN;
        provider_completion->error_message = NULL;
        provider_completion->retry_after_ms = -1;

        /* Build ik_msg_t from completion data */
        ik_msg_t *msg = NULL;
        if (http_completion->tool_call != NULL) {
            /* Tool call response */
            msg = ik_openai_msg_create_tool_call(
                provider_completion,
                http_completion->tool_call->id,
                "function",
                http_completion->tool_call->name,
                http_completion->tool_call->arguments,
                http_completion->tool_call->arguments
            );
        } else {
            /* Text response - model should have sent content but we don't have it in completion */
            /* For now, create empty assistant message */
            msg = ik_openai_msg_create(provider_completion, "assistant", "");
        }

        /* Transform legacy message to normalized response */
        ik_response_t *response = NULL;
        res_t transform_res = ik_openai_shim_transform_response(provider_completion, msg, &response);
        if (is_err(&transform_res)) {
            /* Transform error - deliver as error completion */
            provider_completion->success = false;
            provider_completion->response = NULL;
            provider_completion->error_category = IK_ERR_CAT_UNKNOWN;
            provider_completion->error_message = talloc_asprintf(provider_completion,
                "Response transformation failed: %s", transform_res.err->msg);
            provider_completion->retry_after_ms = -1;

            /* Free transform error */
            talloc_free(transform_res.err);
        } else {
            provider_completion->response = response;
        }

    } else {
        /* HTTP error - map to provider error */
        provider_completion->success = false;
        provider_completion->http_status = http_completion->http_code;
        provider_completion->response = NULL;
        provider_completion->retry_after_ms = -1;

        /* Map HTTP error type to error category */
        switch (http_completion->type) {
            case IK_HTTP_CLIENT_ERROR:
                if (http_completion->http_code == 401 || http_completion->http_code == 403) {
                    provider_completion->error_category = IK_ERR_CAT_AUTH;
                } else if (http_completion->http_code == 429) {
                    provider_completion->error_category = IK_ERR_CAT_RATE_LIMIT;
                } else if (http_completion->http_code == 404) {
                    provider_completion->error_category = IK_ERR_CAT_NOT_FOUND;
                } else {
                    provider_completion->error_category = IK_ERR_CAT_INVALID_ARG;
                }
                break;

            case IK_HTTP_SERVER_ERROR:
                provider_completion->error_category = IK_ERR_CAT_SERVER;
                break;

            case IK_HTTP_NETWORK_ERROR:
                provider_completion->error_category = IK_ERR_CAT_NETWORK;
                break;

            default:
                provider_completion->error_category = IK_ERR_CAT_UNKNOWN;
                break;
        }

        /* Copy error message */
        if (http_completion->error_message != NULL) {
            provider_completion->error_message = talloc_strdup(provider_completion,
                http_completion->error_message);
            if (provider_completion->error_message == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
        } else {
            provider_completion->error_message = NULL;
        }
    }

    /* Invoke user callback */
    res_t cb_result = wrapper->user_cb(provider_completion, wrapper->user_ctx);

    /* User callback is responsible for stealing response if needed */
    /* We can now free the wrapper context which will free provider_completion */

    return cb_result;
}

/* ================================================================
 * Vtable Methods - Request Initiation
 * ================================================================ */

static res_t openai_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL);  // LCOV_EXCL_BR_LINE

    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;

    /* Validate API key */
    if (shim->api_key == NULL || shim->api_key[0] == '\0') {
        return ERR(shim, MISSING_CREDENTIALS, "OpenAI API key is not set");
    }

    /* Validate messages */
    if (req->message_count == 0) {
        return ERR(shim, INVALID_ARG, "Request has no messages");
    }

    /* Transform request to legacy format */
    ik_openai_request_t *legacy_req = NULL;
    res_t transform_res = ik_openai_shim_transform_request(shim, req, &legacy_req);
    if (is_err(&transform_res)) {
        return transform_res;
    }

    /* Build temporary config */
    ik_config_t *cfg = NULL;
    res_t cfg_res = build_temp_config(shim, shim, req, &cfg);
    if (is_err(&cfg_res)) {
        talloc_free(legacy_req);
        return cfg_res;
    }

    /* Create wrapper context for completion callback */
    completion_wrapper_ctx_t *wrapper = talloc_zero(shim->multi, completion_wrapper_ctx_t);
    if (wrapper == NULL) {  // LCOV_EXCL_BR_LINE
        talloc_free(legacy_req);  // LCOV_EXCL_LINE
        talloc_free(cfg);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    wrapper->user_cb = completion_cb;
    wrapper->user_ctx = completion_ctx;

    /* Add request to multi-handle (non-blocking) */
    res_t add_res = ik_openai_multi_add_request(
        shim->multi,
        cfg,
        legacy_req->conv,
        NULL,  /* stream_cb - not used for non-streaming */
        NULL,  /* stream_ctx */
        completion_wrapper,
        wrapper,
        false,  /* limit_reached - use tool_choice auto */
        NULL    /* logger */
    );

    /* Clean up temporary allocations */
    talloc_free(legacy_req);
    talloc_free(cfg);

    if (is_err(&add_res)) {
        talloc_free(wrapper);
        return add_res;
    }

    /* Return immediately - request will progress through perform/info_read cycle */
    return OK(NULL);
}

static res_t openai_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    /* LCOV_EXCL_START */
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_stream not yet implemented");
    talloc_free(tmp);
    return result;
    /* LCOV_EXCL_STOP */
}

static void openai_cleanup(void *ctx)
{
    (void)ctx;

    /* LCOV_EXCL_START */
    /* Stub: talloc hierarchy handles cleanup */
    /* LCOV_EXCL_STOP */
}

static void openai_cancel(void *ctx)
{
    (void)ctx;

    /* LCOV_EXCL_START */
    /* Stub: cancellation not yet implemented */
    /* LCOV_EXCL_STOP */
}

/* ================================================================
 * OpenAI Provider Vtable
 * ================================================================ */

static const ik_provider_vtable_t openai_vtable = {
    .fdset = openai_fdset,
    .perform = openai_perform,
    .timeout = openai_timeout,
    .info_read = openai_info_read,
    .start_request = openai_start_request,
    .start_stream = openai_start_stream,
    .cleanup = openai_cleanup,
    .cancel = openai_cancel,
};

/* ================================================================
 * Public API Implementation
 * ================================================================ */

res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    /* Validate API key */
    if (api_key == NULL || api_key[0] == '\0') {
        return ERR(ctx, MISSING_CREDENTIALS, "OpenAI API key is NULL or empty");
    }

    /* Allocate provider */
    ik_provider_t *provider = talloc_zero(ctx, ik_provider_t);
    if (!provider) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Allocate shim context as child of provider */
    ik_openai_shim_ctx_t *shim = talloc_zero(provider, ik_openai_shim_ctx_t);
    if (!shim) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Duplicate API key as child of shim context */
    shim->api_key = talloc_strdup(shim, api_key);
    if (!shim->api_key) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Create multi-handle for async HTTP */
    res_t multi_res = ik_openai_multi_create(shim);
    if (is_err(&multi_res)) {
        /* Error is on shim context, need to reparent before cleanup */
        talloc_steal(ctx, multi_res.err);
        talloc_free(provider);
        return multi_res;
    }
    shim->multi = multi_res.ok;

    /* Initialize provider with vtable */
    provider->name = "openai";
    provider->vt = &openai_vtable;
    provider->ctx = shim;

    *out = provider;
    return OK(provider);
}

void ik_openai_shim_destroy(void *impl_ctx)
{
    /* NULL-safe: no-op if context is NULL */
    if (impl_ctx == NULL) {
        return;
    }

    /* All cleanup handled by talloc hierarchy */
    /* This function exists for API symmetry and future needs */
}
