#ifndef IK_ERROR_H
#define IK_ERROR_H

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <talloc.h>

// Error codes - start empty, add as needed during Phase 1
typedef enum {
    OK = 0,
    ERR_INVALID_ARG,
    ERR_OUT_OF_RANGE,
    ERR_IO,                  // File operations, config loading
    ERR_PARSE,               // JSON/protocol parsing
} err_code_t;

// Error with context and embedded message buffer
// Always talloc-allocated on a parent context
typedef struct err {
    err_code_t code;
    const char *file;
    int32_t line;
    char msg[128];
} err_t;

// Result type - stack-allocated, zero overhead for success case
typedef struct {
    union {
        void *ok;
        err_t *err;
    };
    bool is_err;
} res_t;

// Core Result construction - always inline for zero overhead
static inline res_t ok(void *value)
{
    return (res_t)
           {
               .ok = value, .is_err = false
           };
}

static inline res_t err(err_t *error)
{
    assert(error != NULL); // LCOV_EXCL_BR_LINE
    return (res_t)
           {
               .err = error, .is_err = true
           };
}

// Result checking
static inline bool is_ok(const res_t *result)
{
    assert(result != NULL); // LCOV_EXCL_BR_LINE
    return !result->is_err;
}

static inline bool is_err(const res_t *result)
{
    assert(result != NULL); // LCOV_EXCL_BR_LINE
    return result->is_err;
}

// Injectable allocator for error structures (defined in error.c)
// Weak symbol - tests can override to inject allocation failures
void *talloc_zero_for_error(TALLOC_CTX *ctx, size_t size);

// Forward declaration of PANIC (defined in panic.h)
// Source files using error.h should include panic.h to get the implementation
#define PANIC(msg) ik_panic_impl((msg), __FILE__, __LINE__)
void ik_panic_impl(const char *msg, const char *file, int line) __attribute__((noreturn));

// Error creation - allocates on talloc context
static inline err_t *_make_error(TALLOC_CTX *ctx,
                                 err_code_t code,
                                 const char *file,
                                 int32_t line,
                                 const char *fmt,
                                 ...)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(fmt != NULL); // LCOV_EXCL_BR_LINE
    err_t *err = talloc_zero_for_error(ctx, sizeof(err_t));
    if (err == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    err->code = code;
    err->file = file;
    err->line = line;

    va_list args;
    va_start(args, fmt);
    int32_t written = vsnprintf(err->msg, sizeof(err->msg), fmt, args);
    va_end(args);
    if (written < 0) PANIC("vsnprintf failed in error message formatting");   // LCOV_EXCL_LINE
    if ((size_t)written >= sizeof(err->msg)) PANIC("error message truncated");   // LCOV_EXCL_LINE

    return err;
}

// Clean public API - Rust-style OK/ERR
#define OK(value) ok(value)

#define ERR(ctx, code_suffix, ...) \
        err(_make_error(ctx, ERR_ ## code_suffix, __FILE__, __LINE__, __VA_ARGS__))

// CHECK macro - propagate error to caller (return early if error)
#define CHECK(expr) \
        do { \
            res_t _result = (expr); \
            if (_result.is_err) { \
                return _result; \
            } \
        } while (0)

// TRY macro - extract ok value or propagate error (returns early if error)
// Uses GCC statement expression to allow use in assignments
#define TRY(expr) \
        ({ \
        res_t _try_result = (expr); \
        if (_try_result.is_err) { \
            return _try_result; \
        } \
        _try_result.ok; \
    })

// Error code to string conversion
static inline const char *error_code_str(err_code_t code)
{
    switch (code) { // LCOV_EXCL_BR_LINE
        case OK:
            return "OK";
        case ERR_INVALID_ARG:
            return "Invalid argument";
        case ERR_OUT_OF_RANGE:
            return "Out of range";
        case ERR_IO:
            return "IO error";
        case ERR_PARSE:
            return "Parse error";
        default: // LCOV_EXCL_LINE
            PANIC("Invalid error code"); // LCOV_EXCL_LINE
    }
}

// Error inspection
static inline err_code_t error_code(const err_t *err)
{
    assert(err != NULL); // LCOV_EXCL_BR_LINE
    return err->code;
}

static inline const char *error_message(const err_t *err)
{
    assert(err != NULL); // LCOV_EXCL_BR_LINE
    return err->msg[0] ? err->msg : error_code_str(err->code);
}

// Error formatting for debugging
static inline void error_fprintf(FILE *f, const err_t *err)
{
    assert(f != NULL); // LCOV_EXCL_BR_LINE
    assert(err != NULL); // LCOV_EXCL_BR_LINE
    fprintf(f, "Error: %s [%s:%" PRId32 "]\n", error_message(err), err->file ? err->file : "unknown", err->line);
}

#endif // IK_ERROR_H
