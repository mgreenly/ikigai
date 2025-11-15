#ifndef IK_TEST_UTILS_H
#define IK_TEST_UTILS_H

#include <talloc.h>

// Test utilities for ikigai test suite

// ========== Wrapper Function Overrides ==========
// These override the weak symbols in src/wrapper.c for testing

void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size);
char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str);
void *ik_talloc_array_wrapper(TALLOC_CTX *ctx, size_t el_size, size_t count);
void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size);
char *ik_talloc_asprintf_wrapper(TALLOC_CTX *ctx, const char *fmt, ...);

#endif // IK_TEST_UTILS_H
