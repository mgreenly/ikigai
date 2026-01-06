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

**Current Pattern:**
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

**Why Critical:** These helpers are used by ~160 other tests. Fix these first, and many tests get the fix "for free".

**Change Type:** Update to use new paths module API for test path resolution.

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

**Change Type:** Update to work with paths module's test mode or override mechanism.

**Specific Changes:**
- Use paths module API to get directory paths
- Verify paths module resolves to expected test locations
- Update hardcoded `.ikigai` references to use paths API

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

**Test Helper:**
```c
// tests/test_helpers.h
void test_paths_setup_env(void);
void test_paths_cleanup_env(void);
```

**Behavior:**
- `test_paths_setup_env()`: Sets IKIGAI_BIN_DIR, IKIGAI_CONFIG_DIR, IKIGAI_DATA_DIR, IKIGAI_LIBEXEC_DIR to test-appropriate values
- `test_paths_cleanup_env()`: Unsets all IKIGAI_*_DIR environment variables

**Usage in Tests:**
Tests that need custom paths set environment variables directly before calling `ik_paths_init()`. Tests that use standard paths call `test_paths_setup_env()` helper.

**Test Isolation Benefits:**
- Tests use production code path (no special test-only APIs)
- Each test controls environment variables independently
- No state sharing between tests (each sets/unsets env vars)
- Easy to verify specific install scenarios (set env vars accordingly)

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
- All 160 calls to `ik_shared_ctx_init()` must update
- BUT most go through test helpers (Category 1)
- Fixing helpers cascades to most tests

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

3. **Phase 3 - Ripple Updates**
   - Update config tests (Category 3)
   - Verify remaining tests (Category 4)
   - Fix filesystem tests (Category 5)
   - Run full test suite

---

## Success Metrics

**Test Migration Complete When:**
- [ ] All 565 existing tests pass
- [ ] 8-12 new paths module tests added
- [ ] 100% coverage maintained across codebase
- [ ] Test helpers use paths module
- [ ] No hardcoded path strings in tests (except in paths module tests)
- [ ] All install types tested (dev/user/system)
- [ ] Test isolation verified (no cross-test contamination)

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

## References

- `project/install-directories.md` - Installation directory specification
- `cdd/plan/paths-module.md` - Paths module implementation plan
- `tests/helpers/test_contexts.c` - Current test helper implementation
- `tests/test_utils.c` - Core test utilities
