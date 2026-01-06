# Paths Module Test Migration Strategy

This document classifies the test changes required when introducing the paths module and outlines the migration strategy.

## Overview

The paths module introduces centralized path resolution logic (development/user/system install detection, XDG support, tool discovery). This affects ~160-180 existing test files that currently use hardcoded paths.

**Key Finding:** Most tests won't need changes if we fix the test helper infrastructure first.

## Test Change Classification

### Category 1: Infrastructure - Test Helpers (2 files, HIGHEST IMPACT)

**Files:**
- `tests/helpers/test_contexts.c` - Creates shared context with hardcoded `/tmp` and `.ikigai`
- `tests/test_utils.c` - Core test infrastructure

**Helper Functions Requiring Updates:**

1. **`test_shared_ctx_create()`** - Used by ~160 tests
2. **`test_shared_ctx_create_with_cfg()`** - Used by tests needing custom config
3. **`test_repl_create()`** - No changes needed (works via delegation to #1)

**BEFORE Pattern (Current):**
```c
// tests/helpers/test_contexts.c
res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    ik_config_t *cfg = test_cfg_create(ctx);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    return ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, out);
    // Hard-coded "/tmp" and ".ikigai"
}
```

**AFTER Pattern (With Paths Module):**
```c
res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    // Setup test environment (helper from test_utils.c)
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    if (is_err(&result)) return result;

    // Create config and logger
    ik_config_t *cfg = test_cfg_create(ctx);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // New signature: paths replaces working_dir + ikigai_subdir
    return ik_shared_ctx_init(ctx, cfg, paths, logger, out);
}
```

**test_shared_ctx_create_with_cfg()** follows identical pattern but receives `cfg` as parameter.

**Why Critical:** These helpers are used by ~160 other tests. Fix these first, and many tests get the fix "for free".

**Change Type:** Add paths module initialization and update signature.

**Impact:** Once fixed, ~80% of other tests should work unchanged (if helper API is well-designed).

---

### Category 2: New Tests for Paths Module (8-12 new test files)

**Files to Create:**

#### tests/unit/paths/paths_test.c
**Tests:**
- Install type detection (dev/user/system)
- Binary location discovery (`/proc/self/exe`)
- Prefix extraction from binary path
- Path construction for each install type
- Test mode API

#### tests/unit/paths/xdg_test.c
**Tests:**
- XDG environment variable handling
- `XDG_CONFIG_HOME` / `XDG_DATA_HOME` / `XDG_STATE_HOME`
- Fallback to defaults when XDG vars not set
- Multiple XDG paths resolution

#### tests/unit/paths/tilde_test.c
**Tests:**
- Tilde expansion (`~` → `$HOME`)
- Tilde in middle of path (should not expand)
- Missing `$HOME` environment variable
- Already-expanded paths (no-op)

**Note:** Migrate existing tilde tests from `tests/unit/config/tilde_test.c`

#### tests/unit/paths/tool_dirs_test.c
**Tests:**
- System tools directory resolution per install type
- User tools directory (always `~/.ikigai/tools/`)
- Project tools directory (always `./.ikigai/tools/`)
- Directory existence checking (graceful handling of missing dirs)

#### tests/unit/paths/config_search_test.c
**Tests:**
- Config search order (project > user > system)
- First-found-wins behavior
- Missing directories handled gracefully
- XDG config path resolution

#### tests/unit/paths/tool_precedence_test.c
**Tests:**
- Tool override precedence (project > user > system)
- Same tool name in multiple directories
- Shadowing behavior

#### tests/unit/paths/env_override_test.c
**Tests:**
- `IKIGAI_DEV` environment variable detection
- Development mode activation
- Fallback behavior when `IKIGAI_DEV` not set

#### tests/unit/paths/test_mode_test.c
**Tests:**
- Test mode creation API
- Override paths for testing
- Isolation between tests
- Cleanup behavior

**TDD Workflow Note:** All Category 2 test files must follow Red/Green/Verify cycle:
1. **RED:** Write failing test, add function declaration, add stub returning NULL/error
2. **GREEN:** Implement minimal code to pass test
3. **VERIFY:** Run `make check` and `make lint`

See detailed TDD workflows in subagent investigation reports (agent IDs: a03d7bd for examples).

---

### Category 3: Config Tests That Change Behavior (11 files)

**Directory:** `tests/unit/config/`

**Files:**
- `tilde_test.c` - Tilde expansion (migrate to paths module)
- `filesystem_test.c` - Directory operations
- `config_test.c` - Main config loading
- `search_test.c` - Config file search order
- `defaults_test.c` - Default config creation
- Others referencing hardcoded paths

**Why Special:** Config module will now CALL paths module for path resolution. Tests need to reflect this architectural change.

**Change Type:** Update test expectations to match new path resolution behavior.

**Specific Changes:**
- Config loading tests must use paths module's test mode
- Tilde expansion tests migrate to paths module
- Config search tests verify integration with paths module
- Hardcoded `~/.config/ikigai/config.json` references must use paths API

**Affected Test Files (72 call sites across 11 files):**
1. `tests/unit/config/basic_test.c` (6 sites)
2. `tests/unit/config/config_test.c` (7 sites)
3. `tests/unit/config/filesystem_test.c` (2 sites)
4. `tests/unit/config/tilde_test.c` (1 site - migrate to paths module)
5. `tests/unit/config/validation_types_test.c` (8 sites)
6. `tests/unit/config/validation_ranges_test.c` (10 sites)
7. `tests/unit/config/validation_missing_test.c` (6 sites)
8. `tests/unit/config/default_provider_test.c` (4 sites)
9. `tests/unit/config/history_size_test.c` (7 sites)
10. `tests/unit/config/tool_limits_test.c` (7 sites)
11. `tests/integration/config_integration_test.c` (9 sites)

**Example BEFORE/AFTER:**

BEFORE (current):
```c
// tests/unit/config/basic_test.c
char test_config[512];
snprintf(test_config, sizeof(test_config), "/tmp/ikigai_test_%d.json", getpid());
res_t result = ik_config_load(ctx, test_config, &cfg);
```

AFTER (with paths module):
```c
// Setup test environment
char test_dir[256];
snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", getpid());
setenv("IKIGAI_CONFIG_DIR", test_dir, 1);

// Create paths instance
ik_paths_t *paths = NULL;
TRY(ik_paths_init(ctx, &paths));

// Config loading uses paths (no explicit path parameter)
res_t result = ik_config_load(ctx, paths, &cfg);
```

---

### Category 4: Tests That Use Helpers (140-150 files)

**Examples:**
- All REPL tests (13+ files in `tests/unit/repl/`)
- Logger tests (11 files in `tests/unit/logger/`)
- History tests (3 files in `tests/unit/history/`)
- Integration tests (13 files in `tests/integration/`)
- Database tests
- Shared context tests
- Credentials tests
- Many other unit tests

**Current Pattern:**
```c
// Most tests do this:
test_shared_ctx_create(ctx, &shared);  // Uses hardcoded paths internally
```

**Expected After Category 1 Fix:**
No changes needed! The helper will use the paths module internally.

**Possible Changes Needed:**
- Tests that explicitly verify path strings might need updates
- Tests that mock filesystem calls might need updated expectations
- Tests that check specific directory names

**Verification Strategy:**
1. Fix Category 1 helpers
2. Run `make check`
3. Fix only tests that fail
4. Most should pass unchanged

---

### Category 5: Direct Filesystem Tests (10-15 files)

**Files:**
- `tests/unit/history/directory_test.c` - Creates/removes `.ikigai` dirs
- `tests/integration/history_edge_cases_test.c` - Manages `.ikigai` files
- `tests/unit/config/filesystem_test.c` - Tests directory operations
- Tests doing explicit `mkdir`/`rmdir`/`unlink` on paths

**Why Special:** These tests directly manipulate `.ikigai` directories and verify filesystem state.

**Current Pattern:**
```c
// tests/unit/history/directory_test.c
static void teardown(void)
{
    chdir("/");
    char ikigai_path[512];
    snprintf(ikigai_path, sizeof(ikigai_path), "%s/.ikigai", test_dir_buffer);
    // ... cleanup with stat/unlink/rmdir
}
```

**Change Type:** Continue using existing patterns (no paths module API needed).

**Specific Changes:**
- **NO NEW API NEEDED** - Tests should continue using relative paths like `.ikigai` and `.ikigai/history`
- Tests that need absolute paths use `snprintf(buf, size, "%s/.ikigai", test_dir)`
- The `.ikigai/` directory is a module-specific implementation detail, not a cross-cutting path from paths module
- Paths module provides `.ikigai/tools/` via `ik_paths_get_tools_project_dir()`, but `.ikigai/` base is accessed directly

**Rationale:**
- Category 5 tests need filesystem manipulation flexibility (create, delete, chmod)
- `.ikigai/` is an implementation convention of history/logger modules, not an install path
- Adding paths module API would constrain test independence without benefit
- Current approach (relative/constructed paths) is correct and should continue

---

## Migration Strategy

### Phase 1: Infrastructure Foundation

**Goal:** Build foundation for all other changes.

**Tasks:**
1. Implement paths module core functionality
2. Add test mode API to paths module (see Test Mode Design below)
3. Update `tests/helpers/test_contexts.c` to use paths module
4. Update `tests/test_utils.c` to use paths module
5. Verify helper tests pass

**Success Criteria:**
- Test helpers compile and link
- Helper tests pass
- Foundation ready for Category 4 tests

---

### Phase 2: New Paths Module Tests

**Goal:** Comprehensive test coverage for paths module.

**Tasks:**
1. Create `tests/unit/paths/paths_test.c` - Core functionality
2. Create `tests/unit/paths/xdg_test.c` - XDG variables
3. Create `tests/unit/paths/tilde_test.c` - Tilde expansion (migrate from config)
4. Create `tests/unit/paths/tool_dirs_test.c` - Tool directory resolution
5. Create `tests/unit/paths/config_search_test.c` - Config search order
6. Create `tests/unit/paths/tool_precedence_test.c` - Override precedence
7. Create `tests/unit/paths/env_override_test.c` - Environment variables
8. Create `tests/unit/paths/test_mode_test.c` - Test mode API

**Success Criteria:**
- 100% coverage of paths module
- All install types tested (dev/user/system)
- All edge cases covered
- `make check` passes

---

### Phase 3: Ripple Updates

**Goal:** Fix tests broken by architectural changes.

**Tasks:**
1. Update config tests (Category 3) - Verify new integration
2. Verify Category 4 tests - Run `make check`, fix failures only
3. Fix Category 5 tests - Update filesystem manipulation tests
4. Run full test suite: `make check`
5. Verify coverage: `make coverage` (100% requirement)

**Success Criteria:**
- All ~565 tests pass
- 100% coverage maintained
- No test regressions

---

## Test Mode Design

**Requirement:** Tests need isolated, controlled path environments without affecting the real filesystem or requiring special setup.

### Recommended Approach: Environment Variables

With the wrapper script approach, testing is simple: set environment variables, call `ik_paths_init()`.

**No special test API needed.** Tests use exactly the same code path as production.

**Test Helper Specification:**

**Location:** `tests/test_utils.c` and `tests/test_utils.h`

**Function Declarations:**
```c
// tests/test_utils.h
const char *test_paths_setup_env(void);
void test_paths_cleanup_env(void);
```

**Implementation (PID-based isolation required):**
```c
// tests/test_utils.c
static __thread char test_path_prefix[256] = {0};

const char *test_paths_setup_env(void)
{
    // Generate PID-based unique prefix for parallel test execution
    snprintf(test_path_prefix, sizeof(test_path_prefix),
             "/tmp/ikigai_test_%d", getpid());

    // Create base directory
    mkdir(test_path_prefix, 0755);

    // Build and set environment variables with subdirectories
    char buf[512];

    snprintf(buf, sizeof(buf), "%s/bin", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_BIN_DIR", buf, 1);

    snprintf(buf, sizeof(buf), "%s/config", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_CONFIG_DIR", buf, 1);

    snprintf(buf, sizeof(buf), "%s/share", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_DATA_DIR", buf, 1);

    snprintf(buf, sizeof(buf), "%s/libexec", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_LIBEXEC_DIR", buf, 1);

    return test_path_prefix;
}

void test_paths_cleanup_env(void)
{
    // Unset environment variables
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");

    // Remove test directory tree
    if (test_path_prefix[0] != '\0') {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null || true",
                 test_path_prefix);
        system(cmd);
        test_path_prefix[0] = '\0';
    }
}
```

**Behavior:**
- `test_paths_setup_env()`: Creates PID-isolated directories (`/tmp/ikigai_test_${PID}/{bin,config,share,libexec}`), sets environment variables, returns path prefix
- `test_paths_cleanup_env()`: Unsets all IKIGAI_*_DIR environment variables, recursively removes test directory tree

**PID-Based Isolation:**
- **Required** for parallel test execution (tests run with `-j$(MAKE_JOBS)`)
- Prevents cross-test interference when multiple test processes run simultaneously
- Established pattern: 127 existing uses of `getpid()` across test codebase
- Thread-local storage (`__thread`) ensures safety in parallel execution

**Usage in Tests:**
Tests that need custom paths set environment variables directly before calling `ik_paths_init()`. Tests that use standard paths call `test_paths_setup_env()` helper.

**Test Isolation Benefits:**
- Tests use production code path (no special test-only APIs)
- Each test process gets unique, isolated filesystem paths
- No state sharing between tests (PID-based separation)
- Easy to verify specific install scenarios (set env vars accordingly)
- Automatic cleanup via `rm -rf` (simple, robust, matches existing pattern)

---

## Integration Points

### Updated `ik_shared_ctx_init()` Signature

**Current:**
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_config_t *cfg,
    const char *working_dir,
    const char *ikigai_subdir,  // ".ikigai"
    ik_logger_t *logger,
    ik_shared_ctx_t **out
);
```

**Proposed:**
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_config_t *cfg,
    ik_paths_t *paths,  // Replaces working_dir + ikigai_subdir
    ik_logger_t *logger,
    ik_shared_ctx_t **out
);
```

