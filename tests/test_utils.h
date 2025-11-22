#ifndef IK_TEST_UTILS_H
#define IK_TEST_UTILS_H

#include <talloc.h>
#include "config.h"

// Test utilities for ikigai test suite

// ========== Wrapper Function Overrides ==========
// These override the weak symbols in src/wrapper.c for testing

void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
char *talloc_strdup_(TALLOC_CTX *ctx, const char *str);
void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count);
void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size);
char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...);

// ========== Test Config Helper ==========
// Creates a minimal config for testing (does not require config file)
ik_cfg_t *ik_test_create_config(TALLOC_CTX *ctx);

// ========== File I/O Helpers ==========
// Load a file into a talloc-allocated string
char *load_file_to_string(TALLOC_CTX *ctx, const char *path);

#endif // IK_TEST_UTILS_H
