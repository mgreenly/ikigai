#ifndef IK_OPENAI_CLIENT_H
#define IK_OPENAI_CLIENT_H

#include "error.h"
#include "config.h"
#include "openai/sse_parser.h"
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
 * OpenAI message structure
 *
 * Represents a single message in a conversation (user, assistant, or system).
 * Follows OpenAI Chat API message format.
 */
typedef struct {
    char *role;      /* "user", "assistant", or "system" */
    char *content;   /* Message text content */
} ik_openai_msg_t;

/**
 * OpenAI conversation structure
 *
 * Container for a sequence of messages that form a conversation.
 * Passed to API to provide context for the request.
 */
typedef struct {
    ik_openai_msg_t **messages;  /* Array of message pointers */
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
 * Create a new OpenAI message
 *
 * @param parent  Talloc context parent (or NULL)
 * @param role    Message role ("user", "assistant", "system")
 * @param content Message text content
 * @return        OK(message) or ERR(...)
 */
res_t ik_openai_msg_create(void *parent, const char *role, const char *content);

/*
 * Conversation functions
 */

/**
 * Create a new conversation
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        OK(conversation) or ERR(...)
 */
res_t ik_openai_conversation_create(void *parent);

/**
 * Add a message to a conversation
 *
 * @param conv    Conversation to modify
 * @param msg     Message to add (will be reparented to conv)
 * @return        OK(NULL) or ERR(...)
 */
res_t ik_openai_conversation_add_msg(ik_openai_conversation_t *conv, ik_openai_msg_t *msg);

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
ik_openai_request_t *ik_openai_request_create(void *parent, const ik_cfg_t *cfg, ik_openai_conversation_t *conv);

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
 * @param parent   Talloc context parent (or NULL)
 * @param request  Request to serialize
 * @return         JSON string
 */
char *ik_openai_serialize_request(void *parent, const ik_openai_request_t *request);

/*
 * HTTP client
 */

/**
 * Send a chat completion request with streaming
 *
 * @param parent      Talloc context parent (or NULL)
 * @param cfg         Configuration with API key, model, etc.
 * @param conv        Conversation to send
 * @param stream_cb   Callback for streaming chunks (or NULL for no streaming)
 * @param cb_ctx      Context pointer passed to callback
 * @return            OK(response) or ERR(...)
 */
res_t ik_openai_chat_create(void *parent, const ik_cfg_t *cfg,
                             ik_openai_conversation_t *conv,
                             ik_openai_stream_cb_t stream_cb, void *cb_ctx);

/*
 * Internal wrapper function (exposed for testing)
 */

ik_openai_msg_t *get_message_at_index(ik_openai_msg_t **messages, size_t idx);

#endif /* IK_OPENAI_CLIENT_H */
