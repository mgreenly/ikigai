// External library wrappers for testing
// These provide link seams that tests can override to inject failures
//
// MOCKABLE functions are:
//   - weak symbols in debug/test builds (can be overridden)
//   - always_inline in release builds (zero overhead)

#ifndef IK_WRAPPER_H
#define IK_WRAPPER_H

#include <talloc.h>
#include <jansson.h>
#include <stddef.h>
#include <stdarg.h>

// MOCKABLE: Weak symbols for testing in debug builds,
// inline with definitions in header for zero overhead in release builds
#ifdef NDEBUG
#define MOCKABLE static inline
#else
#define MOCKABLE __attribute__((weak))
#endif

// ============================================================================
// talloc wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
MOCKABLE void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

MOCKABLE char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

MOCKABLE void *ik_talloc_array_wrapper(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

MOCKABLE void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}

MOCKABLE char *ik_talloc_asprintf_wrapper(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

#else
// Debug/test build: weak symbol declarations
MOCKABLE void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size);
MOCKABLE char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str);
MOCKABLE void *ik_talloc_array_wrapper(TALLOC_CTX *ctx, size_t el_size, size_t count);
MOCKABLE void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size);
MOCKABLE char *ik_talloc_asprintf_wrapper(TALLOC_CTX *ctx, const char *fmt, ...);
#endif

// ============================================================================
// jansson wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
MOCKABLE json_t *ik_json_object_wrapper(void)
{
    return json_object();
}

MOCKABLE char *ik_json_dumps_wrapper(const json_t *json, size_t flags)
{
    return json_dumps(json, flags);
}

MOCKABLE int ik_json_is_object_wrapper(const json_t *json)
{
    return json_is_object(json);
}

MOCKABLE int ik_json_is_string_wrapper(const json_t *json)
{
    return json_is_string(json);
}

#else
// Debug/test build: weak symbol declarations
MOCKABLE json_t *ik_json_object_wrapper(void);
MOCKABLE char *ik_json_dumps_wrapper(const json_t *json, size_t flags);
MOCKABLE int ik_json_is_object_wrapper(const json_t *json);
MOCKABLE int ik_json_is_string_wrapper(const json_t *json);
#endif

#endif // IK_WRAPPER_H
