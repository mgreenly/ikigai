# Error Handling and Defensive Programming

## Table of Contents

### Core Concepts
- [Philosophy: Three Mechanisms for Three Problems](#philosophy-three-mechanisms-for-three-problems)
  - [1. Result Types - Expected Runtime Errors](#1-result-types---expected-runtime-errors)
  - [2. Assertions - Development-Time Contracts](#2-assertions---development-time-contracts)
  - [3. FATAL() - Unrecoverable Logic Errors](#3-fatal---unrecoverable-logic-errors)

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

**Use for:** IO operations, resource allocation, parsing - any external failure

**Characteristics:**
- Failures are unpredictable (OOM, disk full, network down, malformed input)
- Must be handled gracefully by caller
- Caller decides how to recover or propagate
- Always present in both debug and release builds

**Examples:**
```c
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment);
res_t config_load(TALLOC_CTX *ctx, const char *path);
res_t message_parse(TALLOC_CTX *ctx, const char *json);
```

**Operations that should return `res_t`:**
- Heap allocation
- File I/O, network operations
- Parsing user input or external data
- Resource acquisition

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

### 3. FATAL() - Unrecoverable Logic Errors

**Use for:** Data corruption, impossible states that should never occur

**Characteristics:**
- Failures indicate severe logic errors or corruption
- Present in both debug and release builds
- Continuing would be more dangerous than crashing
- Should be extremely rare (~1-2 per 1000 lines of code)

**Examples:**
```c
if (array->size > array->capacity) {
    FATAL("Array corruption: size > capacity");
}

switch (state) {
    case STATE_INIT: /* ... */ break;
    case STATE_READY: /* ... */ break;
    case STATE_DONE: /* ... */ break;
    default:
        FATAL("Invalid state in state machine");
}
```

**When to use FATAL():**
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
    char message[128];
} err_t;
```

**Error Codes:** Start empty, add organically as needed
```c
typedef enum {
    OK = 0,
    ERR_OOM, ERR_INVALID_ARG, ERR_OUT_OF_RANGE,
    ERR_IO, ERR_PARSE
} err_code_t;
```

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

    ik_array_t *array = /* ... allocate ... */;
    if (!array) return ERR(ctx, OOM, "Failed to allocate array");

    array->size = 0;
    array->capacity = increment;

    assert(array->size == 0);           // LCOV_EXCL_BR_LINE
    assert(array->capacity > 0);        // LCOV_EXCL_BR_LINE

    return OK(array);
}
```

**See [error_patterns.md](error_patterns.md) for detailed best practices and testing strategies.**

---

## FATAL() - Unrecoverable Logic Errors

### Purpose

While assertions compile out in release builds, some logic errors indicate such severe corruption that continuing would be more dangerous than crashing. For these rare cases, use `FATAL()`.

### The FATAL() Macro

**Location:** `src/fatal.h`

```c
#define FATAL(msg) \
    do { \
        fprintf(stderr, "FATAL: %s\n  at %s:%d\n", \
                (msg), __FILE__, __LINE__); \
        fflush(stderr); \
        abort(); \
    } while(0)
```

### When to Use FATAL()

**Use sparingly** - approximately 1-2 per 1000 lines of code.

#### ✅ Good Candidates:

**1. Data structure corruption detected:**
```c
if (array->size > array->capacity) {
    FATAL("Array corruption: size > capacity");  // LCOV_EXCL_LINE
}
```

**2. Impossible state combinations:**
```c
if (state == STATE_CLOSED && fd >= 0) {
    FATAL("Inconsistent state: closed but fd valid");  // LCOV_EXCL_LINE
}
```

**3. Switch defaults (always):**
```c
switch (action.type) {
    case ACTION_INSERT: /* ... */ break;
    case ACTION_DELETE: /* ... */ break;
    case ACTION_MOVE: /* ... */ break;
    default:
        FATAL("Invalid action type in switch");  // LCOV_EXCL_LINE
}
```

#### ❌ Don't Use FATAL() For:

**1. Precondition checks** - Use `assert()`:
```c
// Bad
if (ptr == NULL) FATAL("NULL pointer");

// Good
assert(ptr != NULL);  // LCOV_EXCL_BR_LINE
```

**2. Expected errors** - Use `Result`:
```c
// Bad
if (file_open_failed) FATAL("Can't open file");

// Good
return ERR(ctx, IO, "Cannot open file: %s", path);
```

### FATAL() vs assert()

**Key distinction:** assert() compiles out in release builds (`-DNDEBUG`), FATAL() is always present.

**Use `assert()` for:**
- Precondition checks - Caller's responsibility
- Contract violations - Bugs in how functions are called
- Development-time verification

**Use `FATAL()` for:**
- Unreachable code - Switch defaults, impossible states
- Data structure corruption - Invariants violated at runtime
- Internal logic errors - Conditions that should be impossible

**Critical principle:** If reaching a code location means the program is in an undefined/corrupted state, use `FATAL()`. Never let the program continue in an unknown state.

---

## Decision Framework

When something goes wrong, use this decision tree:

### 1. Can this happen with correct code and valid input?
   - **Yes** → Use `Result` (e.g., file not found, out of memory, network timeout)
   - **No** → Continue to #2

### 2. Is this a precondition / function contract violation?
   - **Yes** → Use `assert()` (e.g., NULL pointer passed by caller, out-of-bounds index)
   - **No** → Continue to #3

### 3. Is this unreachable code or impossible internal state?
   - **Yes** → Use `FATAL()` (e.g., switch defaults, enum values after validation, corrupted data structures)
   - **No** → Reconsider - you may have a precondition (step 2) or expected error (step 1)

**Key insight:** If you reach code that should be impossible to reach, that's `FATAL()` territory. The program is in an undefined state and must terminate immediately, even in production.

### Quick Reference

| Situation | Mechanism | Reason |
|-----------|-----------|--------|
| User passed NULL | `Result` | Expected error - validate at boundary |
| Internal function received NULL | `assert()` | Contract violation - caller's bug |
| File not found | `Result` | Expected runtime error |
| Out of memory | `Result` | Expected runtime error |
| Array size > capacity | `FATAL()` | Data corruption - unrecoverable |
| Invalid enum after validation | `FATAL()` | Logic error - should be impossible |
| NULL pointer check | `assert()` | Precondition - development aid |
| Bounds check | `assert()` | Precondition - development aid |
| Switch default | `FATAL()` | Should never be reached |

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

3. **`FATAL()`** - Unrecoverable logic errors
   - Production crashes for corruption
   - Use sparingly (~1-2 per 1000 LOC)
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
