#include "apps/ikigai/providers/response.h"
#include "shared/error.h"
#include "shared/wrapper.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
/**
 * Response Builder Implementation
 *
 * This module provides builder functions for constructing and working
 * with ik_response_t structures.
 */

/* ================================================================
 * Response Builder Functions
 * ================================================================ */

res_t ik_response_create(TALLOC_CTX *ctx, ik_response_t **out)
{
    assert(out != NULL); // LCOV_EXCL_BR_LINE

    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    if (!resp) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    resp->content_blocks = NULL;
    resp->content_count = 0;
    resp->finish_reason = IK_FINISH_STOP;
    resp->usage.input_tokens = 0;
    resp->usage.output_tokens = 0;
    resp->usage.thinking_tokens = 0;
    resp->usage.cached_tokens = 0;
    resp->usage.total_tokens = 0;
    resp->model = NULL;
    resp->provider_data = NULL;

    *out = resp;
    return OK(*out);
}