**Migration Impact:**
- **90 direct call sites** to `ik_shared_ctx_init()` must update (not ~163 as initially estimated)
  - 1 in `src/client.c` (production code)
  - 2 in `tests/helpers/test_contexts.c` (helper definitions - **FIX FIRST**)
  - 87 in test files (39 unit tests + 48 integration tests)
- **~160 indirect calls** through test helpers will work once helpers are fixed
- Fixing 2 helper definitions cascades to ~160 downstream tests

**Call Site Categorization:**
| Category | Count | Files | Priority |
|----------|-------|-------|----------|
| Production code | 1 | `src/client.c` | High |
| Test helpers | 2 | `tests/helpers/test_contexts.c` | **Critical** |
| Unit tests (direct) | 39 | 9 files | Medium |
| Integration tests (direct) | 48 | 13 files | Medium |
| **Total** | **90** | **23 files** | |

**Note:** Original estimate of ~163 was high. Actual count is 90 direct calls.

### Config Module Integration

**Current:**
```c
// src/config.c
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path, ik_cfg_t **out);
```

**After Paths Module:**
```c
// Config loading now uses paths module (single location)
res_t ik_cfg_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_cfg_t **out);
```

**Behavior:**
Loads config from single install-appropriate location (from IKIGAI_CONFIG_DIR environment variable). Builds path as `config_dir/config`. If file exists, loads it. If not found, creates default config. No cascading search - one config location per install.

