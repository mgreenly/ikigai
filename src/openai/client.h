#ifndef IK_OPENAI_CLIENT_H
#define IK_OPENAI_CLIENT_H

#include "error.h"
#include "config.h"
#include "msg.h"
#include "openai/sse_parser.h"
#include "openai/tool_choice.h"
#include "vendor/yyjson/yyjson.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * OpenAI API client module
 *
 * Provides HTTP client for OpenAI Chat Completions API with streaming support.
 * Uses libcurl for HTTP operations and yyjson for JSON parsing.
 *
 * Memory: All structures use talloc-based ownership
 * Errors: Result types with OK()/ERR() patterns
 */

/**
 * Message structure (ik_msg_t)
 *
 * The OpenAI module now uses the canonical ik_msg_t type defined in msg.h.
 * This type is shared across all modules for database storage, in-memory representation,
 * and rendering to scrollback.
 *
 * Note: The field is currently 'role' but will be renamed to 'kind' in a follow-up fix.
 */

/**
 * OpenAI conversation structure
 *
 * Container for a sequence of messages that form a conversation.
 * Passed to API to provide context for the request.
 */
typedef struct ik_openai_conversation {
    ik_msg_t **messages;  /* Array of message pointers */
    size_t message_count;         /* Number of messages */
} ik_openai_conversation_t;

/**
 * OpenAI API request structure
 *
 * Contains all parameters for a Chat Completions API request.
 */
typedef struct {
    char *model;                      /* Model identifier (e.g., "gpt-5-mini") */
    ik_openai_conversation_t *conv;   /* Conversation messages */
    double temperature;               /* Randomness (0.0-2.0) */
    int32_t max_completion_tokens;    /* Maximum response tokens */
    bool stream;                      /* Enable streaming responses */
} ik_openai_request_t;

/**
 * OpenAI API response structure
 *
 * Contains the complete response from the API after streaming completes.
 */
typedef struct {
    char *content;          /* Complete response text */
    char *finish_reason;    /* "stop", "length", "content_filter", etc. */
    int32_t prompt_tokens;  /* Tokens in the prompt */
    int32_t completion_tokens;  /* Tokens in the response */
    int32_t total_tokens;   /* Total tokens used */
} ik_openai_response_t;

/**
 * Streaming callback function type
 *
 * Called for each content chunk received during streaming.
 *
 * @param chunk   Content chunk (null-terminated string)
 * @param ctx     User-provided context pointer
 * @return        OK(NULL) to continue, ERR(...) to abort
 */
typedef res_t (*ik_openai_stream_cb_t)(const char *chunk, void *ctx);

/*
 * Message functions
 */

/**
 * Create a new canonical message
 *
 * @param ctx     Talloc context parent (or NULL)
 * @param role    Message role ("user", "assistant", "system")
 * @param content Message text content
 * @return        Pointer to message (never NULL, PANICs on OOM)
 */
ik_msg_t *ik_openai_msg_create(TALLOC_CTX *ctx, const char *role, const char *content);

/**
 * Create a canonical tool_call message
 *
 * Creates a message with role="tool_call" that will be transformed to OpenAI's
 * role="assistant" + tool_calls array format during serialization.
 *
 * @param parent      Talloc context parent (or NULL)
 * @param id          Tool call ID (e.g., "call_abc123")
 * @param type        Tool type (always "function" for now)
 * @param name        Function name (e.g., "glob", "file_read")
 * @param arguments   Function arguments as JSON string
 * @param content     Human-readable summary (e.g., "glob(pattern=\"*.c\")")
 * @return            Created message (panics on OOM)
 */
ik_msg_t *ik_openai_msg_create_tool_call(void *parent,
                                                const char *id,
                                                const char *type,
                                                const char *name,
                                                const char *arguments,
                                                const char *content);

/**
 * Create a canonical tool_result message
 *
 * Creates a message with role="tool_result" that will be transformed to OpenAI's
 * role="tool" format during serialization.
 *
 * @param parent        Talloc context parent (or NULL)
 * @param tool_call_id  ID of the tool call this result is for (e.g., "call_abc123")
 * @param content       Tool result content (JSON string with result data)
 * @return              Created message (panics on OOM)
 */
