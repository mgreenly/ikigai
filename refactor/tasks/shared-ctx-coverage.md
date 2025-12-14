# Task: Fix Coverage Test with New Shared Context DI Pattern

## Target

Fix the coverage gap in `shared_test.c` for the history load failure path using the new dependency injection pattern.

## Context

### The Original Problem

During coverage testing, we discovered uncovered lines in `shared.c` around history loading failure handling (lines 79-88). The issue was a mocking conflict:

1. `ik_shared_ctx_init()` internally called both logger initialization and history loading
2. Both logger and history use `posix_mkdir_()` to create `.ikigai` directory
3. Mocking `mkdir()` to fail for history also failed it for logger
4. This made it impossible to selectively test the history failure path

### The Solution

The previous tasks (`shared-ctx-signature.md` and `shared-ctx-callsites.md`) refactored `ik_shared_ctx_init()` to accept logger and ikigai_path as injected dependencies. This enables us to:

1. Create the logger separately (mkdir succeeds)
2. Mock mkdir to fail ONLY for history directory creation
3. Test the graceful degradation path when history loading fails

## Pre-read

### Skills
- `coverage.md` - 100% coverage requirements
- `testability.md` - Writing testable code
- `mocking.md` - Mocking strategies
- `tdd-strict.md` - Strict TDD for coverage
- `di.md` - Dependency injection
- `scm.md` - Git commit practices
- `default.md` - Project overview

### Source Code
- `src/shared.c` - Lines 79-88 contain the uncovered graceful degradation path
- `src/history.c` - History loading implementation, `ik_history_ensure_directory()`
- `tests/unit/shared/shared_test.c` - Test file to update

### Documentation
- Review the architect's recommendation from the coverage investigation (Option A: DI refactoring)

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. `shared-ctx-callsites.md` task completed (all call sites updated)
3. All tests pass (`make check`)
4. Coverage currently shows lines 79-88 in `shared.c` as uncovered

## Task

Add a test case in `shared_test.c` that exercises the history load failure path using the new DI pattern.

## TDD Cycle

### Red: Write the Failing Test

1. **Read the target code** to understand what needs coverage:
   ```
   src/shared.c lines 79-88
   ```

   This is the error handling path when `ik_history_load()` fails. It should:
   - Log a warning about history load failure
   - Continue with empty history (graceful degradation)
   - Not return an error (shared context still usable)

2. **Read the existing test file**:
   ```
   tests/unit/shared/shared_test.c
   ```

3. **Add a new test case** named `test_ik_shared_ctx_init_history_load_failure`:

   ```c
   START_TEST(test_ik_shared_ctx_init_history_load_failure)
   {
       // Setup: Create a unique test directory
       char unique_dir[256];
       snprintf(unique_dir, sizeof(unique_dir), "/tmp/ikigai_shared_test_history_%d", getpid());
       posix_mkdir_(unique_dir, 0755);

       // Create logger FIRST (mkdir will succeed for logger's .ikigai directory)
       ik_logger_t *logger = ik_logger_create(temp_ctx, unique_dir);
       ck_assert_ptr_nonnull(logger);

       // NOW mock mkdir to fail - this only affects history's mkdir attempt
       mock_mkdir_fail_path = ".ikigai";  // Fail when history tries to create .ikigai

       // Create minimal config
       ik_cfg_t *cfg = create_test_cfg(temp_ctx);

       // Call shared_ctx_init - history load should fail gracefully
       ik_shared_ctx_t *shared = NULL;
       res_t result = ik_shared_ctx_init(temp_ctx, cfg, unique_dir, ".ikigai", logger, &shared);

       // Should succeed despite history failure (graceful degradation)
       ck_assert(is_ok(&result));
       ck_assert_ptr_nonnull(shared);

       // Shared context should exist with empty/default history
       ck_assert_ptr_nonnull(shared->history);

       // Clean up mock
       mock_mkdir_fail_path = NULL;

       // Clean up test directory
       rmdir(unique_dir);
   }
   END_TEST
   ```

