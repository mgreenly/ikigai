#include "error.h"
#include "wrapper.h"
#include <talloc.h>


#include "poison.h"
// Use the shared allocator wrapper for consistency
void *talloc_zero_for_error(TALLOC_CTX *ctx, size_t size)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    return talloc_zero_(ctx, size);
}
