# Error Handling and Defensive Programming

## Table of Contents

### Core Concepts
- [Philosophy: Three Mechanisms for Three Problems](#philosophy-three-mechanisms-for-three-problems)
  - [1. Result Types - Expected Runtime Errors](#1-result-types---expected-runtime-errors)
  - [2. Assertions - Development-Time Contracts](#2-assertions---development-time-contracts)
  - [3. PANIC() - Unrecoverable Logic Errors](#3-fatal---unrecoverable-logic-errors)

### Result Types
- [Result Types - Core API](#result-types---core-api)
- [Result Types - Macros](#result-types---macros)
- [Result Types - Memory Management](#result-types---memory-management)

### Error Mechanisms
- [Assertions: When to Use](#assertions-when-to-use)
  - [Purpose](#purpose)
  - [When to Assert](#when-to-assert)
- [FATAL() - Unrecoverable Logic Errors](#fatal---unrecoverable-logic-errors)
  - [Purpose](#purpose-1)
  - [The FATAL() Macro](#the-fatal-macro)
  - [When to Use FATAL()](#when-to-use-fatal)
  - [FATAL() vs assert()](#fatal-vs-assert)

### Decision Making
- [Decision Framework](#decision-framework)
- [Summary](#summary)

### Related Documentation
- **[return_values.md](return_values.md)** - Complete guide to function return patterns and how to use them
- **[error_patterns.md](error_patterns.md)** - Detailed patterns, best practices, and usage examples
- **[error_testing.md](error_testing.md)** - Testing strategy and coverage requirements

---

## Philosophy: Three Mechanisms for Three Problems

Ikigai uses three complementary mechanisms to handle different types of problems:

1. **`Result`** - Runtime error handling for expected failures
2. **`assert()`** - Development-time contract enforcement
3. **`FATAL()`** - Production crashes for unrecoverable logic errors

This creates a clear taxonomy of failure modes with appropriate responses for each.

---

### 1. Result Types - Expected Runtime Errors

**Use for:** IO operations, parsing, external failures (but NOT memory allocation)

**Characteristics:**
- Failures are unpredictable (disk full, network down, malformed input)
- Must be handled gracefully by caller
- Caller decides how to recover or propagate
- Always present in both debug and release builds

**Note:** Out of memory conditions are NOT handled via Result types - they cause PANIC (abort).

**Examples:**
```c
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment);
res_t config_load(TALLOC_CTX *ctx, const char *path);
res_t message_parse(TALLOC_CTX *ctx, const char *json);
```

**Operations that should return `res_t`:**
- File I/O, network operations
- Parsing user input or external data
- Resource acquisition (non-memory)

**Operations that should use PANIC:**
- Memory allocation failures (OOM)

---

### 2. Assertions - Development-Time Contracts

**Use for:** Preconditions, postconditions, invariants, programmer errors

**Characteristics:**
- Failures indicate bugs in YOUR code, not external conditions
- Fire immediately during development with clear messages
- Compile out in release builds (`-DNDEBUG`) - zero runtime cost
- Enable fearless refactoring with fast feedback

**Examples:**
```c
void ik_array_delete(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    // ... implementation
}
```

**What to assert:**
- NULL checks on pointer parameters
- Array bounds checks
- Valid state for operations
- Relationships between parameters (`start <= end`)
- Internal data structure consistency

---

### 3. PANIC() - Unrecoverable Errors

**Use for:** Out of memory, data corruption, impossible states

**Characteristics:**
- Failures indicate severe errors that cannot be recovered from
- Present in both debug and release builds
- Continuing would be more dangerous than crashing
- Used for OOM and logic errors (~1-2 per 1000 lines of code excluding OOM checks)

**Examples:**
```c
// Out of memory - always PANIC
void *ptr = talloc_zero_(ctx, size);
if (ptr == NULL) PANIC("Out of memory");

// Data corruption detected
if (array->size > array->capacity) {
    PANIC("Array corruption: size > capacity");
}

// Impossible state
switch (state) {
    case STATE_INIT: /* ... */ break;
    case STATE_READY: /* ... */ break;
    case STATE_DONE: /* ... */ break;
    default:
        PANIC("Invalid state in state machine");
}
```

**When to use PANIC():**
- Memory allocation failures (OOM) - most common case
- Data structure invariant violations detected at runtime
- Impossible state combinations
- Switch defaults that should never be reached
- Post-validation logic errors

---

## Result Types - Core API

**Result Structure:**
```c
typedef struct {
    union { void *ok; err_t *err; };
    bool is_err;
} res_t;
```

**Error Structure:**
```c
typedef struct err {
    err_code_t code;
    const char *file;
    int32_t line;
    char message[256];
} err_t;
```

**Error Codes:** Start empty, add organically as needed
```c
typedef enum {
    OK = 0,
    ERR_INVALID_ARG, ERR_OUT_OF_RANGE,
    ERR_IO, ERR_PARSE
} err_code_t;
```

**Note:** ERR_OOM has been removed - memory allocation failures now cause PANIC instead.

**Inspection Functions:**
```c
bool is_ok(const res_t *result);
bool is_err(const res_t *result);
err_code_t error_code(const err_t *err);
const char *error_message(const err_t *err);
void error_fprintf(FILE *f, const err_t *err);
```

**When to Return `void` vs `res_t`:**

A function must return `void` (not `res_t`) if:
1. It performs no IO operations (no file/network/allocation that can fail)
2. It returns only information the caller already has (like echoing back a pointer parameter)
3. All meaningful results are communicated via output parameters

**Rule: Functions that don't perform IO and only return information the caller already has must return `void`.**

---

## Result Types - Macros

**Creating Results:**
```c
return OK(value);                              // Success
return ERR(ctx, IO, "Cannot open: %s", path);  // Error (allocates on ctx)
```

**Propagating Errors:**
```c
// CHECK - propagate full result (when you don't need the value immediately)
res_t res = config_load(ctx, path);
CHECK(res);
ik_cfg_t *config = res.ok;

// TRY - extract value or propagate error (for inline use)
char *path = TRY(expand_tilde(ctx, "~/.config"));  // Cleaner!
ik_cfg_t *config = TRY(config_load(ctx, path));
```

**When to use CHECK vs TRY:**
- `CHECK`: When you need to inspect the result before using it
- `TRY`: When you just want the value (most common case)
- Both return early on error

---

## Result Types - Memory Management

**Ownership Rules:**
1. Caller provides `TALLOC_CTX *ctx`
2. Results (success values and errors) allocated as children of `ctx`
3. Caller frees context - single `talloc_free(ctx)` cleans everything

**Lifecycle Example:**
```c
TALLOC_CTX *root = talloc_new(NULL);

ik_cfg_t *config = TRY(config_load(root, "~/.ikigai/config.json"));
int result = server_run(root, config);

talloc_free(root);  // Frees config, server state, everything
return result;
```

**Per-Request Context Pattern:**
```c
int websocket_callback(request, response, user_data) {
    ws_connection_t *conn = user_data;
    TALLOC_CTX *msg_ctx = talloc_new(conn->ctx);

    message_t *msg = TRY(message_parse(msg_ctx, json_data));
    // ... process ...

    talloc_free(msg_ctx);  // Frees msg and all allocations
    return 0;
}
```

---

### Error Context Lifetime (Critical)

**WARNING:** Errors allocated on a freed context cause use-after-free bugs.

**Problem:**
```c
res_t ik_foo_init(void *parent, foo_t **out) {
    foo_t *foo = talloc_zero_(parent, sizeof(foo_t));
    res_t result = ik_bar_init(foo, &foo->bar);  // Error on foo
    if (is_err(&result)) {
        talloc_free(foo);  // FREES ERROR TOO - use-after-free!
        return result;
    }
}
```

**Fix A (Preferred):** Pass parent context to sub-functions:
```c
res_t ik_foo_init(void *parent, foo_t **out) {
    res_t result = ik_bar_init(parent, NULL);  // Error on parent
    if (is_err(&result)) return result;  // Safe - parent survives
    foo_t *foo = talloc_zero_(parent, sizeof(foo_t));
    // ...
    return OK(foo);
}
```

**Fix B:** Reparent error before freeing:
```c
if (is_err(&result)) {
    talloc_steal(parent, result.err);  // Survive free
    talloc_free(foo);
    return result;
}
```

**Rule:** Error context must outlive the error. Either allocate on parent (A) or use `talloc_steal` before freeing (B).

---

## Assertions: When to Use

### Purpose

Assertions are **development-time contract enforcement mechanisms**. They:

1. **Accelerate development** - Catch bugs immediately with clear error messages
2. **Document contracts** - Make preconditions, postconditions, and invariants explicit
3. **Enable fearless refactoring** - Changes that violate contracts fail fast in tests
4. **Prevent cascading failures** - Stop bad calls from causing confusing downstream errors

**Assertions compile out in release builds (`-DNDEBUG`)** - they provide zero runtime overhead in production.

### When to Assert

**Assert liberally.** Since assertions compile out in release builds, there is no cost to being thorough.

#### Preconditions (Function Inputs)

```c
void ik_array_delete(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    // ... implementation
}
```

#### Invariants (Internal Consistency)

```c
void process_buffer(buffer_t *buf) {
    assert(buf != NULL);                    // LCOV_EXCL_BR_LINE
    assert(buf->data != NULL);              // LCOV_EXCL_BR_LINE
    assert(buf->size <= buf->capacity);     // LCOV_EXCL_BR_LINE
    // ... do work ...
}
```

#### Postconditions (Function Outputs)

```c
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment) {
    assert(ctx != NULL);                // LCOV_EXCL_BR_LINE
    assert(element_size > 0);           // LCOV_EXCL_BR_LINE

    ik_array_t *array = talloc_zero_(ctx, sizeof(ik_array_t));
    if (!array) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    array->size = 0;
    array->capacity = increment;

    assert(array->size == 0);           // LCOV_EXCL_BR_LINE
    assert(array->capacity > 0);        // LCOV_EXCL_BR_LINE

    return OK(array);
}
```

**See [error_patterns.md](error_patterns.md) for detailed best practices and testing strategies.**

---

## PANIC() - Unrecoverable Errors

### Purpose

Some errors are so severe that continuing would be more dangerous than crashing. Out of memory is the most common case, but data corruption and impossible states also require immediate termination. For these cases, use `PANIC()`.

### The PANIC() Macro

**Location:** `src/panic.h`

```c
#define PANIC(msg) ik_panic_impl((msg), __FILE__, __LINE__)
```

The implementation handles terminal restoration and provides async-signal-safe error reporting before calling `abort()`.

### When to Use PANIC()

**Most common:** Out of memory conditions - every memory allocation checks for NULL and calls PANIC.

**Rare:** Logic errors indicating corruption (~1-2 per 1000 lines of code, excluding OOM checks).

#### ✅ Good Candidates:

**1. Out of memory (most common):**
```c
void *ptr = talloc_zero_(ctx, size);
if (ptr == NULL) {
    PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
}
```

**2. Data structure corruption detected:**
```c
if (array->size > array->capacity) {
    PANIC("Array corruption: size > capacity");  // LCOV_EXCL_LINE
}
```

**3. Impossible state combinations:**
```c
if (state == STATE_CLOSED && fd >= 0) {
    PANIC("Inconsistent state: closed but fd valid");  // LCOV_EXCL_LINE
}
```

**4. Switch defaults (always):**
```c
switch (action.type) {
    case ACTION_INSERT: /* ... */ break;
    case ACTION_DELETE: /* ... */ break;
    case ACTION_MOVE: /* ... */ break;
    default:
        PANIC("Invalid action type in switch");  // LCOV_EXCL_LINE
}
```

#### ❌ Don't Use PANIC() For:

**1. Precondition checks** - Use `assert()`:
```c
// Bad
if (ptr == NULL) PANIC("NULL pointer");

// Good
assert(ptr != NULL);  // LCOV_EXCL_BR_LINE
```

**2. Expected errors** - Use `Result`:
```c
// Bad
if (file_open_failed) PANIC("Can't open file");

// Good
return ERR(ctx, IO, "Cannot open file: %s", path);
```

### PANIC() vs assert()

**Key distinction:** assert() compiles out in release builds (`-DNDEBUG`), PANIC() is always present.

**Use `assert()` for:**
- Precondition checks - Caller's responsibility
- Contract violations - Bugs in how functions are called
- Development-time verification

**Use `PANIC()` for:**
- Out of memory - Cannot continue without memory
- Unreachable code - Switch defaults, impossible states
- Data structure corruption - Invariants violated at runtime
- Internal logic errors - Conditions that should be impossible

**Critical principle:** If reaching a code location means the program is in an undefined/corrupted state, use `PANIC()`. Never let the program continue in an unknown state.

---

## Decision Framework

When something goes wrong, use this decision tree:

### 1. Is this an out of memory condition?
   - **Yes** → Use `PANIC()` - memory allocation failures cannot be recovered from
   - **No** → Continue to #2

### 2. Can this happen with correct code and valid input?
   - **Yes** → Use `Result` (e.g., file not found, network timeout, parse error)
   - **No** → Continue to #3

### 3. Is this a precondition / function contract violation?
   - **Yes** → Use `assert()` (e.g., NULL pointer passed by caller, out-of-bounds index)
   - **No** → Continue to #4

### 4. Is this unreachable code or impossible internal state?
   - **Yes** → Use `PANIC()` (e.g., switch defaults, enum values after validation, corrupted data structures)
   - **No** → Reconsider - you may have a precondition (step 3) or expected error (step 2)

**Key insight:** Memory allocation failures always cause PANIC. If you reach code that should be impossible to reach, that's also `PANIC()` territory. The program is in an undefined state and must terminate immediately, even in production.

### Quick Reference

| Situation | Mechanism | Reason |
|-----------|-----------|--------|
| User passed NULL | `Result` | Expected error - validate at boundary |
| Internal function received NULL | `assert()` | Contract violation - caller's bug |
| File not found | `Result` | Expected runtime error |
| Out of memory | `PANIC()` | Cannot continue without memory - abort immediately |
| Array size > capacity | `PANIC()` | Data corruption - unrecoverable |
| Invalid enum after validation | `PANIC()` | Logic error - should be impossible |
| NULL pointer check | `assert()` | Precondition - development aid |
| Bounds check | `assert()` | Precondition - development aid |
| Switch default | `PANIC()` | Should never be reached |

---

## Summary

### Three Mechanisms

1. **`Result`** - Runtime errors
   - Expected failures (IO, resources, user input)
   - Must be handled by caller
   - Clear error messages

2. **`assert()`** - Development-time contracts
   - Compiles out in release builds (`-DNDEBUG`)
   - Fast feedback during development
   - Use liberally - zero cost

3. **`PANIC()`** - Unrecoverable errors
   - Production crashes for OOM and corruption
   - OOM checks throughout codebase, plus rare logic errors (~1-2 per 1000 LOC)
   - When continuing is more dangerous than crashing

### Key Principles

- **Assert liberally** - Zero cost, huge development value
- **Validate exhaustively** at trust boundaries
- **Crash explicitly** when corruption detected
- **Handle errors gracefully** when recovery is possible

**For detailed patterns and examples**, see [error_patterns.md](error_patterns.md).

**For testing strategies and coverage requirements**, see [error_testing.md](error_testing.md).

---

**Principle:** Make errors explicit. Make contracts visible. Make corruption impossible to ignore.
