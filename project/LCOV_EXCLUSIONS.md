# LCOV Coverage Exclusions Documentation

This document explains why certain code blocks in the ikigai codebase are legitimately excluded from code coverage analysis using `LCOV_EXCL_START/STOP` markers.

## Overview

While we maintain 80% test coverage per file as our requirement, there are specific categories of code that are fundamentally untestable in a unit test environment. These exclusions are documented here to justify their necessity.

## Legitimate Exclusion Categories

### 1. Process-Terminating Panic Handlers

**File:** `src/panic.c` (entire file, lines 1-142)

**Reason:** Panic handlers are designed to abort the process immediately when catastrophic errors occur. They cannot be tested without terminating the test process itself.

**Details:**
- The panic infrastructure provides signal-safe error reporting before process termination
- Testing these functions would require actually aborting the test runner
- The code contains manual string handling and syscall wrappers that must be async-signal-safe
- These functions are only invoked in truly exceptional circumstances (e.g., out of memory, corrupt data structures)

**Code Example:**
```c
// LCOV_EXCL_START - Panic handlers cannot be tested (they abort the process)
void ik_panic(const char *message) {
    /* Signal-safe error output */
    write(STDERR_FILENO, message, safe_strlen(message));
    abort();  /* Process terminates here */
}
// LCOV_EXCL_STOP
```

**Justification:** Process termination is the intended behavior, making these inherently untestable.

---

### 2. Test-Only Wrapper Functions

**File:** `src/wrapper.c` (lines 11-327, ~300 lines)

**Reason:** These wrapper functions are only compiled in debug/test builds and exist specifically to enable failure injection during testing. They are tested indirectly through the code that uses them.

**Details:**
- Wrappers for external library functions (talloc, curl, yyjson, POSIX APIs)
- In release builds (`NDEBUG`), these are defined as static inline direct calls
- In debug builds, they're weak symbols that tests can override for failure injection
- Every wrapper is exercised by the actual code that calls it during normal operation

**Code Example:**
```c
#ifndef NDEBUG
// LCOV_EXCL_START
MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size) {
    return talloc_zero_size(ctx, size);
}
// LCOV_EXCL_STOP
#endif
```

**Justification:** These wrappers are tested via the production code that uses them. Testing the wrappers directly would be redundant since every call site is already covered.

---

### 3. OS Signal Handlers

**File:** `src/signal_handler.c` (lines 17-24, 40-50)

**Reason:** Signal handlers require actual OS signal delivery, which is difficult to reliably trigger and test in a unit test environment.

**Details:**
- `handle_sigwinch()`: SIGWINCH handler for terminal resize events
- `ik_signal_check_resize()`: Checks if resize signal was received
- The core resize logic (`ik_repl_handle_resize()`) IS fully tested in `repl_resize_test.c`
- Only the signal delivery mechanism itself is excluded

**Code Example:**
```c
// LCOV_EXCL_START
static void handle_sigwinch(int sig) {
    (void)sig;
    g_resize_pending = 1;  /* Set flag atomically */
}
// LCOV_EXCL_STOP
```

**Justification:** The application logic (resize handling) is fully tested. Only the OS signal delivery plumbing is excluded, which would require sending actual POSIX signals during tests.

---

### 4. Main Entry Point

**File:** `src/client.c` (lines 10-45)

**Reason:** The `main()` function is the process entry point and contains only initialization, cleanup, and integration of already-tested components.

**Details:**
- Loads configuration (tested in config tests)
- Initializes REPL (tested in repl_init tests)
- Runs REPL event loop (tested in repl_run tests)
- All individual components are fully tested; only the top-level integration is excluded

**Code Example:**
```c
/* LCOV_EXCL_START */
int main(void) {
    void *root_ctx = talloc_new(NULL);
    res_t cfg_result = ik_cfg_load(root_ctx, "~/.config/ikigai/config.json");
    /* ... initialization ... */
    res_t result = ik_repl_run(repl);
    /* ... cleanup ... */
    return is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;
}
/* LCOV_EXCL_STOP */
```

**Justification:** Integration tests and manual testing verify the main flow. Each component called by `main()` has comprehensive unit tests.

---

## Removed Exclusions (Now Tested)

The following exclusions have been removed and the code is now tested:

### 1. HTTP Error Handling

**File:** `src/openai/client_multi.c` (previously lines 129-211)

**Status:** ✅ Exclusions removed

**Coverage:** Code is now exercised by integration tests that make actual HTTP requests. Manual testing (Tasks 7.10-7.14) verified all error paths.

---

### 2. REPL HTTP Completion Callback

**File:** `src/repl_callbacks.c` (previously lines 98-154)

**Status:** ✅ Exclusions removed, comprehensive unit tests added

**Test File:** `tests/unit/repl/repl_http_completion_callback_test.c`

**Coverage:** 9 test cases covering:
- Flushing streaming buffer on completion
- Clearing previous errors
- Storing error messages on failure (4xx, 5xx, network errors)
- Storing response metadata on success
- Clearing previous metadata

---

### 3. REPL Error Handler

**File:** `src/repl.c` (previously lines 113-140)

**Status:** ✅ Exclusions removed

**Coverage:** Function `handle_request_error()` is now called during the completion flow and tested through REPL completion tests.

---

### 4. Debug Output

**File:** `src/openai/client_multi_request.c` (previously lines 94-106)

**Status:** ✅ Exclusions removed

**Coverage:** Debug code paths are now covered when tests use debug output pipes.

---

## Current Branch Exclusions

Some defensive programming branches are fundamentally unreachable but kept for robustness. These are documented with inline `// LCOV_EXCL_BR_LINE` comments.

### 1. yyjson Library Invariants

**File:** `src/openai/http_handler.c`

**Lines:** 68, 81, 220

**Reason:** Defense-in-depth checks for conditions that cannot occur given yyjson's API contracts and C standard library behavior.

**Details:**
- **Line 68:** `if (!root || !yyjson_is_obj(root))` - The `!root` branch is unreachable because `yyjson_doc_get_root()` never returns NULL for a valid document (which we've already verified exists).
- **Line 81:** `if (!choice0 || !yyjson_is_obj(choice0))` - The `!choice0` branch is unreachable because `yyjson_arr_get(arr, 0)` never returns NULL for a non-empty array (which we've already verified).
- **Line 220:** `if (written < 0 || (size_t)written >= sizeof(auth_header))` - The `written < 0` branch is extremely rare; `snprintf` only returns -1 on encoding errors which don't occur with ASCII strings and %s format specifiers.

**Justification:** These defensive checks protect against theoretical edge cases in external library behavior and are kept for code robustness, but are effectively untestable without modifying the external library's internal state.

---

## Coverage Metrics

After removing unjustified exclusions:

- **Lines excluded:** 819 (only legitimate exclusions remain)
- **Coverage target:** 80% per file of testable code
- **Excluded categories:** 4 (panic, wrappers, signals, main)

All other code must maintain 80% line, function, and branch coverage per file.

---

## Verification

To verify coverage:

```bash
make BUILD=coverage check
```

To review exclusions:

```bash
grep -n "LCOV_EXCL" src/**/*.c
```

---

## Policy

**New exclusions require:**

1. Strong justification (falls into one of the 4 legitimate categories)
2. Documentation in this file
3. Approval from code owners
4. Confirmation that the code is fundamentally untestable

**Invalid reasons for exclusion:**

- "Hard to test" (not impossible, just difficult)
- "Tested manually" (write automated tests)
- "Rarely executed" (still must be tested)
- "Integration tested" (unit test it too if possible)

---

**Last Updated:** 2025-11-27
**Maintained By:** Development Team
