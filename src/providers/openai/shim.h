#ifndef IK_PROVIDERS_OPENAI_SHIM_H
#define IK_PROVIDERS_OPENAI_SHIM_H

#include "error.h"
#include "providers/provider.h"
#include "msg.h"
#include <talloc.h>

/* Forward declarations to avoid including openai/client.h fully */
typedef struct ik_openai_multi ik_openai_multi_t;
typedef struct ik_openai_request ik_openai_request_t;
typedef struct ik_openai_conversation ik_openai_conversation_t;

/**
 * OpenAI Provider Shim
 *
 * This module adapts the existing OpenAI client (src/openai/) to the
 * new unified provider interface (src/providers/provider.h).
 *
 * The shim context holds OpenAI-specific state needed to bridge between
 * the provider vtable and the existing OpenAI implementation.
 */

/**
 * OpenAI shim context
 *
 * Holds OpenAI-specific state for the provider vtable callbacks.
 * This context is the bridge between the generic provider interface
 * and the existing OpenAI client code.
 *
 * Memory ownership:
 * - Allocated as child of the provider struct
 * - api_key is child of this context
 * - multi is child of this context
 * - Single talloc_free on provider releases everything
 */
typedef struct {
    char *api_key;              /* OpenAI API key (owned) */
    ik_openai_multi_t *multi;   /* Multi-handle for async HTTP */
} ik_openai_shim_ctx_t;

/**
 * Create an OpenAI provider instance
 *
 * Factory function that creates a new provider with the OpenAI vtable.
 * The provider uses the existing OpenAI client code via the shim layer.
 *
 * Memory ownership:
 * - Provider allocated on ctx
 * - Shim context allocated on provider
 * - API key duplicated and owned by shim context
 *
 * @param ctx     Talloc context for allocation (parent of provider)
 * @param api_key OpenAI API key (must not be NULL)
 * @param out     Output: created provider (NULL on error)
 * @return        OK(provider) on success, ERR(...) on failure
 *
 * Error cases:
 * - ERR_MISSING_CREDENTIALS if api_key is NULL
 * - ERR_OUT_OF_MEMORY on allocation failure (via PANIC)
 * - ERR_NOT_IMPLEMENTED for stub vtable methods
 */
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

/**
 * Destroy OpenAI shim context
 *
 * Cleanup function for the shim context. This is called from the
 * provider vtable cleanup method.
 *
 * NULL-safe: calling with NULL context is a no-op.
 *
 * Memory: All cleanup is handled by talloc hierarchy. This function
 * exists to provide a symmetrical API and for future cleanup needs.
 *
 * @param impl_ctx Shim context to destroy (may be NULL)
 */
void ik_openai_shim_destroy(void *impl_ctx);

/**
 * Transform a single normalized message to legacy ik_msg_t format
 *
 * Converts from ik_message_t (normalized provider format) to ik_msg_t
 * (legacy OpenAI client format).
 *
 * Transformation rules:
 * - IK_ROLE_USER -> kind="user"
 * - IK_ROLE_ASSISTANT -> kind="assistant"
 * - IK_CONTENT_TEXT -> content=text, data_json=NULL
 * - IK_CONTENT_TOOL_CALL -> kind="tool_call", data_json with structured data
 * - IK_CONTENT_TOOL_RESULT -> kind="tool_result", data_json with structured data
 *
 * @param ctx Talloc context for allocation (must not be NULL)
 * @param msg Normalized message to transform (must not be NULL)
 * @param out Output parameter for legacy message (must not be NULL)
 * @return    OK(msg) on success, ERR(...) on failure
 *
 * Error cases:
 * - ERR_INVALID_ARG if msg has no content blocks
 * - ERR_INVALID_ARG if content block type is unsupported
 */
res_t ik_openai_shim_transform_message(TALLOC_CTX *ctx, const ik_message_t *msg, ik_msg_t **out);

/**
 * Build legacy conversation from normalized messages
 *
 * Converts an array of normalized ik_message_t to legacy ik_openai_conversation_t.
 * System prompt is handled as the first message with kind="system".
 *
 * @param ctx Talloc context for allocation (must not be NULL)
 * @param req Normalized request with messages (must not be NULL)
 * @param out Output parameter for legacy conversation (must not be NULL)
 * @return    OK(conv) on success, ERR(...) on failure
 *
 * Error cases:
 * - ERR_INVALID_ARG if req has no messages
 * - Propagates errors from message transformation
 */
res_t ik_openai_shim_build_conversation(TALLOC_CTX *ctx, const ik_request_t *req, ik_openai_conversation_t **out);

/**
 * Transform normalized request to legacy OpenAI request format
 *
 * Converts from ik_request_t (normalized provider format) to ik_openai_request_t
 * (legacy OpenAI client format).
 *
 * Handles:
 * - Message transformation via ik_openai_shim_build_conversation()
 * - Model name passthrough
 * - Temperature (default 0.7 if not specified)
 * - max_output_tokens -> max_completion_tokens
 * - System prompt as first message
 *
 * @param ctx Talloc context for allocation (must not be NULL)
 * @param req Normalized request to transform (must not be NULL)
 * @param out Output parameter for legacy request (must not be NULL)
 * @return    OK(request) on success, ERR(...) on failure
 *
 * Error cases:
 * - ERR_INVALID_ARG if req has no messages
 * - Propagates errors from conversation building
 */
res_t ik_openai_shim_transform_request(TALLOC_CTX *ctx, const ik_request_t *req, ik_openai_request_t **out);

/**
 * Transform legacy OpenAI response message to normalized format
 *
 * Converts from ik_msg_t (legacy OpenAI client format) to ik_response_t
 * (normalized provider format).
 *
 * Transformation rules:
 * - kind="assistant" -> single text content block
 * - kind="tool_call" -> single tool_call content block (extracts from data_json)
 * - finish_reason extracted from msg metadata (if available)
 * - usage tokens set based on available data
 *
 * @param ctx Talloc context for allocation (must not be NULL)
 * @param msg Legacy message from ik_openai_chat_create() (must not be NULL)
 * @param out Output parameter for normalized response (must not be NULL)
 * @return    OK(response) on success, ERR(...) on failure
 *
 * Error cases:
 * - ERR_INVALID_ARG if msg is NULL
 * - ERR_PARSE if data_json is malformed for tool_call
 */
res_t ik_openai_shim_transform_response(TALLOC_CTX *ctx, const ik_msg_t *msg, ik_response_t **out);

/**
 * Map OpenAI finish_reason string to normalized enum
 *
 * Mapping:
 * - "stop" -> IK_FINISH_STOP
 * - "length" -> IK_FINISH_LENGTH
 * - "tool_calls" -> IK_FINISH_TOOL_USE
 * - "content_filter" -> IK_FINISH_CONTENT_FILTER
 * - NULL or unknown -> IK_FINISH_UNKNOWN
 *
 * @param openai_reason OpenAI finish_reason string (may be NULL)
 * @return              Normalized finish reason enum
 */
ik_finish_reason_t ik_openai_shim_map_finish_reason(const char *openai_reason);

#endif /* IK_PROVIDERS_OPENAI_SHIM_H */
