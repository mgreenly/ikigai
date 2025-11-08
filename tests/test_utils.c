#include "test_utils.h"
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include <stdarg.h>

// ========== OOM Test State ==========

// Test control state
static struct {
    int should_fail_next;
    int fail_after_n_calls;
    int call_count;
} oom_state = { 0 };

// ========== OOM Control API ==========

void oom_test_fail_next_alloc(void)
{
    oom_state.should_fail_next = 1;
}

void oom_test_fail_after_n_calls(int n)
{
    oom_state.fail_after_n_calls = n;
    oom_state.call_count = 0;
}

void oom_test_reset(void)
{
    memset(&oom_state, 0, sizeof(oom_state));
}

int oom_test_get_call_count(void)
{
    return oom_state.call_count;
}

// ========== Allocator Wrapper Overrides ==========
// Strong symbols that override the weak symbols in src/alloc.c

void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size)
{
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal allocation
    return talloc_zero_size(ctx, size);
}

char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str)
{
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal allocation
    return talloc_strdup(ctx, str);
}

void *ik_talloc_array_wrapper(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal allocation
    return talloc_zero_size(ctx, el_size * count);
}

void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal allocation
    return talloc_realloc_size(ctx, ptr, size);
}

char *ik_talloc_asprintf_wrapper(TALLOC_CTX *ctx, const char *fmt, ...)
{
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal allocation
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
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal allocation
    return json_object();
}

char *ik_json_dumps_wrapper(const json_t *json, size_t flags)
{
    oom_state.call_count++;

    // Check if we should fail the next allocation
    if (oom_state.should_fail_next) {
        oom_state.should_fail_next = 0;
        return NULL;
    }

    // Check if we should fail after N calls
    if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls) {
        return NULL;
    }

    // Normal operation
    return json_dumps(json, flags);
}

int ik_json_is_object_wrapper(const json_t *json)
{
    // These wrapper functions don't participate in OOM testing
    // They're just for isolating external library calls
    return json_is_object(json);
}

int ik_json_is_string_wrapper(const json_t *json)
{
    // These wrapper functions don't participate in OOM testing
    // They're just for isolating external library calls
    return json_is_string(json);
}
