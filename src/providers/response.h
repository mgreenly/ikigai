#ifndef IK_PROVIDERS_RESPONSE_H
#define IK_PROVIDERS_RESPONSE_H

#include "error.h"
#include "providers/provider.h"
#include <talloc.h>

/**
 * Response Builder API
 *
 * This module provides builder functions for constructing and working
 * with ik_response_t structures.
 *
 * Memory model: All builders allocate on the provided TALLOC_CTX and
 * use talloc hierarchical ownership for automatic cleanup.
 */

/* ================================================================
 * Response Builder Functions
 * ================================================================ */

/**
 * Create empty response
 *
 * Allocates and initializes ik_response_t structure with empty
 * content array, finish_reason set to STOP, and zeroed usage counters.
 *
 * @param ctx Talloc parent context
 * @param out Receives allocated response
 * @return    OK with response, ERR on allocation failure
 */
res_t ik_response_create(TALLOC_CTX *ctx, ik_response_t **out);

/**
 * Add content block to response
 *
 * Appends a content block to the response's content array. The block
 * is owned by the response after adding.
 *
 * @param resp  Response to modify
 * @param block Content block to add (will be copied)
 * @return      OK on success, ERR on allocation failure
 */
res_t ik_response_add_content(ik_response_t *resp, ik_content_block_t *block);

#endif /* IK_PROVIDERS_RESPONSE_H */