---

## Test Execution Order

**Execute test migration in this order:**

1. **Phase 1 - Infrastructure (FIRST)**
   - Implement paths module
   - Update test helpers
   - Verify helpers work

2. **Phase 2 - New Tests**
   - Write paths module tests
   - Achieve 100% coverage
   - Verify all scenarios

3. **Phase 3 - Ripple Updates (Detailed Execution Order)**

   **Step 1: Config Tests (Category 3) - 11 files, 72 call sites**

   Update in dependency order:
   1. `tests/unit/config/basic_test.c` (6 sites) - Foundation tests
   2. `tests/unit/config/config_test.c` (7 sites) - Core loading
   3. `tests/unit/config/filesystem_test.c` (2 sites) - Directory ops
   4. `tests/unit/config/tilde_test.c` (1 site) - Tilde integration
   5. `tests/unit/config/validation_types_test.c` (8 sites)
   6. `tests/unit/config/validation_ranges_test.c` (10 sites)
   7. `tests/unit/config/validation_missing_test.c` (6 sites)
   8. `tests/unit/config/default_provider_test.c` (4 sites)
   9. `tests/unit/config/history_size_test.c` (7 sites)
   10. `tests/unit/config/tool_limits_test.c` (7 sites)
   11. `tests/integration/config_integration_test.c` (9 sites)

   Run `make check` after each file to catch issues incrementally.

   **Step 2: Verify Category 4 (Helper-Using Tests) - ~160 files**

   After Phase 1 helper fixes, run `make check`.
   **Expected:** 0 failures if helpers fixed correctly.
   **Do NOT update these files** - they work through helper delegation.

   **Step 3: Filesystem Tests (Category 5) - 10-15 files**

   Verify (not update) these files:
   1. `tests/unit/history/directory_test.c`
   2. `tests/unit/history/file_io_test.c`
   3. `tests/unit/history/file_io_errors_test.c`
   4. `tests/integration/history_edge_cases_test.c`
   5. `tests/integration/history_navigation_test.c`
   6. `tests/integration/history_persistence_test.c`
   7. Others found by: `grep -r "\.ikigai" tests/`

   **Step 4: Full Verification**
   - Run `make check` - all ~565 tests pass
   - Run `make coverage` - 100% coverage maintained

