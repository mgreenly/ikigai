---
name: errors
description: Error Handling skill for the ikigai project
---

# Error Handling

## Three Mechanisms

| Mechanism | When | Compiles Out? |
|-----------|------|---------------|
| `res_t` | IO, parsing, external failures | No |
| `assert()` | Preconditions, contracts | Yes (-DNDEBUG) |
| `PANIC()` | OOM, corruption, impossible states | No |

## Decision Framework

1. **OOM?** → `PANIC()`
2. **Can happen with correct code?** → `res_t`
3. **Precondition/contract?** → `assert()`
4. **Impossible state?** → `PANIC()`

## Core Types

```c
typedef struct {
    union { void *ok; err_t *err; };
    bool is_err;
} res_t;

typedef struct err {
    err_code_t code;
    const char *file;
    int32_t line;
    char msg[256];
} err_t;
```

## Error Codes

| Code | Value | Usage |
|------|-------|-------|
| `OK` | 0 | Success |
| `ERR_INVALID_ARG` | 1 | Invalid argument |
| `ERR_OUT_OF_RANGE` | 2 | Out of range |
| `ERR_IO` | 3 | File/config operations |
| `ERR_PARSE` | 4 | JSON/protocol parsing |
| `ERR_DB_CONNECT` | 5 | DB connection |
| `ERR_DB_MIGRATE` | 6 | DB migration |
| `ERR_OUT_OF_MEMORY` | 7 | Allocation failure |
| `ERR_AGENT_NOT_FOUND` | 8 | Agent lookup |
| `ERR_PROVIDER` | 9 | Provider errors |
| `ERR_MISSING_CREDENTIALS` | 10 | Missing credentials |
| `ERR_NOT_IMPLEMENTED` | 11 | Not implemented |

## Macros

- `OK(value)` / `ERR(ctx, CODE, "msg", ...)` - Create results
- `TRY(expr)` - Extract value or return error
- `CHECK(res)` - Propagate error if failed
- `is_ok(&res)` / `is_err(&res)` - Inspect result

## Assertions & PANIC

- **Debug build**: asserts active → `SIGABRT` on failure
- **Release build** (`-DNDEBUG`): asserts compiled out → UB on violation
- Mark with `// LCOV_EXCL_BR_LINE`, test both paths, split compound assertions

```c
if (ptr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
default: PANIC("Invalid state");          // LCOV_EXCL_LINE
```

## Trust Boundary

- **User input** → validate with `res_t`, never crash
- **Internal** → `assert()` preconditions, trust caller validated

## Testing

- Assertions: `#ifndef NDEBUG` + `tcase_add_test_raise_signal(tc, test, SIGABRT)`
- PANIC: `tcase_add_test_raise_signal(tc, test, SIGABRT)` (all builds)
- OOM: Untestable (process terminates)

## Coverage Exclusions

| Code | Marker |
|------|--------|
| Assertions/OOM | `// LCOV_EXCL_BR_LINE` |
| PANIC logic | `// LCOV_EXCL_LINE` |

## Error Context Lifetime (Critical)

**THE TRAP:** Errors on a context that gets freed → use-after-free.

```c
// BROKEN
res_t result = ik_bar_init(foo, &foo->bar);  // Error on foo
if (is_err(&result)) {
    talloc_free(foo);  // FREES THE ERROR!
    return result;     // USE-AFTER-FREE
}
```

**FIX A (Preferred):** Pass parent for error allocation:
```c
res_t result = ik_bar_init(parent, &bar);  // Error on parent - survives!
if (is_err(&result)) return result;
foo_t *foo = talloc_zero_(parent, sizeof(foo_t));
talloc_steal(foo, bar);
```

**FIX B (Fallback):** Reparent error before free:
```c
if (is_err(&result)) {
    talloc_steal(parent, result.err);  // Save error
    talloc_free(foo);
    return result;
}
```

## References

`project/return_values.md`, `project/error_handling.md`, `project/error_patterns.md`