ik_msg_t *ik_openai_msg_create_tool_result(void *parent,
                                                  const char *tool_call_id,
                                                  const char *content);

/*
 * Conversation functions
 */

/**
 * Create a new conversation
 *
 * @param ctx  Talloc context parent (or NULL)
 * @return     Pointer to conversation (never NULL, PANICs on OOM)
 */
ik_openai_conversation_t *ik_openai_conversation_create(TALLOC_CTX *ctx);

/**
 * Add a message to a conversation
 *
 * @param conv    Conversation to modify
 * @param msg     Message to add (will be reparented to conv)
 * @return        OK(NULL) or ERR(...)
 */
res_t ik_openai_conversation_add_msg(ik_openai_conversation_t *conv, ik_msg_t *msg);

/**
 * Clear all messages from a conversation
 *
 * Removes all messages from the conversation, freeing associated memory.
 * Resets conversation to empty state.
 *
 * @param conv    Conversation to clear
 */
void ik_openai_conversation_clear(ik_openai_conversation_t *conv);

/*
 * Request/Response functions
 */

/**
 * Create a new API request
 *
 * Panics on out-of-memory.
 *
 * @param parent       Talloc context parent (or NULL)
 * @param cfg          Configuration with model, temperature, etc.
 * @param conv         Conversation to send (borrowed reference)
 * @return             Request instance
 */
ik_openai_request_t *ik_openai_request_create(void *parent, const ik_config_t *cfg, ik_openai_conversation_t *conv);

/**
 * Create a new API response
 *
 * Panics on out-of-memory.
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        Response instance
 */
ik_openai_response_t *ik_openai_response_create(void *parent);

/*
 * JSON serialization
 */

/**
 * Serialize a request to JSON
 *
 * Panics on out-of-memory.
 *
 * @param parent         Talloc context parent (or NULL)
 * @param request        Request to serialize
 * @param tool_choice    Tool choice configuration (auto, none, required, or specific)
 * @return               JSON string
 */
char *ik_openai_serialize_request(void *parent, const ik_openai_request_t *request, ik_tool_choice_t tool_choice);

/*
 * HTTP client
 */

/**
 * Send a chat completion request with streaming
 *
 * Returns canonical message format (ik_msg_t*).
 * - For tool calls: role="tool_call", data_json contains structured data, content has human-readable summary
 * - For text responses: role="assistant", content has response text
 *
 * @param parent      Talloc context parent (or NULL)
 * @param cfg         Configuration with API key, model, etc.
 * @param conv        Conversation to send
 * @param stream_cb   Callback for streaming chunks (or NULL for no streaming)
 * @param cb_ctx      Context pointer passed to callback
 * @return            OK(ik_msg_t*) or ERR(...)
 */
res_t ik_openai_chat_create(void *parent, const ik_config_t *cfg,
                             ik_openai_conversation_t *conv,
                             ik_openai_stream_cb_t stream_cb, void *cb_ctx);

/*
 * Internal wrapper function (exposed for testing)
 */

ik_msg_t *ik_openai_get_message_at_index(ik_msg_t **messages, size_t idx);

/*
 * Message serialization helpers (in client_serialize.c)
 */

/**
 * Serialize a tool_call message to OpenAI wire format
 *
 * Transforms canonical role="tool_call" to role="assistant" + tool_calls array.
 *
 * @param doc      yyjson mutable document
 * @param msg_obj  Message object to populate
 * @param msg      Message with role="tool_call"
 * @param parent   Talloc context for temporary allocations
 */
void ik_openai_serialize_tool_call_msg(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                                        const ik_msg_t *msg, void *parent);

/**
 * Serialize a tool_result message to OpenAI wire format
 *
 * Transforms canonical role="tool_result" to role="tool" + tool_call_id + content.
 *
 * @param doc      yyjson mutable document
 * @param msg_obj  Message object to populate
 * @param msg      Message with role="tool_result"
 * @param parent   Talloc context for temporary allocations
 */
void ik_openai_serialize_tool_result_msg(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                                          const ik_msg_t *msg, void *parent);

#endif /* IK_OPENAI_CLIENT_H */
