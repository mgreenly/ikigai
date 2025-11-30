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

## Return Patterns

1. **res_t** - Failable operations (IO, parsing)
2. **Direct pointer** - Simple creation (PANICs on OOM)
3. **void** - Cannot fail
4. **Primitive** - Queries (size, bool checks)
5. **Raw pointer** - Into buffer (use `_ptr` suffix)
6. **Callback** - res_t for events, value for queries, void for side-effects

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

## Macros

- `OK(value)` / `ERR(ctx, CODE, "msg", ...)` - Create results
- `TRY(expr)` - Extract value or return error
- `CHECK(res)` - Propagate error if failed
- `is_ok(&res)` / `is_err(&res)` - Inspect

## Assertions

- Mark with `// LCOV_EXCL_BR_LINE`
- Test both paths (pass + SIGABRT)
- Split compound assertions
- Assert side-effect free

## PANIC Usage

```c
// OOM - most common
if (ptr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

// Switch default
default: PANIC("Invalid state");  // LCOV_EXCL_LINE

// Corruption
if (size > capacity) PANIC("Array corruption");  // LCOV_EXCL_LINE
```

## Trust Boundary

- **User input** → validate exhaustively with `res_t`, never crash
- **Internal functions** → `assert()` preconditions, trust caller validated

## Testing

- Assertions: `#ifndef NDEBUG` + `tcase_add_test_raise_signal(tc, test, SIGABRT)`
- PANIC: `tcase_add_test_raise_signal(tc, test, SIGABRT)` (all builds)
- OOM: Cannot be tested (process terminates)

## Coverage Exclusions

| Code | Marker |
|------|--------|
| Assertions | `// LCOV_EXCL_BR_LINE` |
| OOM checks | `// LCOV_EXCL_BR_LINE` |
| PANIC logic errors | `// LCOV_EXCL_LINE` |

New exclusions require updating `LCOV_EXCL_COVERAGE` in Makefile.

## References

Full details: `docs/return_values.md`, `docs/error_handling.md`, `docs/error_patterns.md`, `docs/error_testing.md`
