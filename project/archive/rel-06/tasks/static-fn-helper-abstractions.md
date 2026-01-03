# Task: Add Common Helper Abstractions (Optional)

## Target

Refactoring #5: Reconsider the Static Function Ban (Phase 1: Foundation)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm

### Source Files

- src/panic.h (PANIC macro definition)
- src/wrapper.h (MOCKABLE pattern)
- src/json_allocator.h (talloc integration with yyjson)

### Reference

- .agents/skills/style.md (updated static function policy)
- rel-06/docs/lcov-static-fn-findings.md (LCOV behavior documentation)

### Test Patterns

- tests/unit/panic/panic_test.c (PANIC testing)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `static-fn-policy-update` is complete
- New static function policy is in place

## Task

Add commonly needed helper macros/functions to reduce code duplication, now that static functions are allowed with guidelines.

### What

Add helper abstractions for patterns that are repeated 30+ times across the codebase:
1. Temporary talloc context creation
2. Mutex lock/unlock patterns (if applicable)

### How

1. **Temporary Context Helper** - Create `src/tmp_ctx.h`:
   ```c
   #ifndef IK_TMP_CTX_H
   #define IK_TMP_CTX_H

   #include <talloc.h>
   #include "panic.h"

   // Create a temporary talloc context, PANIC if allocation fails
   // Usage:
   //   TALLOC_CTX *tmp = tmp_ctx_create();
   //   // ... use tmp ...
   //   talloc_free(tmp);
   static inline TALLOC_CTX *tmp_ctx_create(void)
   {
       TALLOC_CTX *ctx = talloc_new(NULL);
       if (ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       return ctx;
   }

   #endif // IK_TMP_CTX_H
   ```

2. **WITH_TMP_CTX Macro** (alternative approach):
   ```c
   // Usage: WITH_TMP_CTX(tmp) { ... use tmp ... }
   #define WITH_TMP_CTX(name) \
       for (TALLOC_CTX *name = tmp_ctx_create(), *_once = (void*)1; \
            _once; \
            _once = NULL, talloc_free(name))
   ```

3. **Test the helper**:
   - Create `tests/unit/tmp_ctx/tmp_ctx_test.c`
   - Test that `tmp_ctx_create()` returns non-NULL
   - Test that the context can be used and freed
   - Verify LCOV exclusion works correctly

4. **Optional: Gradually migrate existing code**:
   - This task only adds the helper
   - Future tasks can migrate the 30+ duplicated patterns
   - Keep track of migration candidates in a comment or doc

### Why

The static function ban led to 30+ repetitions of:
```c
TALLOC_CTX *tmp = talloc_new(NULL);
if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
// ... work ...
talloc_free(tmp);
```

With static functions allowed (per updated policy), we can:
1. Define the pattern once
2. Use inline function for type safety
3. Reduce code duplication
4. Make code more readable

## TDD Cycle

### Red

Write tests that:
1. Call `tmp_ctx_create()` and verify it returns non-NULL
2. Allocate something using the returned context
3. Free the context without errors

Tests will fail because the header doesn't exist yet.

### Green

1. Create `src/tmp_ctx.h` with the inline function
2. Create test file
3. Run `make check` - test should pass

### Refactor

1. Consider if macro approach is better than inline function
2. Ensure LCOV exclusion is working (check coverage report)

### Verify

- `make check` passes
- `make coverage` passes (LCOV exclusion honored)
- `make lint` passes

## Post-conditions

- `src/tmp_ctx.h` exists with `tmp_ctx_create()` helper
- Test coverage for the helper
- LCOV exclusion working correctly
- Existing code unchanged (migration is future work)

## Notes

- This is an OPTIONAL low-priority task
- Only adds the helper; does not migrate existing code
- Can be deferred if higher priority work exists
- Priority: Lowest (optional enhancement)
- Depends on: static-fn-policy-update
- Future work: Migrate 30+ existing patterns to use helper