4. **Register the test** in the test suite:
   ```c
   tcase_add_test(tc, test_ik_shared_ctx_init_history_load_failure);
   ```

5. **Run the test** - it should pass (if the mock works correctly):
   ```bash
   make check
   ```

### Green: Verify Coverage

1. **Run coverage** to verify the new test covers lines 79-88:
   ```bash
   make coverage
   ```

2. **Check the coverage report**:
   ```bash
   # Look for src/shared.c coverage
   # Verify lines 79-88 are now covered
   ```

3. **If coverage is not achieved**, debug the mock:
   - Verify mkdir mock is being called
   - Check that logger creation happens BEFORE the mock is set
   - Ensure the mock pattern matches what history code uses
   - Add debug logging if needed

### Refactor: Optimize if Needed

1. **Review the test**:
   - Is the test clear and understandable?
   - Does it test exactly what it should (graceful degradation)?
   - Is cleanup properly handled?

2. **Add comments** explaining the key technique:
   ```c
   // Key: Create logger BEFORE setting mkdir mock.
   // This allows logger's .ikigai creation to succeed,
   // but history's .ikigai creation to fail.
   // Tests graceful degradation when history load fails.
   ```

3. **Run full quality checks**:
   ```bash
   make lint
   make check
   make coverage
   ```

4. **Commit the changes**:
   ```bash
   git add tests/unit/shared/shared_test.c
   git commit -m "$(cat <<'EOF'
test: add coverage for history load failure in shared_ctx_init

Add test case for graceful degradation when history loading fails.

The new DI pattern allows logger to be created separately before
mocking mkdir to fail. This enables selective failure of history
directory creation without affecting logger initialization.

Test verifies:
- shared_ctx_init succeeds despite history failure
- Warning is logged about history load failure
- Empty/default history is used instead
- Graceful degradation works as designed

Achieves 100% coverage of error handling path (shared.c:79-88).

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
EOF
)"
   ```

## Expected Coverage

After this test, `src/shared.c` lines 79-88 should be fully covered:

```c
// Lines 79-88 in src/shared.c (should now be covered)
result = ik_history_load(shared, shared->history);
if (is_err(&result)) {
    // Log warning but continue with empty history (graceful degradation)
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(doc, root, "event", "history_load_failed");
    yyjson_mut_obj_add_str(doc, root, "error", result.err->msg);
    ik_log_warn_json(log_doc);
    talloc_free(result.err);
}
```

## Debugging Tips

If the mock doesn't work as expected:

1. **Check mock timing**:
   - Logger created BEFORE mock is set ✓
   - Mock is set BEFORE shared_ctx_init is called ✓
   - Mock is cleared AFTER test completes ✓

2. **Check mock pattern matching**:
   - What path does history use? (check `src/history.c`)
   - Does the mock pattern match? (usually `.ikigai`)
   - Is the mock check case-sensitive?

3. **Add temporary debug logging**:
   ```c
   // In posix_mkdir_ mock
   fprintf(stderr, "mkdir called with: %s (fail_path: %s)\n", pathname, mock_mkdir_fail_path);
   ```

4. **Verify history actually tries to create directory**:
   - Read `src/history.c` to confirm mkdir is called
   - Check that history loading happens in shared_ctx_init

## Post-conditions

1. New test case `test_ik_shared_ctx_init_history_load_failure` added
2. Test registered in test suite
3. All tests pass (`make check`)
4. Coverage shows lines 79-88 in `shared.c` are covered
5. Coverage passes (`make coverage`)
6. Changes committed with descriptive message
7. Working tree is clean

## Notes

- The key insight is that DI allows us to create the logger separately
- This breaks the temporal coupling between logger and history initialization
- Now we can mock mkdir after logger is created but before history loads
- This is a textbook example of how DI improves testability

## Expected Outcome

```json
{"ok": true}
```

Or if blocked:

```json
{"ok": false, "reason": "clear explanation of what's blocking progress"}
```
