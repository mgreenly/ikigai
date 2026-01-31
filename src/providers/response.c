#include "providers/response.h"
#include "error.h"
#include "wrapper.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
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

res_t ik_response_add_content(ik_response_t *resp, ik_content_block_t *block)
{
    assert(resp != NULL);  // LCOV_EXCL_BR_LINE
    assert(block != NULL); // LCOV_EXCL_BR_LINE

    /* Grow content blocks array */
    size_t new_count = resp->content_count + 1;
    resp->content_blocks = talloc_realloc(resp, resp->content_blocks,
                                          ik_content_block_t, (unsigned int)new_count);
    if (!resp->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy block contents */
    resp->content_blocks[resp->content_count] = *block;
    resp->content_count = new_count;

    /* Steal block to response (becomes child of response) */
    talloc_steal(resp, block);

    return OK(NULL);
}
