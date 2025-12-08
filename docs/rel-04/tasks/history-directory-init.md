# Task: History Directory Creation on Startup

## Target
Feature: Command History - Directory Initialization

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

### Pre-read Docs
- docs/error_handling.md
- docs/return_values.md

### Pre-read Source (patterns)
- src/wrapper.h (POSIX wrappers for system calls)
- src/config.c (file I/O error handling patterns)

### Pre-read Tests (patterns)
- tests/unit/config/load_test.c (file system interaction tests)

## Pre-conditions
- `make check` passes
- History data structures exist
- POSIX wrapper functions exist (posix_mkdir_, posix_access_)

## Task
Create a function to ensure `$PWD/.ikigai/` directory exists, creating it if necessary. This function should:
- Check if directory exists using access()
- Create directory with mkdir() if missing (mode 0755)
- Return error if creation fails (permissions, disk full, etc.)
- Be idempotent (safe to call multiple times)

This will be called during REPL initialization before loading history file.

## TDD Cycle

### Red
1. Add function declaration to `src/history.h`:
   ```c
   // Ensure history directory exists, create if necessary
   res_t ik_history_ensure_directory(void);
   ```

2. Create `tests/unit/history/directory_test.c`:
   ```c
   START_TEST(test_history_ensure_directory_creates)
   {
       // In temp directory without .ikigai/
       // Call ik_history_ensure_directory()
       // Verify .ikigai/ exists
       // Verify directory has correct permissions
   }
   END_TEST

   START_TEST(test_history_ensure_directory_exists)
   {
       // In temp directory with existing .ikigai/
       // Call ik_history_ensure_directory()
       // Verify it succeeds (idempotent)
   }
   END_TEST

   START_TEST(test_history_ensure_directory_permission_denied)
   {
       // Mock mkdir to return EACCES
       // Verify function returns ERR with appropriate message
   }
   END_TEST
   ```

3. Run `make check` - expect test failures

### Green
1. Implement in `src/history.c`:
   ```c
   res_t ik_history_ensure_directory(void) {
       const char *dir = ".ikigai";

       // Check if already exists
       if (posix_access_(dir, F_OK) == 0) {
           return OK(NULL);
       }

       // Create directory
       if (posix_mkdir_(dir, 0755) != 0) {
           if (errno == EEXIST) {
               return OK(NULL);  // Race condition - another process created it
           }
           return ERR(NULL, IK_ERR_IO, "Failed to create %s: %s",
                      dir, strerror(errno));
       }

       return OK(NULL);
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider logging directory creation (once logger is available)
2. Ensure error messages are clear and actionable
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `ik_history_ensure_directory()` exists
- Function creates `.ikigai/` with mode 0755
- Function is idempotent
- Permission errors are handled gracefully
- 100% test coverage maintained
