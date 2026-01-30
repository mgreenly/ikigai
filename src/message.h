#ifndef IK_MESSAGE_H
#define IK_MESSAGE_H

#include "error.h"
#include "msg.h"
#include "providers/provider.h"

#include <talloc.h>

/**
 * Provider-agnostic message API
 *
 * This module provides functions for creating and managing ik_message_t
 * structures used in the new provider system. It also handles conversion
 * from database ik_msg_t format to the provider-agnostic ik_message_t format.
 */

/**
 * Create a text message
 *
 * Creates an ik_message_t with a single text content block.
 *
 * @param ctx  Talloc context for allocation
 * @param role Message role (IK_ROLE_USER, IK_ROLE_ASSISTANT, IK_ROLE_TOOL)
 * @param text Text content (must not be NULL)
 * @return     Allocated message (never NULL, panics on OOM)
 */
ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text);

/**
 * Create a tool call message
 *
 * Creates an ik_message_t with IK_ROLE_ASSISTANT and a single tool call content block.
 *
 * @param ctx       Talloc context for allocation
 * @param id        Tool call ID (must not be NULL)
 * @param name      Tool name (must not be NULL)
 * @param arguments Tool arguments as JSON string (must not be NULL)
 * @return          Allocated message (never NULL, panics on OOM)
 */
ik_message_t *ik_message_create_tool_call(TALLOC_CTX *ctx, const char *id, const char *name, const char *arguments);

/**
 * Create assistant message with thinking and tool call blocks
 *
 * Creates a message containing:
 * 1. Thinking block (if thinking_text != NULL)
 * 2. Redacted thinking block (if redacted_data != NULL)
 * 3. Tool call block
 *
 * @param ctx                   Talloc parent
 * @param thinking_text         Thinking text (may be NULL)
 * @param thinking_sig          Thinking signature (may be NULL)
 * @param redacted_data         Redacted thinking data (may be NULL)
 * @param tool_id               Tool call ID
 * @param tool_name             Tool name
 * @param tool_args             Tool arguments JSON
 * @param tool_thought_sig      Tool call thought signature (Gemini 3, may be NULL)
 * @return                      Allocated message
 */
ik_message_t *ik_message_create_tool_call_with_thinking(TALLOC_CTX *ctx,
                                                        const char *thinking_text,
                                                        const char *thinking_sig,
                                                        const char *redacted_data,
                                                        const char *tool_id,
                                                        const char *tool_name,
                                                        const char *tool_args,
                                                        const char *tool_thought_sig);

/**
 * Create a tool result message
 *
 * Creates an ik_message_t with IK_ROLE_TOOL and a single tool result content block.
 *
 * @param ctx          Talloc context for allocation
 * @param tool_call_id Tool call ID this result is for (must not be NULL)
 * @param content      Result content (must not be NULL)
 * @param is_error     true if tool execution failed
 * @return             Allocated message (never NULL, panics on OOM)
 */
ik_message_t *ik_message_create_tool_result(TALLOC_CTX *ctx,
                                            const char *tool_call_id,
                                            const char *content,
                                            bool is_error);

/**
 * Convert database message to provider message format
 *
 * Converts ik_msg_t (database format) to ik_message_t (provider format).
 * Handles text messages, tool calls, and tool results.
 *
 * Special case: System messages are handled via request->system_prompt,
 * not as messages in the conversation array. When db_msg->kind is "system",
 * this function sets *out to NULL and returns OK.
 *
 * @param ctx    Talloc context for allocation (also used for error allocation)
 * @param db_msg Database message to convert (must not be NULL)
 * @param out    Output parameter for converted message (must not be NULL)
 *               Set to NULL for system messages, set to message otherwise
 * @return       OK on success, ERR on parse failure (JSON errors)
 */
res_t ik_message_from_db_msg(TALLOC_CTX *ctx, const ik_msg_t *db_msg, ik_message_t **out);

#endif /* IK_MESSAGE_H */
