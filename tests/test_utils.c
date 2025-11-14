#include "test_utils.h"
#include <talloc.h>
#include <jansson.h>
#include <stdarg.h>

// ========== Allocator Wrapper Overrides ==========
// Strong symbols that override the weak symbols in src/wrapper.c

void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

void *ik_talloc_array_wrapper(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}

char *ik_talloc_asprintf_wrapper(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

// ========== Jansson Wrapper Overrides ==========
// Strong symbols that override the weak symbols in src/wrapper.c

json_t *ik_json_object_wrapper(void)
{
    return json_object();
}

char *ik_json_dumps_wrapper(const json_t *json, size_t flags)
{
    return json_dumps(json, flags);
}

int ik_json_is_object_wrapper(const json_t *json)
{
    return json_is_object(json);
}

int ik_json_is_string_wrapper(const json_t *json)
{
    return json_is_string(json);
}
