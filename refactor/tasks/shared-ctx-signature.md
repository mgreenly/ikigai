# Task: Update shared_ctx_init Signature with Dependency Injection

## Target

Refactor `ik_shared_ctx_init()` to accept logger and ikigai_path as explicit parameters, improving testability and following dependency injection patterns.

## Context

During coverage testing, we discovered a mocking conflict where both logger initialization and history loading call `posix_mkdir_()` for the `.ikigai` directory. This makes it impossible to selectively test history failure paths without affecting logger initialization.

The root cause is hidden dependencies: `ik_shared_ctx_init()` creates its own logger and hardcodes the `.ikigai` path. This violates the dependency injection pattern used throughout the codebase.

## Pre-read

### Skills
- `di.md` - Dependency injection patterns
- `testability.md` - Writing testable code
- `tdd.md` - Test-driven development
- `naming.md` - Naming conventions
- `errors.md` - Error handling patterns
- `scm.md` - Git commit practices
- `default.md` - Project overview

### Source Code
- `src/shared.c` - Current implementation that creates logger internally
- `src/shared.h` - Function signature to be updated
- `src/logger.c` - Logger creation function (`ik_logger_create`)
- `src/history.c` - History loading that uses `.ikigai` path

### Test Patterns
- `tests/unit/shared/shared_test.c` - Existing tests that will need updating

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. All tests pass (`make check`)
3. Logger dependency injection has been completed (task: `logger-dependency-injection.md`)

## Task

Refactor `ik_shared_ctx_init()` to accept logger and ikigai_path as explicit dependencies.

### Current Signature
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_cfg_t *cfg,
    const char *working_dir,
    ik_shared_ctx_t **out
);
```

### New Signature
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_cfg_t *cfg,
    const char *working_dir,
    const char *ikigai_path,     // e.g., ".ikigai" - always required
    ik_logger_t *logger,          // Pre-created logger - always required
    ik_shared_ctx_t **out
);
```

## TDD Cycle

### Red: Update Tests First

1. **Read the existing test file**:
   ```
   tests/unit/shared/shared_test.c
   ```

2. **Update test setup** to create logger before calling `ik_shared_ctx_init()`:
   ```c
   // In each test's setup
   ik_logger_t *logger = ik_logger_create(temp_ctx, test_dir);

   // Update all calls to ik_shared_ctx_init()
   result = ik_shared_ctx_init(temp_ctx, cfg, test_dir, ".ikigai", logger, &shared);
   ```

3. **Run tests** - they should fail to compile because the signature doesn't match yet:
   ```bash
   make check
   ```

### Green: Update Implementation

1. **Update header** (`src/shared.h`):
   - Modify the function signature to include `ikigai_path` and `logger` parameters
   - Update any documentation comments

2. **Update implementation** (`src/shared.c`):
   - Add the new parameters to the function definition
   - **Remove logger creation** - use the injected logger instead:
     ```c
     // DELETE THIS:
     // shared->logger = ik_logger_create(shared, working_dir);

     // REPLACE WITH:
     assert(logger != NULL);  // LCOV_EXCL_BR_LINE
     shared->logger = logger;
     talloc_steal(shared, logger);  // Transfer ownership
     ```
   - **Use ikigai_path parameter** when initializing history or any code that references `.ikigai`
   - **Assert required parameters**:
     ```c
     assert(ikigai_path != NULL);  // LCOV_EXCL_BR_LINE
     assert(logger != NULL);        // LCOV_EXCL_BR_LINE
     ```

3. **Run tests** to verify the implementation:
   ```bash
   make check
   ```

### Refactor: Clean Up

1. **Review the changes**:
   - Ensure no hardcoded `.ikigai` strings remain in `shared.c`
   - Verify logger is properly transferred to shared context ownership
   - Check that all assertions are in place

2. **Run full quality checks**:
   ```bash
   make lint
   make check
   ```

3. **Commit the changes**:
   ```bash
   git add src/shared.h src/shared.c tests/unit/shared/shared_test.c
   git commit -m "$(cat <<'EOF'
refactor: add logger and ikigai_path DI to ik_shared_ctx_init

Update ik_shared_ctx_init() signature to accept logger and ikigai_path
as explicit parameters instead of creating them internally.

This improves testability by making dependencies explicit and enables
selective mocking of mkdir operations for testing history load failures
without affecting logger initialization.

Changes:
- Add ikigai_path and logger parameters to ik_shared_ctx_init()
- Remove internal logger creation, use injected logger
- Update tests to create logger before calling init
- Add parameter assertions for required dependencies

Follows DI pattern established in logger refactor.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
EOF
)"
   ```

## Post-conditions

1. `ik_shared_ctx_init()` signature includes `ikigai_path` and `logger` parameters
2. Implementation uses injected dependencies, does not create logger internally
3. Tests updated to create logger before calling init
4. All tests pass (`make check`)
5. Lint passes (`make lint`)
6. Changes committed with descriptive message
7. Working tree is clean

## Notes

- **DO NOT update call sites** in this task - that's handled in the next task (`shared-ctx-callsites.md`)
- Only update the signature, implementation, and the tests in `tests/unit/shared/shared_test.c`
- The tests in `shared_test.c` are the ONLY call sites you should update in this task
- This task focuses on the function itself, not its usage across the codebase

## Expected Outcome

```json
{"ok": true}
```

Or if blocked:

```json
{"ok": false, "reason": "clear explanation of what's blocking progress"}
```
