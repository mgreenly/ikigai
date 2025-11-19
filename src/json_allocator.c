/**
 * @file json_allocator.c
 * @brief talloc-based allocator for yyjson
 */

#include "json_allocator.h"
#include <talloc.h>

/**
 * talloc-based malloc for yyjson
 */
static void *json_talloc_malloc(void *ctx, size_t size)
{
    return talloc_size((TALLOC_CTX *)ctx, size);
}

/**
 * talloc-based realloc for yyjson
 */
static void *json_talloc_realloc(void *ctx, void *ptr, size_t old_size, size_t size)
{
    (void)old_size;     /* unused, talloc tracks sizes internally */
    return talloc_realloc_size((TALLOC_CTX *)ctx, ptr, size);
}

/**
 * talloc-based free for yyjson
 */
static void json_talloc_free(void *ctx, void *ptr)
{
    (void)ctx;     /* unused, talloc_free doesn't need parent context */
    talloc_free(ptr);
}

yyjson_alc ik_make_talloc_allocator(TALLOC_CTX *ctx)
{
    yyjson_alc allocator = {
        .malloc = json_talloc_malloc,
        .realloc = json_talloc_realloc,
        .free = json_talloc_free,
        .ctx = ctx
    };
    return allocator;
}
