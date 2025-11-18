#ifndef IK_OPENAI_CLIENT_H
#define IK_OPENAI_CLIENT_H

#include "error.h"
#include "config.h"
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
    char *model;                      /* Model identifier (e.g., "gpt-4-turbo") */
    ik_openai_conversation_t *conv;   /* Conversation messages */
    double temperature;               /* Randomness (0.0-2.0) */
    int32_t max_tokens;               /* Maximum response tokens */
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

/*
 * Request/Response functions
 */

/**
 * Create a new API request
 *
 * @param parent       Talloc context parent (or NULL)
 * @param cfg          Configuration with model, temperature, etc.
 * @param conv         Conversation to send (borrowed reference)
 * @return             OK(request) or ERR(...)
 */
res_t ik_openai_request_create(void *parent, const ik_cfg_t *cfg, ik_openai_conversation_t *conv);

/**
 * Create a new API response
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        OK(response) or ERR(...)
 */
res_t ik_openai_response_create(void *parent);

/*
 * JSON serialization
 */

/**
 * Serialize a request to JSON
 *
 * @param parent   Talloc context parent (or NULL)
 * @param request  Request to serialize
 * @return         OK(json_string) or ERR(...)
 */
res_t ik_openai_serialize_request(void *parent, const ik_openai_request_t *request);

/*
 * SSE (Server-Sent Events) parser
 */

/**
 * SSE parser state
 *
 * Accumulates incoming bytes and extracts complete SSE events.
 * Events are delimited by double newline (\n\n).
 */
typedef struct {
    char *buffer;         /* Accumulation buffer */
    size_t buffer_len;    /* Current buffer length (excluding null terminator) */
    size_t buffer_cap;    /* Buffer capacity (excluding null terminator) */
} ik_openai_sse_parser_t;

/**
 * Create a new SSE parser
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        OK(parser) or ERR(...)
 */
res_t ik_openai_sse_parser_create(void *parent);

/**
 * Feed data to the SSE parser
 *
 * Accumulates incoming bytes into the internal buffer.
 * Call ik_openai_sse_parser_get_event() to extract complete events.
 *
 * @param parser  Parser instance
 * @param data    Data to feed (does not need to be null-terminated)
 * @param len     Length of data in bytes
 * @return        OK(NULL) or ERR(...)
 */
res_t ik_openai_sse_parser_feed(ik_openai_sse_parser_t *parser,
                                  const char *data, size_t len);

/**
 * Get the next complete SSE event from the parser
 *
 * Extracts and returns the next complete event (delimited by \n\n).
 * Removes the event from the internal buffer.
 *
 * @param parser  Parser instance
 * @return        OK(event_string) if event available, OK(NULL) if no complete event
 */
res_t ik_openai_sse_parser_get_event(ik_openai_sse_parser_t *parser);

/**
 * Parse an SSE event and extract content delta
 *
 * Strips "data: " prefix, handles [DONE] marker, parses JSON,
 * and extracts choices[0].delta.content field.
 *
 * @param parent  Talloc context parent (or NULL)
 * @param event   SSE event string (e.g., "data: {...}")
 * @return        OK(content_string) if content present,
 *                OK(NULL) if [DONE] or no content,
 *                ERR(...) on parse error
 */
res_t ik_openai_parse_sse_event(void *parent, const char *event);

/*
 * HTTP client (to be implemented in later tasks)
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
 * Internal wrapper functions (exposed for testing)
 *
 * These consolidate yyjson inline functions to make defensive branches testable.
 */

yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc);
yyjson_val *yyjson_arr_get_wrapper(yyjson_val *arr, size_t idx);
bool yyjson_is_obj_wrapper(yyjson_val *val);
ik_openai_msg_t *get_message_at_index(ik_openai_msg_t **messages, size_t idx);

#endif /* IK_OPENAI_CLIENT_H */