---

## Success Metrics

**Test Migration Complete When:**
- [ ] All 565 existing tests pass
- [ ] 8-12 new paths module tests added
- [ ] 100% coverage maintained across codebase
- [ ] Test helpers use paths module
- [ ] No hardcoded installation paths in application test code (test infrastructure may hardcode test-specific paths like `/tmp/ikigai_test_*`)
- [ ] All install types tested (dev/user/system)
- [ ] Test isolation verified (PID-based paths prevent cross-test contamination)

---

## Risks and Mitigations

**Risk:** Breaking existing tests during helper updates
**Mitigation:** Fix helpers incrementally, test after each change

**Risk:** Incomplete test coverage of edge cases
**Mitigation:** Comprehensive paths module tests BEFORE migration

**Risk:** Test isolation failures
**Mitigation:** PID-based unique prefixes, explicit cleanup

**Risk:** Cascading test failures
**Mitigation:** Fix Category 1 first, then verify incrementally

---

## Code Cleanup and Removal

The paths module introduction requires removing old code that becomes obsolete:

### Production Code Removals

**1. Hardcoded Config Path (src/client.c:50)**

**Remove:**
```c
res_t cfg_result = ik_config_load(root_ctx, "~/.config/ikigai/config.json", &cfg);
```

**Replace with:**
```c
// After creating paths instance
ik_paths_t *paths = NULL;
TRY(ik_paths_init(root_ctx, &paths));

// Config loading now uses paths for search
res_t cfg_result = ik_config_load(root_ctx, paths, &cfg);
```

