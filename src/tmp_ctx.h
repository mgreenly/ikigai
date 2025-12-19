#ifndef IK_TMP_CTX_H
#define IK_TMP_CTX_H

#include <talloc.h>

#include "panic.h"

// Create a temporary talloc context, PANIC if allocation fails
// Usage:
//   TALLOC_CTX *tmp = tmp_ctx_create();
//   // ... use tmp ...
//   talloc_free(tmp);
static inline TALLOC_CTX *tmp_ctx_create(void)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    if (ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return ctx;
}

#endif // IK_TMP_CTX_H
