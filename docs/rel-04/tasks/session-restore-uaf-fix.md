# Task: Fix Use-After-Free Bugs in session_restore.c

## Target
Bug Fix: Use-after-free in error paths of `ik_repl_restore_session()`

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/di.md
- .agents/skills/patterns/arena-allocator.md

### Pre-read Docs
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md
- fix.md (full problem analysis)

### Pre-read Source (patterns)
- src/marks.c:62-66 (CORRECT pattern: error allocated on parent context)

### Pre-read Source (bug sites)
- src/repl/session_restore.c (current broken pattern)

Bug locations per fix.md:
- Line 45-48: `ik_db_messages_load(tmp, ...)` - error on `tmp`, then `talloc_free(tmp)` = UAF
- Line 108-111: `ik_msg_from_db_(tmp, ...)` - error on `tmp`, then freed = UAF

Safe cases (no changes needed):
- Line 34-37: error on `db_ctx`, not `tmp` (safe)
- Line 91-100: error on `scrollback`, not `tmp` (safe)
- Line 117-120: error on `conv`, not `tmp` (safe)
- Line 129-132: error on `db_ctx`, not `tmp` (safe)
- Line 138-141: error on `db_ctx`, not `tmp` (safe)

### Pre-read Tests (patterns)
- tests/unit/repl/session_restore_test.c

## Pre-conditions
- `make check` passes
- `ik_repl_restore_session()` uses `tmp` context pattern
- Two confirmed UAF bugs at lines 45-48 and 108-111

## Task
Fix the two use-after-free bugs in `ik_repl_restore_session()` by ensuring errors are allocated on a context that survives the cleanup.

Two approaches are valid:
1. **talloc_steal before free** (minimal change): `talloc_steal(repl, result.err)` before `talloc_free(tmp)`
2. **Pass repl for errors** (cleaner): Pass `repl` as error context to the failable functions

Option 1 is simpler and matches the fix.md band-aid pattern. Option 2 requires changing function signatures.

Recommended: Use option 1 (talloc_steal) for consistency with the documented workaround, then add a comment explaining why.

## TDD Cycle

### Red
1. Add tests that trigger the error paths:
   - Mock `ik_db_messages_load` to return error
   - Mock `ik_msg_from_db_` to return error
   - Verify error message is accessible after function returns

2. Run with ASan (`make BUILD=sanitize check`) - should detect UAF in existing code

### Green
1. Fix line 45-48:
   ```c
   res_t load_res = ik_db_messages_load(tmp, db_ctx, session_id);
   if (is_err(&load_res)) {
       talloc_steal(repl, load_res.err);  // Reparent error before freeing tmp
       talloc_free(tmp);
       return load_res;
   }
   ```

2. Fix line 108-111:
   ```c
   res_t msg_res = ik_msg_from_db_(tmp, db_msg);
   if (is_err(&msg_res)) {
       talloc_steal(repl, msg_res.err);  // Reparent error before freeing tmp
       talloc_free(tmp);
       return msg_res;
   }
   ```

3. Add comment at top of function explaining the pattern:
   ```c
   // NOTE: When returning errors after talloc_free(tmp), we must first
   // reparent the error to repl via talloc_steal(). See fix.md for details
   // on this use-after-free bug pattern.
   ```

4. Run `make BUILD=sanitize check` - expect pass with no ASan errors

### Refactor
1. Consider whether a helper macro would be cleaner:
   ```c
   #define CLEANUP_TMP_AND_RETURN(tmp, survivor, result) do { \
       if (is_err(&(result))) talloc_steal((survivor), (result).err); \
       talloc_free(tmp); \
       return (result); \
   } while(0)
   ```
2. Review all other error paths to ensure they're safe
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `make BUILD=sanitize check` passes (no ASan errors)
- Error paths at lines 45-48 and 108-111 use `talloc_steal` before `talloc_free`
- Comment documents the pattern and references fix.md
- 100% test coverage maintained