**2. Function Signature Changes - Remove Path Parameters**

**File:** `src/shared.h` and `src/shared.c`

**Current signature (REMOVE THESE PARAMETERS):**
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_config_t *cfg,
    const char *working_dir,    // REMOVE
    const char *ikigai_subdir,  // REMOVE
    ik_logger_t *logger,
    ik_shared_ctx_t **out
);
```

**New signature:**
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_config_t *cfg,
    ik_paths_t *paths,          // NEW: replaces working_dir + ikigai_subdir
    ik_logger_t *logger,
    ik_shared_ctx_t **out
);
```

**Impact:** This signature appears in ~163 call sites across production and test code.

**3. Config Loading Signature Change**

**File:** `src/config.h` and `src/config.c`

**Current signature (REMOVE PATH PARAMETER):**
```c
res_t ik_config_load(TALLOC_CTX *ctx, const char *path, ik_cfg_t **out);
```

**New signature:**
```c
res_t ik_config_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_cfg_t **out);
```

**4. Move Tilde Expansion to Paths Module**

**File:** `src/config.c` and `src/config.h`

**Function to MOVE (not delete):**
```c
res_t ik_cfg_expand_tilde(TALLOC_CTX *ctx, const char *path);
```

**Action:**
- Move implementation from `src/config.c` to `src/paths.c`
- Make it internal (static) to paths module
- Used internally by paths module for path expansion
- Remove from `src/config.h` public API

**Current usage:** `src/config.c:97`, `src/credentials.c` (may need adjustment)

**5. Remove cwd Parameter from client.c**

**File:** `src/client.c:76`

