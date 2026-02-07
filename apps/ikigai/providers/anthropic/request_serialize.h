/**
 * @file request_serialize.h
 * @brief Anthropic request serialization helpers (internal)
 */

#ifndef IK_PROVIDERS_ANTHROPIC_REQUEST_SERIALIZE_H
#define IK_PROVIDERS_ANTHROPIC_REQUEST_SERIALIZE_H

#include <stdbool.h>
#include "vendor/yyjson/yyjson.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Serialize a single content block to Anthropic JSON format
 */
bool ik_anthropic_serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr, const ik_content_block_t *block, size_t message_idx, size_t block_idx);

/**
 * Serialize message content (handles both string and array formats)
 */
bool ik_anthropic_serialize_message_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj, const ik_message_t *message, size_t message_idx);

/**
 * Serialize messages array
 */
bool ik_anthropic_serialize_messages(yyjson_mut_doc *doc, yyjson_mut_val *root, const ik_request_t *req);

/**
 * Map internal role to Anthropic role string
 */
const char *ik_anthropic_role_to_string(ik_role_t role);

#endif /* IK_PROVIDERS_ANTHROPIC_REQUEST_SERIALIZE_H */
