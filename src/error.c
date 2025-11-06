#include "error.h"
#include <talloc.h>

// Weak implementation of talloc allocator for errors
// Tests can provide a strong symbol to override this and inject failures
// LCOV_EXCL_START
__attribute__((weak))
void *ik_talloc_zero_for_error(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

// LCOV_EXCL_STOP
