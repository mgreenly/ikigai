## Objective

Fix all quality check failures until every check passes cleanly.

Work through checks in order: compile, filesize, unit, integration, complexity, sanitize, tsan, valgrind, helgrind, coverage. For each failing check, use the guidance below to fix it, then re-verify before moving to the next.

## Strategy

1. Run `.claude/scripts/check-compile`
   - If it fails, fix using compile guidance below
   - Re-run `.claude/scripts/check-compile` until it passes
2. Run `.claude/scripts/check-filesize`
   - If it fails, fix using filesize guidance below
   - Re-run `.claude/scripts/check-filesize` until it passes
3. Run `.claude/scripts/check-unit`
   - If it fails, fix using unit test guidance below
   - Re-run `.claude/scripts/check-unit` until it passes
4. Run `.claude/scripts/check-integration`
   - If it fails, fix using integration test guidance below
   - Re-run `.claude/scripts/check-integration` until it passes
5. Run `.claude/scripts/check-complexity`
   - If it fails, fix using complexity guidance below
   - Re-run `.claude/scripts/check-complexity` until it passes
6. Run `.claude/scripts/check-sanitize`
   - If it fails, fix using sanitize guidance below
   - Re-run `.claude/scripts/check-sanitize` until it passes
7. Run `.claude/scripts/check-tsan`
   - If it fails, fix using tsan guidance below
   - Re-run `.claude/scripts/check-tsan` until it passes
8. Run `.claude/scripts/check-valgrind`
   - If it fails, fix using valgrind guidance below
   - Re-run `.claude/scripts/check-valgrind` until it passes
9. Run `.claude/scripts/check-helgrind`
   - If it fails, fix using helgrind guidance below
   - Re-run `.claude/scripts/check-helgrind` until it passes
10. Run `.claude/scripts/check-coverage`
    - If it fails, fix using coverage guidance below
    - Re-run `.claude/scripts/check-coverage` until it passes

**Run checks ONE AT A TIME.** Do not run in parallel. Wait for each to complete before proceeding.

## Why This Order

Each check builds on what comes before:
- **compile** - Must build before anything else matters
- **filesize** - Files must be manageable before deep analysis
- **unit + integration** - Tests establish baseline correctness
- **complexity** - Code must be simple enough to trust
- **sanitize** - Memory safety (ASan, UBSan, LSan)
- **tsan** - Thread safety (data races)
- **valgrind** - Memory errors under Valgrind
- **helgrind** - Thread errors under Valgrind
- **coverage** - Final completeness check (90% requirement)

## Regression Warning

Changes can break earlier checks. A tsan fix might add code that breaks filesize. A coverage fix might break a unit test.

After fixing any check, if you suspect earlier checks might have regressed, re-verify them before continuing forward.

---

## Fix Guidance by Check Type

### compile - Get It Building

**Common Issues:**
- Implicit declarations (missing includes/forward declarations)
- Type mismatches (wrong signatures)
- Undefined references (missing implementations, linker issues)
- Syntax errors (typos, missing semicolons)

**Key Advice:**
- Fix first error first - subsequent errors often cascade from it
- Check signatures match between declaration and definition
- Ensure new .c files added to Makefile targets

**Skills:** `/load errors`, `/load style`

### filesize - Split Large Files

**Critical Rule:** SPLIT files along functional boundaries, don't just shrink code.

**How to Split:**
- Extract helper utilities (string processing, array operations)
- Separate parsing logic from business logic
- Isolate formatting/rendering functions
- Group I/O operations together

**Don't:**
- Split mid-function or mid-logical-unit
- Make arbitrary cuts to hit byte count
- Create files without clear purpose

**After Splitting:**
- Add new .c files to Makefile
- Update test includes if needed
- Run `make fmt` then verify `.claude/scripts/check-compile`

**Skills:** `/load source-code` (understand module structure), `/load style`

### unit + integration - Fix Failing Tests

**Critical Rule:** Fix the implementation to match tests. Do NOT modify tests to pass.

