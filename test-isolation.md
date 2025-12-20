# Test Isolation Fix: Filesystem Resource Conflict

## Problem

Tests running in parallel experience intermittent failures due to shared filesystem resources.

### Root Cause

All tests use the same log file path: `/tmp/.ikigai/logs/current.log`

When multiple test binaries run in parallel, they:
- Race to create `/tmp/.ikigai/logs/` directory
- Race to open/rotate `/tmp/.ikigai/logs/current.log`
- Interleave log writes to the same file
- Conflict on file locks or directory operations

### Current Isolation Status

| Resource | Isolated? | Mechanism |
|----------|-----------|-----------|
| Database | Yes | Each test file gets unique DB via `ik_test_db_name(__FILE__)` |
| Transactions | Yes | Each test uses BEGIN/ROLLBACK |
| Log files | **No** | All tests write to `/tmp/.ikigai/logs/current.log` |

## Solution

Add environment variable `IKIGAI_LOG_DIR` to override the log directory. Tests set this to a unique path per test file.

### Design

1. **Logger checks `IKIGAI_LOG_DIR` first**
   - If set: use `{IKIGAI_LOG_DIR}/current.log`
   - If not set: use default `{working_dir}/.ikigai/logs/current.log`

2. **Test utility sets unique log directory per test file**
   - Derive from `__FILE__` (same pattern as database naming)
   - Set env var before any logger is created

3. **Benefits**
   - Tests get isolated log directories
   - Users can override log location if needed
   - No changes to existing function signatures

## Implementation

### Step 1: Modify `src/logger.c`

In `ik_log_setup_directories()`, check env var first:

```c
static void ik_log_setup_directories(const char *working_dir, char *log_path)
{
    const char *override = getenv("IKIGAI_LOG_DIR");

    if (override != NULL && override[0] != '\0') {
        // Use override directory directly
        // Create directory if needed, then set log_path to {override}/current.log
    } else {
        // Default behavior: {working_dir}/.ikigai/logs/current.log
    }
}
```

### Step 2: Add helper to `tests/test_utils.h`

```c
/**
 * Set IKIGAI_LOG_DIR to a unique path based on test file.
 * Call this in suite_setup() before any logger is created.
 *
 * @param file_path  Pass __FILE__ to derive unique directory name
 */
void ik_test_set_log_dir(const char *file_path);
```

### Step 3: Implement in `tests/test_utils.c`

```c
void ik_test_set_log_dir(const char *file_path)
{
    // Derive name from file path (same logic as ik_test_db_name)
    // Set IKIGAI_LOG_DIR=/tmp/ikigai_logs_{basename}
    // Create directory if it doesn't exist
}
```

### Step 4: Update test files

In each test file's suite setup, add:

```c
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
    // ... rest of setup
}
```

## Files to Modify

### Core changes
- `src/logger.c` - Add env var check in `ik_log_setup_directories()`
- `tests/test_utils.h` - Declare `ik_test_set_log_dir()`
- `tests/test_utils.c` - Implement `ik_test_set_log_dir()`

### Test files (add `ik_test_set_log_dir(__FILE__)` to suite setup)

Priority (files with known flakiness):
- `tests/unit/repl/agent_restore_test.c`
- `tests/integration/agent_restore_test.c`

All other files using `ik_logger_create()`:
- `tests/unit/repl/*.c`
- `tests/unit/commands/*.c`
- `tests/unit/shared/*.c`
- `tests/unit/logger/*.c`
- `tests/integration/*.c`
- `tests/helpers/test_contexts.c`

## Verification

1. `make check` - All tests pass
2. `make check-valgrind` - No memory issues, tests don't fail intermittently
3. Run `make check` multiple times to verify no flaky failures

## Future Consideration

Could add `ik_test_cleanup_log_dir()` to remove temp directories after tests, but not strictly necessary since `/tmp` is cleaned periodically.
