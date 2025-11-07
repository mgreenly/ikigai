#ifndef IK_TEST_UTILS_H
#define IK_TEST_UTILS_H

#include <talloc.h>
#include <jansson.h>

// Test utilities for ikigai test suite

// ========== OOM Allocator Control ==========
// These functions control the behavior of ik_talloc_zero_for_error
// and _talloc_zero in test builds, allowing simulation of
// out-of-memory conditions

// Make the next allocation fail (return NULL)
// Affects both error allocations and talloc_zero calls
void oom_test_fail_next_alloc (void);

// Fail allocations after N successful calls
// Set to 0 to disable this behavior
void oom_test_fail_after_n_calls (int n);

// Reset OOM test state to normal operation
void oom_test_reset (void);

// Get the number of times allocations have been called
int oom_test_get_call_count (void);

// ========== Wrapper Function Overrides ==========
// These override the weak symbols in src/wrapper.c for testing

void *ik_talloc_zero_wrapper (TALLOC_CTX * ctx, size_t size);
char *ik_talloc_strdup_wrapper (TALLOC_CTX * ctx, const char *str);
void *ik_talloc_array_wrapper (TALLOC_CTX * ctx, size_t el_size, size_t count);
json_t *ik_json_object_wrapper (void);
char *ik_json_dumps_wrapper (const json_t * json, size_t flags);
int ik_json_is_object_wrapper (const json_t * json);
int ik_json_is_string_wrapper (const json_t * json);

#endif // IK_TEST_UTILS_H