**Common Issues:**
- Assertion failures (implementation doesn't match expected behavior)
- Segfaults (memory errors, null dereferences)
- Talloc errors (ownership violations)
- Setup/teardown issues (fixtures not initialized)

**Skills:** `/load tdd`, `/load memory`, `/load errors`, `/load database`

### complexity - Simplify Functions

**Thresholds:**
- Cyclomatic complexity ≤ 15
- Nesting depth ≤ 5

**How to Reduce:**
- Extract helper functions for logical subtasks
- Use early returns (guard clauses) to reduce nesting
- Simplify conditionals (extract to named boolean functions)
- Replace nested loops with helper functions

**Don't:**
- Use clever tricks to lower numbers
- Create helpers that don't make code clearer

**Validation Sequence:**
1. `make fmt` (format code)
2. `.claude/scripts/check-compile` (ensure builds)
3. `.claude/scripts/check-complexity` (verify thresholds)

**Skills:** `/load style`, `/load source-code`

### sanitize - Memory Safety

**Error Types:**
- heap-buffer-overflow (reading/writing past bounds)
- use-after-free (accessing freed memory)
- double-free (freeing twice)
- memory leak (not freed or not attached to talloc)
- undefined-behavior (integer overflow, null deref, invalid shifts)

**Critical Advice:**
- For use-after-free: start at FREED location, not crash location
- Fix ownership/lifetime issues (usually wrong context to allocator)
- Do NOT add defensive NULL checks everywhere - fix root cause
- Ensure talloc memory properly parented

**Skills:** `/load memory`, `/load errors`, `/load sanitizers`

### tsan - Thread Safety

**Common Issues:**
- Data races (multiple threads, no synchronization)
- Lock ordering violations (potential deadlocks)
- Missing synchronization

**Key Advice:**
- ThreadSanitizer shows TWO conflicting accesses - both need sync
- Prefer eliminating shared state over adding locks
- Ensure consistent lock ordering

**Skills:** `/load sanitizers`

### valgrind - Memory Errors

**Common Issues:**
- Invalid read/write (accessing outside bounds)
- Uninitialized value (using before setting)
- Memory leak (not freed)
- Invalid free (already freed or never allocated)

**Key Advice:**
- Pay attention to origin of uninitialized values
- Ensure all paths initialize before use
- For leaks, ensure talloc memory properly parented
- Do NOT suppress - fix the bug

**Skills:** `/load memory`, `/load errors`, `/load valgrind`

### helgrind - Thread Errors

**Common Issues:**
- Lock order violations (inconsistent acquisition order)
- Data races (conflicting accesses)
- Unlocked access (accessing protected data without lock)

**Key Advice:**
- Establish consistent global lock order
- Ensure all accesses to shared data use same lock

**Skills:** `/load sanitizers`

### coverage - Achieve 90%

**THE MOST IMPORTANT CHECK**

**The Policy:** 90% coverage of Lines, Functions, AND Branches for ENTIRE codebase.

**Output Format:** Items as objects:
```json
{"file": "src/file.c", "lines": "55.7%", "functions": "100%", "branches": "31.9%"}
```

**Critical Rules:**
1. **Add tests, NOT production code** - if code was pruned, don't add it back
2. **Never use exclusions without explicit permission**
3. **If it can execute in production, it must be tested**

**Decision Framework:**

When you hit uncovered code, ask:

```
Can this code path execute in production?
├─ No → ACCEPT exclusion (assert/PANIC only)
└─ Yes
   └─ What triggers it?
      ├─ User input / External data → MUST TEST
      ├─ Environment / IO failure → WRAP AND MOCK
      ├─ Vendor library error → WRAP AND MOCK
      ├─ OOM condition → REFACTOR to PANIC
      ├─ Function can never fail → REFACTOR res_t to void
      └─ Broken invariant → REFACTOR to PANIC
```

**Allowed Exclusions (with `LCOV_EXCL_BR_LINE` only):**
- `assert()` - compiled out in release
- `PANIC()` - invariant violations that terminate

**Never Exclude:**
- Defensive checks (test them)
- Library errors (wrap and mock)
- System call failures (wrap and mock)
- "Should never happen" branches (PANIC if truly impossible)

**Common Refactoring Patterns:**

1. **Extract Pure Logic from I/O** - separate parsing from file reading
2. **Infallible → void** - if function cannot fail, don't pretend
3. **OOM Checks → Single-Line PANIC** - consolidate unreachable paths
4. **Unreachable Else → PANIC** - make invariants explicit
5. **Reduce Nesting** - early returns, guard clauses
6. **Parameterize Behavior** - make hardcoded values testable
7. **Wrap Vendor Functions** - allow mocking of external dependencies

**Example - Extract I/O:**
```c
// BEFORE: Hard to test
res_t process_config(const char *path) {
    char *content = read_file(path);  // I/O makes testing hard
    // ... parsing logic ...
}

// AFTER: Testable
res_t parse_config(const char *content, config_t *out);  // Pure, testable
res_t load_config(const char *path, config_t *out) {      // Thin wrapper
    char *content = read_file(path);
    return parse_config(content, out);
}
```

**Example - Wrap External:**
```c
// BEFORE: Can't test yyjson NULL return
yyjson_val *root = yyjson_doc_get_root(doc);
if (!root) return ERR(...);  // "Can't happen" - untestable

// AFTER: Wrapper allows mocking
yyjson_val *root = yyjson_doc_get_root_(doc);  // Wrapped in wrapper.h
if (!root) return ERR(...);  // Now testable with mock
```

**Skills (LOAD ALL FOUR):**
- `/load coverage` - The 90% policy, decision framework, exclusion rules
- `/load lcov` - Finding gaps in coverage.info files, marker syntax
- `/load testability` - Refactoring patterns for hard-to-test code
- `/load mocking` - MOCKABLE pattern for testing external dependencies

Also useful:
- `/load tdd` - Test structure and fixtures
- `/load database` - PostgreSQL fixtures
- `/load memory` - Talloc ownership patterns

---

## General Skills

These apply across multiple checks:

- **For talloc/ownership issues:** `/load memory`
- **For Result type patterns:** `/load errors`
- **For naming conventions:** `/load style`
- **For test structure:** `/load tdd`
- **For PostgreSQL/fixtures:** `/load database`
- **For module understanding:** `/load source-code`

---

## Acceptance

DONE when ALL 10 checks return `{"ok": true}`:
1. `.claude/scripts/check-compile`
2. `.claude/scripts/check-filesize`
3. `.claude/scripts/check-unit`
4. `.claude/scripts/check-integration`
5. `.claude/scripts/check-complexity`
6. `.claude/scripts/check-sanitize`
7. `.claude/scripts/check-tsan`
8. `.claude/scripts/check-valgrind`
9. `.claude/scripts/check-helgrind`
10. `.claude/scripts/check-coverage`
