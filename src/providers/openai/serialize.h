/**
 * @file serialize.h
 * @brief OpenAI JSON serialization utilities
 */

#ifndef IK_PROVIDERS_OPENAI_SERIALIZE_H
#define IK_PROVIDERS_OPENAI_SERIALIZE_H

#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

/**
 * Serialize a single message to OpenAI JSON format
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

#endif // IK_PROVIDERS_OPENAI_SERIALIZE_H
