# Defensive Programming Guide

## Overview

Ikigai uses three complementary mechanisms to handle different types of problems:

1. **`assert()`** - Development-time contract enforcement (compiles out with `-DNDEBUG`)
2. **`Result`** - Runtime error handling for expected failures (IO, resource exhaustion)
3. **`ABORT()`** - Production crashes for unrecoverable logic errors

This document explains when to use each mechanism and how they work together.

---

## Assertions

### Purpose

Assertions in ikigai are **development-time contract enforcement mechanisms**. They exist to:

1. **Accelerate development** - Catch bugs immediately with clear error messages
2. **Document contracts** - Make preconditions, postconditions, and invariants explicit
3. **Enable fearless refactoring** - Changes that violate contracts fail fast in tests
4. **Prevent cascading failures** - Stop bad calls from causing confusing downstream errors

**Assertions compile out in release builds (`-DNDEBUG`)** - they provide zero runtime overhead in production.

### Philosophy

Assertions are **not** production safety mechanisms. They are development aids that:
- Help you know immediately when you use a function incorrectly
- Serve as executable documentation of function contracts
- Provide fast feedback during development and testing
- Catch integration bugs before they cause mysterious failures

Think of assertions as **extensions to the type system** - documenting and enforcing constraints that C's type system cannot express (non-NULL, in-bounds, valid state).

### When to Assert

**Assert liberally.** Since assertions compile out in release builds, there is no cost to being thorough.

#### Preconditions (Function Inputs)

Assert all preconditions at function entry:

```c
void ik_array_delete(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    // ... implementation
}
```

**What to check:**
- Pointer parameters are non-NULL (unless explicitly nullable)
- Indices are within valid ranges
- State is valid for the operation
- Relationships between parameters (`start <= end`)

#### Invariants (Internal Consistency)

Assert internal consistency throughout functions:

```c
void process_buffer(buffer_t *buf) {
    assert(buf != NULL);                    // LCOV_EXCL_BR_LINE
    assert(buf->data != NULL);              // LCOV_EXCL_BR_LINE
    assert(buf->size <= buf->capacity);     // LCOV_EXCL_BR_LINE

    // ... do work ...

    // Recheck critical invariants after modifications
    assert(buf->size <= buf->capacity);     // LCOV_EXCL_BR_LINE
}
```

#### Postconditions (Function Outputs)

Assert outcomes before returning:

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

#### Anomalous Conditions

Assert things that should never happen:

```c
switch (state) {
    case STATE_INIT: /* ... */ break;
    case STATE_READY: /* ... */ break;
    case STATE_DONE: /* ... */ break;
    default:
        assert(false);  // LCOV_EXCL_BR_LINE - should never reach
        break;
}
```

### Assertion Best Practices

#### 1. Assert Both Sides

Don't just assert what **must be** - also assert what **must never be**:

```c
assert(count > 0);          // Must be positive
assert(count <= MAX_SIZE);  // Must not exceed limit
```

#### 2. Split Compound Assertions

Prefer separate assertions for clearer failure messages:

```c
// Good - shows which condition failed
assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
assert(array != NULL);      // LCOV_EXCL_BR_LINE

// Bad - can't tell which pointer is NULL
assert(ctx != NULL && array != NULL);  // LCOV_EXCL_BR_LINE
```

#### 3. Every Public Function Asserts Context

```c
assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
```

This should be the first line of every function taking a talloc context.

#### 4. Array Access Always Bounds-Checked

```c
void *ik_array_get(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    return (char *)array->data + (index * array->element_size);
}
```

#### 5. Assert Side-Effect Free

Never put functional code in assertions:

```c
// Bad - increments only in debug builds!
assert(count++ < MAX);

// Good - assert doesn't change behavior
count++;
assert(count <= MAX);  // LCOV_EXCL_BR_LINE
```

#### 6. Use Implications with If-Statements

For logical implications, use readable if-statements:

```c
// Good - clear and readable
if (has_data) assert(buffer != NULL);  // LCOV_EXCL_BR_LINE

// Bad - requires mental translation
assert(!has_data || buffer != NULL);  // LCOV_EXCL_BR_LINE
```