**Current (REMOVE cwd variable usage):**
```c
char cwd[PATH_MAX];
if (getcwd(cwd, sizeof(cwd)) == NULL) {
    PANIC("Failed to get current working directory");
}
// ...
res_t result = ik_shared_ctx_init(root_ctx, cfg, cwd, ".ikigai", logger, &shared);
```

**New:**
```c
// cwd still needed for logger, but not for shared_ctx_init
char cwd[PATH_MAX];
if (getcwd(cwd, sizeof(cwd)) == NULL) {
    PANIC("Failed to get current working directory");
}
ik_logger_t *logger = ik_logger_create(logger_ctx, cwd);

// Later...
res_t result = ik_shared_ctx_init(root_ctx, cfg, paths, logger, &shared);
```

**Note:** `cwd` still needed for logger initialization, but removed from `ik_shared_ctx_init()` call.

---

### Test Code Removals

**No test code removal needed.** Tests will be updated to use new signatures, but no test-specific code needs deletion.

---

### Summary of Removals

| Code | Action | File(s) |
|------|--------|---------|
| `"~/.config/ikigai/config.json"` | Remove hardcoded path | `src/client.c:50` |
| `cwd` parameter | Remove from function call | `src/client.c:76` |
| `".ikigai"` parameter | Remove from function call | `src/client.c:76` |
| `working_dir` parameter | Remove from signature | `src/shared.h`, `src/shared.c` |
| `ikigai_subdir` parameter | Remove from signature | `src/shared.h`, `src/shared.c` |
| `path` parameter | Remove from signature | `src/config.h`, `src/config.c` |
| `ik_cfg_expand_tilde()` | Move to paths module (internal) | `src/config.c` → `src/paths.c` |

**Total removals:** ~10 lines of hardcoded path logic, 2 function parameters from signatures

---

## Test Impact Matrix

Complete mapping of production code changes to affected tests, based on subagent investigation (agent ID: ad1df78).

### Impact Summary

| Change Type | Production Sites | Test Files Affected | Call Sites | Compilation Errors |
|-------------|------------------|---------------------|------------|-------------------|
| `working_dir` + `ikigai_subdir` params | 3 | 23 | 90 | 90 |
| `path` parameter (config) | 2 | 11 | 72 | 72 |
| `ik_cfg_expand_tilde()` move | 3 | 1 | 3 | 3 (link errors) |
| **Total** | **~10** | **~34** | **~165** | **~165** |

**Note:** ~106 additional test files use helpers indirectly (will work once helpers are fixed).

### Detailed Impact by Change

#### 1. `ik_shared_ctx_init()` Signature Change

**Change:** Remove `working_dir` and `ikigai_subdir` parameters, add `ik_paths_t *paths` parameter

**Direct Call Sites (90 total):**

**Priority 1 - Test Helpers (2 sites - CRITICAL):**
- `tests/helpers/test_contexts.c:36` - `test_shared_ctx_create()`
- `tests/helpers/test_contexts.c:66` - `test_shared_ctx_create_with_cfg()`

**Fix these first** - unblocks ~160 downstream tests.

**Priority 2 - Production (1 site):**
- `src/client.c:76` - Main initialization

**Priority 3 - Unit Tests (39 sites in 9 files):**
- `tests/unit/shared/shared_test.c` (7 sites)
- `tests/unit/repl/repl_init_test.c` (9 sites)
- `tests/unit/repl/repl_scrollback_scroll_test.c` (6 sites)
- `tests/unit/repl/repl_session_test.c` (4 sites)
- `tests/unit/repl/repl_resize_test.c` (4 sites)
- `tests/unit/repl/repl_init_db_test.c` (4 sites)
- `tests/unit/repl/repl_scrollback_submit_test.c` (3 sites)
- `tests/unit/repl/repl_submit_line_error_test.c` (1 site)
- `tests/unit/repl/repl_autoscroll_test.c` (1 site)

