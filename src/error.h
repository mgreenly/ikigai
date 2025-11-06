#ifndef IK_ERROR_H
#define IK_ERROR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <talloc.h>

// Error codes - start empty, add as needed during Phase 1
typedef enum
{
  IK_OK = 0,
  IK_ERR_OOM,			// Out of memory
  IK_ERR_INVALID_ARG,
  IK_ERR_OUT_OF_RANGE,
} ik_error_code_t;

// Error with context and embedded message buffer
// Always talloc-allocated on a parent context
typedef struct ik_error
{
  ik_error_code_t code;
  const char *file;
  int32_t line;
  char message[128];
} ik_error_t;

// Result type - stack-allocated, zero overhead for success case
typedef struct
{
  union
  {
    void *ok;
    ik_error_t *err;
  };
  bool is_err;
} ik_result_t;

// Core Result construction - always inline for zero overhead
static inline ik_result_t
ik_ok (void *value)
{
  return (ik_result_t)
  {
  .ok = value,.is_err = false};
}

static inline ik_result_t
ik_err (ik_error_t *error)
{
  return (ik_result_t)
  {
  .err = error,.is_err = true};
}

// Result checking
static inline bool
ik_is_ok (const ik_result_t *result)
{
  return !result->is_err;
}

static inline bool
ik_is_err (const ik_result_t *result)
{
  return result->is_err;
}

// Injectable allocator for error structures (defined in error.c)
// Weak symbol - tests can override to inject allocation failures
void *ik_talloc_zero_for_error (TALLOC_CTX * ctx, size_t size);

// Global static error for OOM situations
// Read-only, no race conditions, cannot be freed with talloc_free()
static const ik_error_t ik_oom_error = {
  .code = IK_ERR_OOM,
  .file = "<oom>",
  .line = 0,
  .message = "Out of memory"
};

// Check if an error is the static OOM error (cannot be freed)
static inline bool
ik_error_is_static (const ik_error_t *err)
{
  return err == &ik_oom_error;
}

// Error creation - allocates on talloc context
static inline ik_error_t *
_ik_make_error (TALLOC_CTX *ctx, ik_error_code_t code, const char *file, int line, const char *fmt, ...)
{
  ik_error_t *err = ik_talloc_zero_for_error (ctx, sizeof (ik_error_t));
  if (!err)
    {
      return (ik_error_t *) & ik_oom_error;
    }

  err->code = code;
  err->file = file;
  err->line = line;

  va_list args;
  va_start (args, fmt);
  vsnprintf (err->message, sizeof (err->message), fmt, args);
  va_end (args);

  return err;
}

// Clean public API - Rust-style OK/ERR
#define OK(value) ik_ok(value)

#define ERR(ctx, code_suffix, ...) \
  ik_err(_ik_make_error(ctx, IK_ERR_##code_suffix, __FILE__, __LINE__, __VA_ARGS__))

// CHECK macro - propagate error to caller (return early if error)
#define CHECK(expr) \
  do { \
    ik_result_t _result = (expr); \
    if (_result.is_err) { \
      return _result; \
    } \
  } while (0)

// Error code to string conversion
static inline const char *
ik_error_code_str (ik_error_code_t code)
{
  switch (code)
    {
    case IK_OK:
      return "OK";
    case IK_ERR_OOM:
      return "Out of memory";
    case IK_ERR_INVALID_ARG:
      return "Invalid argument";
    case IK_ERR_OUT_OF_RANGE:
      return "Out of range";
    default:
      return "Unknown error";
    }
}

// Error inspection
static inline ik_error_code_t
ik_error_code (const ik_error_t *err)
{
  return err ? err->code : IK_OK;
}

static inline const char *
ik_error_message (const ik_error_t *err)
{
  if (!err)
    return "Success";
  return err->message[0] ? err->message : ik_error_code_str (err->code);
}

// Error formatting for debugging
static inline void
ik_error_fprintf (FILE *f, const ik_error_t *err)
{
  if (!err)
    {
      fprintf (f, "Success\n");
      return;
    }

  fprintf (f, "Error: %s [%s:%d]\n", ik_error_message (err), err->file ? err->file : "unknown", err->line);
}

// Common error conditions as inline functions
static inline ik_result_t
ik_check_null (TALLOC_CTX *ctx, const void *ptr, const char *param_name)
{
  (void) param_name;
  if (!ptr)
    {
      return ERR (ctx, INVALID_ARG, "NULL pointer parameter");
    }
  return ik_ok ((void *) (uintptr_t) ptr);
}

static inline ik_result_t
ik_check_range (TALLOC_CTX *ctx, size_t value, size_t min, size_t max, const char *param_name)
{
  (void) param_name;
  if (value < min || value > max)
    {
      return ERR (ctx, OUT_OF_RANGE, "Value out of range");
    }
  return ik_ok (NULL);
}

#endif // IK_ERROR_H
