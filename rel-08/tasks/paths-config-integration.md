# Task: Config Module Integration with Paths

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/extended
**Depends on:** paths-comprehensive-tests.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded. Only list additional skills.)

**Plan:**
- `rel-08/plan/paths-test-migration.md` - Config Module Integration (lines 476-492), Category 3 (lines 138-197)
- `rel-08/plan/paths-module.md` - Config loading specification

**Source:**
- `src/config.c` - Current implementation with hardcoded paths
- `src/config.h` - Public API
- `tests/unit/config/` - All 11 config test files (72 call sites to update)

## Libraries

Use only:
- talloc - For memory management
- yyjson - For JSON parsing
- Standard POSIX - For file I/O

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] Paths module fully implemented and tested
- [ ] Test helpers using paths module

## Objective

Integrate the config module with the paths module, updating `ik_config_load()` to use paths-based directory resolution instead of receiving an explicit path. Move tilde expansion from config module to paths module (making it internal). Update all 72 config test call sites to use the new signature.

## Interface Changes

### ik_config_load() Signature

**BEFORE (remove path parameter):**
```c
res_t ik_config_load(TALLOC_CTX *ctx, const char *path, ik_cfg_t **out);
```

**AFTER (add paths parameter):**
```c
res_t ik_config_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_cfg_t **out);
```

### ik_cfg_expand_tilde() - REMOVE FROM CONFIG MODULE

**Current location:** `src/config.c` (public API in `src/config.h`)

**Status:** Already implemented as `ik_paths_expand_tilde()` in paths module (PUBLIC API in `src/paths.h`)

**Why:** Tilde expansion is a path operation, not a config operation. Config module was a temporary home. Paths module is the proper location. It's PUBLIC because it's useful for other modules (e.g., credentials.c).

**Current usage sites:**
- `src/config.c` - Internal use, will switch to `ik_paths_expand_tilde()`
- `src/credentials.c` - May use it, will switch to `ik_paths_expand_tilde()`

**Action:**
1. Remove `ik_cfg_expand_tilde()` implementation from `src/config.c`
2. Remove declaration from `src/config.h`
3. Update all callers to use `ik_paths_expand_tilde()` instead
4. Include `src/paths.h` where needed

## Behaviors

### New Config Loading Behavior

**Current (hardcoded path):**
```c
// Client explicitly specifies path
res_t result = ik_config_load(ctx, "~/.config/ikigai/config.json", &cfg);
```

**New (uses paths module):**
```c
// Get config directory from paths, construct path internally
res_t result = ik_config_load(ctx, paths, &cfg);
```

**Internal implementation:**
```c
res_t ik_config_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_cfg_t **out)
{
    // Get config directory from paths module
    const char *config_dir = ik_paths_get_config_dir(paths);

    // Build config file path
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);
    if (!config_path) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Try to load config file
    res_t result = load_config_file(ctx, config_path, out);

    // If file doesn't exist, create default config
    if (is_err(&result) && result.err->code == ERR_IO) {
        return create_default_config(ctx, config_path, out);
    }

    return result;
}
```

**Single location:** Config loads from one install-appropriate location (not a cascading search). The paths module determines the correct config directory based on install type.

### Test Updates Required

**72 call sites across 11 files must change from:**
```c
char test_config[512];
snprintf(test_config, sizeof(test_config), "/tmp/ikigai_test_%d.json", getpid());
res_t result = ik_config_load(ctx, test_config, &cfg);
```

**To:**
```c
// Setup test environment (creates /tmp/ikigai_test_${PID}/config/)
test_paths_setup_env();

// Create paths instance (uses test environment)
ik_paths_t *paths = NULL;
TRY(ik_paths_init(ctx, &paths));

// Config loading uses paths (no explicit path parameter)
res_t result = ik_config_load(ctx, paths, &cfg);
```

### Test Files to Update (in order)

Update in dependency order (foundation → advanced):

