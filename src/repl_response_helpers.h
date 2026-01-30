// REPL response processing helper functions
#ifndef IK_REPL_RESPONSE_HELPERS_H
#define IK_REPL_RESPONSE_HELPERS_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct ik_agent_ctx ik_agent_ctx_t;
typedef struct ik_response ik_response_t;

/**
 * Flush accumulated line content to scrollback with optional model prefix
 */
void ik_repl_flush_line_to_scrollback(ik_agent_ctx_t *agent, const char *chunk,
                                      size_t start, size_t chunk_len);

/**
 * Handle text delta in streaming response
 */
void ik_repl_handle_text_delta(ik_agent_ctx_t *agent, const char *chunk, size_t chunk_len);

/**
 * Render usage statistics event
 */
void ik_repl_render_usage_event(ik_agent_ctx_t *agent);

/**
 * Store response metadata (model, finish reason, token counts)
 */
void ik_repl_store_response_metadata(ik_agent_ctx_t *agent, const ik_response_t *response);

/**
 * Extract tool calls and thinking from response content blocks
 */
void ik_repl_extract_tool_calls(ik_agent_ctx_t *agent, const ik_response_t *response);

#endif /* IK_REPL_RESPONSE_HELPERS_H */
