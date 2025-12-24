#include "providers/openai/shim.h"
#include "db/message.h"
#include "msg.h"
#include "panic.h"
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

/* Struct definitions (duplicated here to avoid header conflicts) */
struct ik_openai_conversation {
    ik_msg_t **messages;      /* Array of message pointers */
    size_t message_count;     /* Number of messages */
};

struct ik_openai_request {
    char *model;                      /* Model identifier */
    ik_openai_conversation_t *conv;   /* Conversation messages */
    double temperature;               /* Randomness (0.0-2.0) */
    int32_t max_completion_tokens;    /* Maximum response tokens */
    bool stream;                      /* Enable streaming responses */
};

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

static res_t openai_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx;
    (void)req;
    (void)completion_cb;
    (void)completion_ctx;

    /* LCOV_EXCL_START */
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_request not yet implemented");
    talloc_free(tmp);
    return result;
    /* LCOV_EXCL_STOP */
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
    if (api_key == NULL) {
        return ERR(ctx, MISSING_CREDENTIALS, "OpenAI API key is NULL");
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