1. `tests/unit/config/basic_test.c` (6 sites) - Foundation tests
2. `tests/unit/config/config_test.c` (7 sites) - Core loading
3. `tests/unit/config/filesystem_test.c` (2 sites) - Directory ops
4. `tests/unit/config/tilde_test.c` (1 site) - SPECIAL: Delete this file (tests migrated to paths module)
5. `tests/unit/config/validation_types_test.c` (8 sites)
6. `tests/unit/config/validation_ranges_test.c` (10 sites)
7. `tests/unit/config/validation_missing_test.c` (6 sites)
8. `tests/unit/config/default_provider_test.c` (4 sites)
9. `tests/unit/config/history_size_test.c` (7 sites)
10. `tests/unit/config/tool_limits_test.c` (7 sites)
11. `tests/integration/config_integration_test.c` (9 sites)

**Compile after each file** to catch issues incrementally.

## Test Implementation

**Follow TDD workflow (Red/Green/Verify):**

**Step 1 - Red (Failing Test):**

**Expected:** All config tests will fail to compile after signature change.

Actions:
1. Update `ik_config_load()` signature in `src/config.h`
2. Update implementation in `src/config.c`
3. Move `ik_cfg_expand_tilde()` from `src/config.c` to `src/paths.c`
4. Build: `make check`
5. Expect 72+ compilation errors

**Step 2 - Green (Minimal Implementation):**

Update each test file one at a time:

1. Update `tests/unit/config/basic_test.c`:
   - Add `test_paths_setup_env()` calls
   - Create `ik_paths_t *paths` instances
   - Pass paths instead of path string to `ik_config_load()`
   - Run `make check` - verify this file's tests pass

2. Repeat for each config test file in order

3. **Special case - tilde_test.c:**
   - Delete `tests/unit/config/tilde_test.c` entirely
   - Tests already migrated to `tests/unit/paths/tilde_test.c`
   - Remove from Makefile

4. **Check credentials.c:**
   - If it calls `ik_cfg_expand_tilde()`, update to use paths module API
   - Or update to call internal config function if needed

STOP when all config tests pass.

**Step 3 - Verify:**
- Run `make check` - all tests must pass
- Verify no compilation errors remain
- Verify coverage maintained: `make coverage`

## Test Patterns

### Basic Test Update Pattern

**BEFORE:**
```c
START_TEST(test_load_valid_config)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *path = "/tmp/test_config.json";
    // Create test file at path...

    ik_cfg_t *cfg = NULL;
    res_t result = ik_config_load(ctx, path, &cfg);

    ck_assert(is_ok(&result));
    // Assertions...

    talloc_free(ctx);
}
END_TEST
```

**AFTER:**
```c
START_TEST(test_load_valid_config)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Setup test environment
    const char *test_dir = test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Config will be loaded from paths->config_dir
    // Create test file there...
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);
    // Write test JSON to config_path...

    ik_cfg_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);

    ck_assert(is_ok(&result));
    // Assertions...

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST
```

### Error Test Update Pattern

**BEFORE:**
```c
START_TEST(test_load_missing_file)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_cfg_t *cfg = NULL;
    res_t result = ik_config_load(ctx, "/nonexistent/path/config.json", &cfg);

    ck_assert(is_err(&result));
    // Or check that default config was created...

    talloc_free(ctx);
}
END_TEST
```

**AFTER:**
```c
START_TEST(test_load_missing_file)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Setup test environment (no config file created)
    test_paths_setup_env();

    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    ik_cfg_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);

    // Behavior: creates default config if missing
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(cfg);
    // Verify it's a default config...

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST
```

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-config-integration.md): [success|partial|failed] - [brief description]

[Optional: Details about what was accomplished, failures, or remaining work]
EOF
)"
```

Report status to orchestration:
- Success: Task complete, config module integrated, all tests passing
- Partial/Failed: Describe what's incomplete or failing

## Postconditions

- [ ] `ik_config_load()` signature updated
- [ ] `ik_cfg_expand_tilde()` removed from config module
- [ ] All callers updated to use `ik_paths_expand_tilde()` instead
- [ ] All 72 config test call sites updated
- [ ] `tests/unit/config/tilde_test.c` deleted
- [ ] `src/credentials.c` updated if needed
- [ ] All tests pass
- [ ] Coverage maintained at 100%
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
