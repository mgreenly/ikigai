/**
 * @file serialize.h
 * @brief OpenAI JSON serialization utilities
 */

#ifndef IK_PROVIDERS_OPENAI_SERIALIZE_H
#define IK_PROVIDERS_OPENAI_SERIALIZE_H

#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

/**
 * Serialize a single message to OpenAI Chat Completions API JSON format
 *
 * Handles all message types:
 * - User/Assistant messages with text content
 * - Assistant messages with tool calls
 * - Tool result messages
 *
 * @param doc yyjson mutable document
 * @param msg Message to serialize
 * @return Mutable JSON value representing the message, or NULL on error
 */
yyjson_mut_val *ik_openai_serialize_message(yyjson_mut_doc *doc, const ik_message_t *msg);

/**
 * Serialize message content blocks to OpenAI Responses API JSON format
 *
 * Returns an array of Responses API input items:
 * - User/Assistant text messages: {"role": "user"/"assistant", "content": "..."}
 * - Tool calls: {"type": "function_call", "call_id": "...", "name": "...", "arguments": "..."}
 * - Tool results: {"type": "function_call_output", "call_id": "...", "output": "..."}
 *
 * @param doc yyjson mutable document
 * @param msg Message to serialize
 * @param out_arr Mutable JSON array to append items to
 * @return true on success, false on error
 */
bool ik_openai_serialize_responses_message(yyjson_mut_doc *doc, const ik_message_t *msg,
                                           yyjson_mut_val *out_arr);

#endif // IK_PROVIDERS_OPENAI_SERIALIZE_H
