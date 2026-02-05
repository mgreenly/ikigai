/**
 * @file response_utils.h
 * @brief Google response utility functions (internal)
 */

#ifndef IK_PROVIDERS_GOOGLE_RESPONSE_UTILS_H
#define IK_PROVIDERS_GOOGLE_RESPONSE_UTILS_H

#include <talloc.h>
#include "apps/ikigai/providers/provider.h"
#include "vendor/yyjson/yyjson.h"

/**
 * Extract thought signature from response JSON
 *
 * @param ctx  Talloc context for allocation
 * @param root Parsed JSON root value
 * @return     JSON string with thought signature, or NULL if not present
 *
 * Internal helper for ik_google_parse_response.
 * Searches for thoughtSignature field in response and builds provider_data JSON.
 */
char *ik_google_extract_thought_signature_from_response(TALLOC_CTX *ctx, yyjson_val *root);

#endif /* IK_PROVIDERS_GOOGLE_RESPONSE_UTILS_H */
