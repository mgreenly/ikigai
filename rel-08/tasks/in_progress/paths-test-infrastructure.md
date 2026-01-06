# Task: Paths Module Test Infrastructure

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** paths-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded. Only list additional skills.)

**Plan:**
- `rel-08/plan/paths-test-migration.md` - Test Mode Design section (lines 332-427)

**Source:**
- `tests/test_utils.c` - Current test utilities
- `tests/test_utils.h` - Header for test utilities
- Existing PID-based patterns: `grep -r "getpid()" tests/` shows 127 existing uses

## Libraries

Use only:
- Standard POSIX - For mkdir, setenv, unsetenv, system, getpid

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] `paths-core.md` task completed (paths module exists)

## Objective

Implement test helper functions that set up isolated, PID-based path environments for testing. Each test process gets unique directories under `/tmp/ikigai_test_${PID}/` to prevent cross-test interference during parallel execution.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `const char *test_paths_setup_env(void)` | Creates PID-based test directories, sets IKIGAI_*_DIR environment variables, returns path prefix |
| `void test_paths_cleanup_env(void)` | Unsets environment variables, removes test directory tree |

## Behaviors

### test_paths_setup_env()

**Directory structure created:**
```
/tmp/ikigai_test_${PID}/
  ├── bin/           # IKIGAI_BIN_DIR
  ├── config/        # IKIGAI_CONFIG_DIR
  ├── share/         # IKIGAI_DATA_DIR
  └── libexec/       # IKIGAI_LIBEXEC_DIR
```

**Environment variables set:**
- `IKIGAI_BIN_DIR` = `/tmp/ikigai_test_${PID}/bin`
- `IKIGAI_CONFIG_DIR` = `/tmp/ikigai_test_${PID}/config`
- `IKIGAI_DATA_DIR` = `/tmp/ikigai_test_${PID}/share`
- `IKIGAI_LIBEXEC_DIR` = `/tmp/ikigai_test_${PID}/libexec`

**Return value:**
- Returns pointer to static thread-local buffer containing `/tmp/ikigai_test_${PID}`
- Buffer persists until `test_paths_cleanup_env()` called
- Safe for use in assertions and test output

**PID-based isolation:**
- Each test process uses unique PID
- Parallel test execution (`-j$(MAKE_JOBS)`) doesn't interfere
- Matches existing pattern: 127 uses of `getpid()` in test code
- Thread-local storage ensures thread safety

**Implementation pattern:**
```c
static __thread char test_path_prefix[256] = {0};

const char *test_paths_setup_env(void)
{
    // Generate PID-based unique prefix
    snprintf(test_path_prefix, sizeof(test_path_prefix),
             "/tmp/ikigai_test_%d", getpid());

    // Create base directory
    mkdir(test_path_prefix, 0755);

    // Create and set each subdirectory
    char buf[512];

    snprintf(buf, sizeof(buf), "%s/bin", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_BIN_DIR", buf, 1);

    // ... repeat for config, share, libexec ...

    return test_path_prefix;
}
```

### test_paths_cleanup_env()

**Behavior:**
- Unsets all IKIGAI_*_DIR environment variables
- Removes test directory tree using `rm -rf`
- Clears static buffer (sets first byte to '\0')
- Safe to call multiple times (idempotent)

**Implementation pattern:**
```c
void test_paths_cleanup_env(void)
{
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");

    if (test_path_prefix[0] != '\0') {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null || true",
                 test_path_prefix);
        system(cmd);
        test_path_prefix[0] = '\0';
    }
}
```

### Memory Management

- Static thread-local buffer (not heap allocated)
- No talloc usage (these are global test utilities)
- Caller does NOT free returned pointer
- Buffer lifetime: until `test_paths_cleanup_env()` called

## Test Implementation

**Follow TDD workflow (Red/Green/Verify):**

**Step 1 - Red (Failing Test):**

Create test file `tests/unit/paths/test_infrastructure_test.c` with test cases:

1. `test_paths_setup_creates_directories` - Verify all 4 directories exist after setup
2. `test_paths_setup_sets_environment` - Verify all 4 env vars set correctly
3. `test_paths_setup_returns_prefix` - Verify return value matches pattern `/tmp/ikigai_test_${PID}`
4. `test_paths_setup_pid_isolation` - Verify prefix contains actual PID
5. `test_paths_cleanup_unsets_environment` - Verify env vars unset after cleanup
6. `test_paths_cleanup_removes_directories` - Verify directories removed after cleanup
7. `test_paths_cleanup_idempotent` - Verify calling cleanup twice doesn't crash

Add function declarations to `tests/test_utils.h`:
```c
const char *test_paths_setup_env(void);
void test_paths_cleanup_env(void);
```

Add stub implementations to `tests/test_utils.c`:
```c
const char *test_paths_setup_env(void) {
    return NULL;
}

void test_paths_cleanup_env(void) {
    // Empty stub
}
```

Build and run: `make check`

Verify tests FAIL with assertion failures.

**Step 2 - Green (Minimal Implementation):**

Implement functions in `tests/test_utils.c`:

1. Add static thread-local buffer
2. Implement `test_paths_setup_env()` per specification
3. Implement `test_paths_cleanup_env()` per specification

STOP when all tests pass.

**Step 3 - Verify:**
- Run `make check` - all tests must pass
- Run `make lint` - complexity under threshold

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-test-infrastructure.md): [success|partial|failed] - [brief description]

[Optional: Details about what was accomplished, failures, or remaining work]
EOF
)"
```

Report status to orchestration:
- Success: Task complete, all tests passing
- Partial/Failed: Describe what's incomplete or failing

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (7 test cases)
- [ ] `make check` passes
- [ ] Helpers ready for use in other tests
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
