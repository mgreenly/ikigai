// REPL HTTP callback handlers
#pragma once

#include "error.h"
#include "openai/client.h"
#include "openai/client_multi.h"

// Forward declaration
struct ik_repl_ctx_t;

/**
 * @brief Streaming callback for OpenAI API responses
 *
 * Called for each content chunk received during streaming.
 * Appends the chunk to the scrollback buffer.
 *
 * @param chunk   Content chunk (null-terminated string)
 * @param ctx     REPL context pointer
 * @return        OK(NULL) to continue, ERR(...) to abort
 */
res_t ik_repl_streaming_callback(const char *chunk, void *ctx);

/**
 * @brief Completion callback for HTTP requests
 *
 * Called when an HTTP request completes (success or failure).
 * Stores error information in REPL context for display by completion handler.
 *
 * NOTE: This function is tested manually (see Tasks 7.10-7.14 in tasks.md)
 *
 * @param completion   Completion information (status, error message)
 * @param ctx          REPL context pointer
 * @return             OK(NULL) on success, ERR(...) on failure
 */
res_t ik_repl_http_completion_callback(const ik_http_completion_t *completion, void *ctx);