### Testing Assertions

All assertions must be tested to verify they fire correctly when contracts are violated.

#### Test Both Paths

1. **Normal path** - assertion passes
2. **Violation path** - assertion fires (SIGABRT)

```c
// Test 1: Normal usage - assertion passes
START_TEST(test_array_get_valid) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int), 10);
    ik_array_t *array = unwrap(&res);

    int value = 42;
    ik_array_append(array, &value);

    int *result = ik_array_get(array, 0);  // Valid - passes
    ck_assert_int_eq(*result, 42);

    talloc_free(ctx);
}
END_TEST

// Test 2: Contract violation - assertion fires
#ifndef NDEBUG
START_TEST(test_array_get_null_array_asserts) {
    ik_array_get(NULL, 0);  // NULL array - fires SIGABRT
}
END_TEST

START_TEST(test_array_get_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int), 10);
    ik_array_t *array = unwrap(&res);

    ik_array_get(array, 0);  // Out of bounds (size=0) - fires SIGABRT

    talloc_free(ctx);
}
END_TEST
#endif

// Register tests in suite
void array_suite(void) {
    Suite *s = suite_create("Array");
    TCase *tc = tcase_create("Core");

    // Normal path
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_array_get_valid);

    // Assertion violation tests (debug only)
#ifndef NDEBUG
    tcase_add_test_raise_signal(tc, test_array_get_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc, test_array_get_out_of_bounds_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc);
}
```

#### Why Test Assertions?

Since assertions compile out in release builds, you might wonder why test them at all:

1. **Verify guard rails work** - Assertions should fire when you expect
2. **Prevent silent removal** - If someone removes an assertion, test fails
3. **Document contracts** - Tests show what violations look like
4. **Catch development bugs** - Prove that invalid calls fail fast in debug builds

### Coverage Requirements

All assertions must be excluded from branch coverage, but both paths must be tested:

1. Mark with `// LCOV_EXCL_BR_LINE` comment
2. Test normal path (assertion passes)
3. Test violation path (assertion fires SIGABRT)

```c
void ik_array_delete(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    // ... implementation
}
```

**Rationale:** Assertions create branches that don't exist in release builds. We exclude them from coverage metrics but still verify they work correctly through SIGABRT tests.

