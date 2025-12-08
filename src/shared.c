#include "shared.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>

res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
    if (shared == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    *out = shared;
    return OK(shared);
}
