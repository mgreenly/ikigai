// REPL HTTP callback handlers
#pragma once

#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

// Forward declaration
struct ik_repl_ctx_t;

/**
 * @brief Stream callback for provider API responses
 *
 * Called during perform() as data arrives from the network.
 * Handles normalized stream events (text deltas, thinking, tool calls, etc.).
 * Updates UI incrementally as content streams in.
 *
 * @param event   Stream event (text delta, tool call, etc.)
 * @param ctx     Agent context pointer
 * @return        OK(NULL) to continue, ERR(...) to abort
 */
res_t ik_repl_stream_callback(const ik_stream_event_t *event, void *ctx);

/**
 * @brief Completion callback for provider requests
 *
 * Called from info_read() when an HTTP request completes (success or failure).
 * Stores response metadata and finalizes streaming state.
 *
 * @param completion   Completion information (usage, metadata, error)
 * @param ctx          Agent context pointer
 * @return             OK(NULL) on success, ERR(...) on failure
 */
res_t ik_repl_completion_callback(const ik_provider_completion_t *completion, void *ctx);