See [error_handling.md](error_handling.md#coverage-requirements-for-assertions) for detailed coverage policy.

---

## ABORT() - Unrecoverable Logic Errors

### Purpose

While assertions compile out in release builds, some logic errors indicate such severe corruption that continuing would be more dangerous than crashing. For these rare cases, use `ABORT()`.

### The ABORT() Macro

**Location:** `src/abort.h`

```c
#ifndef IK_ABORT_H
#define IK_ABORT_H

#include <stdio.h>
#include <stdlib.h>

// Abort with formatted message, file, and line
#define ABORT(fmt, ...) \
    do { \
        fprintf(stderr, "FATAL: " fmt "\n  at %s:%d\n", \
                ##__VA_ARGS__, __FILE__, __LINE__); \
        fflush(stderr); \
        abort(); \
    } while(0)

// For unreachable code paths (switch defaults, after exhaustive if/else)
#define UNREACHABLE() \
    ABORT("Unreachable code executed")

#endif
```

### When to Use ABORT()

**Use `ABORT()` sparingly** - approximately 1-2 per 1000 lines of code.

#### ✅ Good Candidates for ABORT():

**1. Data structure corruption detected:**
```c
void resize_array(array_t *array) {
    if (array->size > array->capacity) {
        // Fundamental invariant violated - data structure corrupted
        ABORT("Array corruption: size=%zu > capacity=%zu",
              array->size, array->capacity);
    }
    // ...
}
```

**2. Impossible state combinations:**
```c
if (state == STATE_CLOSED && fd >= 0) {
    // Logically impossible - closed state should never have valid fd
    ABORT("Inconsistent state: STATE_CLOSED but fd=%d", fd);
}
```

**3. Switch defaults after validation:**
```c
// Enum value was validated earlier in call stack
switch (action.type) {
    case ACTION_INSERT: /* ... */ break;
    case ACTION_DELETE: /* ... */ break;
    case ACTION_MOVE: /* ... */ break;
    default:
        // Already validated, so this indicates logic error
        UNREACHABLE();
}
```

**4. Post-validation logic errors:**
```c
// After extensive validation of file format, structure, checksums, etc.
if (/* condition that validation should have prevented */) {
    ABORT("Validation passed but invariant violated");
}
```

#### ❌ Don't Use ABORT() For:

**1. Precondition checks** - Use `assert()`:
```c
// Bad
if (ptr == NULL) ABORT("NULL pointer");

// Good
assert(ptr != NULL);  // LCOV_EXCL_BR_LINE
```

**2. Expected errors** - Use `Result`:
```c
// Bad
if (file_open_failed) ABORT("Can't open file");

// Good
return ERR(ctx, IO, "Cannot open file: %s", path);
```

**3. Rare but possible cases** - Handle gracefully:
```c
// Bad - race conditions, external modifications can cause this
if (file_size_changed) ABORT("File size changed");

// Good
return ERR(ctx, IO, "File was modified during read");
```

### ABORT() vs assert()

**Key question:** *"If this happens in production, is continuing more dangerous than crashing?"*

- **Data corruption risk?** → `ABORT()`
- **Security boundary violation?** → `ABORT()`
- **Precondition check?** → `assert()`
- **Defensive programming?** → `assert()`

**Rule of thumb:** If you're debating whether to use `ABORT()` or `assert()`, choose `assert()`. Use `ABORT()` only for truly unrecoverable corruption.

---

## User Input Validation

### The Trust Boundary

Functions that accept user input are the **trust boundary**. These functions are responsible for exhaustive validation and must never crash on bad input.

```
┌─────────────────────────────────────┐
│  User Input (untrusted boundary)    │
├─────────────────────────────────────┤
│  Input Validation Layer             │  ← Result types, exhaustive checks
│  - parse_command()                  │  ← Never assert on input content
│  - validate_filepath()              │  ← Handle all possible bad input
│  - parse_json()                     │
├─────────────────────────────────────┤
│  Internal Functions                 │  ← assert() on contracts
│  - process_validated_command()      │  ← Can assert preconditions
│  - array_operations()               │  ← Trust caller validated
├─────────────────────────────────────┤
│  Deep Logic                         │  ← ABORT() on impossible states
│  - state_machine_transition()      │  ← Validated but now impossible?
│  - switch defaults after enum check │
└─────────────────────────────────────┘
```

### User Input Functions Must:

1. **Validate exhaustively** - Account for ALL possible bad input
2. **Return Result types** - Never crash on bad input
3. **Provide clear error messages** - Help users fix their mistakes
4. **Never assert on input content** - Only assert on internal preconditions

### Pattern: Trust Boundary

```c
// Public API - accepts untrusted user input
res_t handle_user_command(TALLOC_CTX *ctx, const char *cmd) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE - our precondition, not user input

    // Validate ALL possible bad input
    if (cmd == NULL) {
        return ERR(ctx, INVALID_ARG, "Command cannot be NULL");
    }

    if (strlen(cmd) == 0) {
        return ERR(ctx, INVALID_ARG, "Command cannot be empty");
    }

    if (strlen(cmd) > MAX_COMMAND_LENGTH) {
        return ERR(ctx, INVALID_ARG, "Command too long (max %d chars)",
                   MAX_COMMAND_LENGTH);
    }

    if (!is_valid_format(cmd)) {
        return ERR(ctx, PARSE, "Invalid command format: %s", cmd);
    }

    // Now pass validated data to internal function
    return process_command(ctx, cmd);
}

// Internal - trusts caller validated input
static res_t process_command(TALLOC_CTX *ctx, const char *cmd) {
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(cmd != NULL);              // LCOV_EXCL_BR_LINE
    assert(strlen(cmd) > 0);          // LCOV_EXCL_BR_LINE
    assert(strlen(cmd) <= MAX_COMMAND_LENGTH);  // LCOV_EXCL_BR_LINE

    // Can assert because caller validated
    // ...
}
```

### What Constitutes "User Input"?

User input includes:
- Command-line arguments
- Terminal input (keystrokes, escape sequences)
- File contents being parsed
- Environment variables
- Configuration files
- Network data (future)

**Rule:** If it comes from outside your process boundaries, validate exhaustively.

### Examples

#### ❌ Bad - Asserting on user input:
```c
res_t parse_user_file(TALLOC_CTX *ctx, const char *path) {
    assert(path != NULL);           // BAD - user provides path
    assert(strlen(path) <= 1000);   // BAD - asserting on user input

    FILE *f = fopen(path, "r");
    assert(f != NULL);              // BAD - file might not exist (user error)
    // ...
}
```

#### ✅ Good - Validating user input:
```c
res_t parse_user_file(TALLOC_CTX *ctx, const char *path) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE - our precondition

    // Validate user input - return errors, don't assert
    if (path == NULL) {
        return ERR(ctx, INVALID_ARG, "File path cannot be NULL");
    }

    if (strlen(path) == 0) {
        return ERR(ctx, INVALID_ARG, "File path cannot be empty");
    }

    if (strlen(path) > MAX_PATH) {
        return ERR(ctx, INVALID_ARG, "File path too long");
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return ERR(ctx, IO, "Cannot open file: %s", path);
    }

    // Now pass validated data to internal parser
    return parse_file_content(ctx, f);
}
```

### Principle

**Assertions are for contracts between YOUR functions, not for validating external input.**

---

## Decision Framework

When something goes wrong, use this decision tree:

### 1. Can this happen with correct code and valid input?
   - **Yes** → Use `Result` (e.g., file not found, out of memory)
   - **No** → Continue to #2

### 2. Is this a precondition / function contract?
   - **Yes** → Use `assert()` (e.g., NULL check, bounds check)
   - **No** → Continue to #3

### 3. Would continuing cause data corruption or security issues?
   - **Yes** → Use `ABORT()` (e.g., state machine corruption, invariant violation)
   - **No** → Use `assert()` (trust testing to catch it)

### 4. Is this "impossible" code path after validation?
   - **Yes** → Use `ABORT()` or `UNREACHABLE()` (e.g., switch default after enum validation)
   - **No** → Use `assert()`

### Quick Reference

| Situation | Mechanism | Reason |
|-----------|-----------|--------|
| User passed NULL | `Result` | Expected error - validate at boundary |
| Internal function received NULL | `assert()` | Contract violation - caller's bug |
| File not found | `Result` | Expected runtime error |
| Out of memory | `Result` | Expected runtime error |
| Array size > capacity | `ABORT()` | Data corruption - unrecoverable |
| Invalid enum after validation | `ABORT()` | Logic error - should be impossible |
| NULL pointer check | `assert()` | Precondition - development aid |
| Bounds check | `assert()` | Precondition - development aid |
| Switch default (unvalidated) | `assert(false)` | Defensive programming |
| Switch default (validated) | `UNREACHABLE()` | Logic error if reached |

---

## Summary

### Three Mechanisms

1. **`assert()`** - Development-time contracts
   - Compiles out in release builds (`-DNDEBUG`)
   - Fast feedback during development
   - Use liberally - zero cost

2. **`Result`** - Runtime errors
   - Expected failures (IO, resources, user input)
   - Must be handled by caller
   - Clear error messages

3. **`ABORT()`** - Unrecoverable logic errors
   - Production crashes for corruption
   - Use sparingly (~1-2 per 1000 LOC)
   - When continuing is more dangerous than crashing

### Key Principles

- **Assert liberally** - Zero cost, huge development value
- **Validate exhaustively** at trust boundaries
- **Crash explicitly** when corruption detected
- **Handle errors gracefully** when recovery is possible

### Testing

- Test assertion violations (SIGABRT)
- Test error paths (Result types)
- Mark assertions with `// LCOV_EXCL_BR_LINE`

Defensive programming makes development faster, code more reliable, and bugs easier to catch.
