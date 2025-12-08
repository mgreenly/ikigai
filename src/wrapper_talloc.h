// talloc wrappers for testing
#ifndef IK_WRAPPER_TALLOC_H
#define IK_WRAPPER_TALLOC_H

#include <stdarg.h>
#include <stddef.h>
#include <talloc.h>
#include "wrapper_base.h"

#ifdef NDEBUG
MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

MOCKABLE char *talloc_strdup_(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

MOCKABLE void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

MOCKABLE void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}

MOCKABLE char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

#else
MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
MOCKABLE char *talloc_strdup_(TALLOC_CTX *ctx, const char *str);
MOCKABLE void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count);
MOCKABLE void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size);
MOCKABLE char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...);
#endif

#endif // IK_WRAPPER_TALLOC_H