**Priority 4 - Integration Tests (48 sites in 13 files):**
- `tests/integration/completion_edge_cases_test.c` (10 sites)
- `tests/integration/repl_test.c` (7 sites)
- `tests/integration/provider_switching_fork_test.c` (5 sites)
- `tests/integration/provider_switching_basic_test.c` (4 sites)
- `tests/integration/history_persistence_test.c` (4 sites)
- `tests/integration/completion_e2e_test.c` (4 sites)
- `tests/integration/history_navigation_test.c` (3 sites)
- `tests/integration/history_edge_cases_test.c` (3 sites)
- `tests/integration/completion_workflow_test.c` (3 sites)
- `tests/integration/provider_switching_thinking_test.c` (2 sites)
- `tests/integration/completion_args_test.c` (2 sites)
- `tests/integration/test_session_restore_e2e.c` (1 site)
- (1 additional file with unspecified count)

**Compilation Errors:** 90 (wrong parameter count)

#### 2. `ik_config_load()` Signature Change

**Change:** Remove `const char *path` parameter, add `ik_paths_t *paths` parameter

**Direct Call Sites (72 total in 11 files):**
1. `tests/unit/config/validation_ranges_test.c` (10 sites)
2. `tests/unit/config/validation_types_test.c` (8 sites)
3. `tests/unit/config/default_provider_test.c` (4 sites)
4. `tests/unit/config/history_size_test.c` (7 sites)
5. `tests/unit/config/config_test.c` (7 sites)
6. `tests/unit/config/validation_missing_test.c` (6 sites)
7. `tests/unit/config/basic_test.c` (6 sites)
8. `tests/unit/config/tilde_test.c` (1 site)
9. `tests/unit/config/filesystem_test.c` (2 sites)
10. `tests/unit/config/tool_limits_test.c` (7 sites)
11. `tests/integration/config_integration_test.c` (9 sites)

**Compilation Errors:** 72 (wrong parameter type)

#### 3. `ik_cfg_expand_tilde()` Function Move

**Change:** Move from `src/config.c` to `src/paths.c`, make internal (static)

**Affected Tests (3 sites in 1 file):**
- `tests/unit/config/tilde_test.c:19,29,55`

**Link Errors:** 3 (symbol not found)

**Migration:** Entire test file migrates to `tests/unit/paths/tilde_test.c`

#### 4. `.ikigai` Directory References

**Impact:** Tests that manipulate `.ikigai` directories directly

**Affected Files (~26 files):**
- `tests/unit/history/directory_test.c` (creates `.ikigai` dirs)
- `tests/unit/history/file_io_test.c` (works with `.ikigai` paths)
- `tests/unit/history/file_io_errors_test.c`
- `tests/unit/commands/mark_db_test.c`
- Logger tests (10+ files constructing `.ikigai/logs` paths)
- Others found by: `grep -r "\.ikigai" tests/`

**Compilation Errors:** 0 (indirect impact - tests should continue working)

### Fix Strategy

**Phase 1: Fix Helpers (Critical Path)**
1. Update `tests/helpers/test_contexts.c` (2 sites)
2. Verify ~160 helper-using tests compile

**Phase 2: Update Direct Callers**
3. Update `src/client.c` (1 site - production)
4. Update `tests/unit/shared/shared_test.c` (7 sites)
5. Update `tests/unit/repl/*.c` (21 sites across 4 files)
6. Update `tests/integration/*.c` (48 sites across 13 files)

**Phase 3: Update Config Tests**
7. Update 11 config test files (72 sites) in dependency order

**Phase 4: Migrate Tilde Tests**
8. Create `tests/unit/paths/tilde_test.c`
9. Migrate 3 tests from `tests/unit/config/tilde_test.c`

**Phase 5: Verification**
10. Run `make check` - verify all tests pass
11. Run `make coverage` - verify 100% maintained

### Expected Failures by Phase

**Before Any Changes:**
- All ~565 tests pass

**After Production Changes (no test updates):**
- **Compilation:** ~165 errors
- **Link:** 3 errors
- **Tests passing:** 0 (won't compile)

**After Helper Updates Only:**
- **Compilation:** ~93 errors remain (direct callers + config tests)
- **Tests passing:** ~160 (helper users now compile)

**After All Updates:**
- **Compilation:** 0 errors
- **Tests passing:** All ~565 tests

---

## References

- `project/install-directories.md` - Installation directory specification
- `cdd/plan/paths-module.md` - Paths module implementation plan
- `tests/helpers/test_contexts.c` - Current test helper implementation
- `tests/test_utils.c` - Core test utilities
