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
 *
 * @param role  Message role
 * @param model Model identifier (for Gemini 3 detection)
 * @return      Google role string ("user", "model", or "function")
 *
 * For Gemini 3 models, IK_ROLE_TOOL maps to "user".
 * For other models, IK_ROLE_TOOL maps to "function".
 */
const char *ik_google_role_to_string(ik_role_t role, const char *model);

/**
 * Serialize a single content block to Google JSON format
 *
 * @param doc   Mutable JSON document
 * @param arr   Parts array to append to
 * @param block Content block to serialize
 * @param model Model identifier (for Gemini 3 thought signature handling)
 * @return      true on success, false on failure
 */
bool ik_google_serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                       const ik_content_block_t *block, const char *model);

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
 *
 * @param doc                Mutable JSON document
 * @param content_obj        Content object to add parts to
 * @param message            Message to serialize
 * @param thought_sig        Thought signature (for first assistant message)
 * @param is_first_assistant true if this is the first assistant message
 * @param model              Model identifier (for Gemini 3 thought signature handling)
 * @return                   true on success, false on failure
 */
bool ik_google_serialize_message_parts(yyjson_mut_doc *doc,
                                       yyjson_mut_val *content_obj,
                                       const ik_message_t *message,
                                       const char *thought_sig,
                                       bool is_first_assistant,
                                       const char *model);

#endif /* IK_PROVIDERS_GOOGLE_REQUEST_HELPERS_H */
