/**
 * @file request_helpers.h
 * @brief Google request serialization helper functions
 */

#ifndef IK_PROVIDERS_GOOGLE_REQUEST_HELPERS_H
#define IK_PROVIDERS_GOOGLE_REQUEST_HELPERS_H

#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"
#include <stdbool.h>

/**
 * Map internal role to Google role string
 */
const char *ik_google_role_to_string(ik_role_t role);

/**
 * Serialize a single content block to Google JSON format
 */
bool ik_google_serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                        const ik_content_block_t *block);

/**
 * Extract thought signature from provider_metadata JSON
 */
const char *ik_google_extract_thought_signature(const char *metadata, yyjson_doc **out_doc);

/**
 * Find most recent thought signature in messages
 */
const char *ik_google_find_latest_thought_signature(const ik_request_t *req, yyjson_doc **out_doc);

/**
 * Serialize message parts array
 */
bool ik_google_serialize_message_parts(yyjson_mut_doc *doc, yyjson_mut_val *content_obj,
                                        const ik_message_t *message, const char *thought_sig,
                                        bool is_first_assistant);

#endif /* IK_PROVIDERS_GOOGLE_REQUEST_HELPERS_H */
