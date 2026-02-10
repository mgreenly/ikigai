#ifndef IK_PROVIDERS_RESPONSE_H
#define IK_PROVIDERS_RESPONSE_H

#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"
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


#endif /* IK_PROVIDERS_RESPONSE_H */
