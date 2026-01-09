# Task: Paths Module Data Directory Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** paths-comprehensive-tests.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded.)

**Plan:**
- `rel-08/plan/paths-module.md` - Data directory specification (lines 118-126)

**Source:**
- `src/paths.c` - Paths module implementation
- `src/paths.h` - Public API
- `tests/unit/paths/` - Existing paths tests

## Critical Gap Found

During manual testing of PREFIX=$HOME/.local install, discovered that:

1. **Database migrations path was hardcoded** as `"share/ikigai/migrations"` in `src/db/connection.c`
2. **ik_db_init() didn't use paths module** to find data directory
3. **This caused segfault** when running installed binary (couldn't find migrations)

**Fix applied:**
- Added `data_dir` parameter to `ik_db_init()`
- Updated `shared.c` to call `ik_paths_get_data_dir(paths)` and pass to db_init
- Fixed wrapper functions in `wrapper_internal.h` and `wrapper_internal.c`

## Objective

Add comprehensive tests for paths module data directory functionality to prevent regression of the hardcoded path bug.

## Test Coverage Needed

### 1. Data Directory Resolution

**File:** `tests/unit/paths/data_dir_test.c`

Test that `ik_paths_get_data_dir()` returns correct paths for different install types:

```c
// Test 1: Development mode (via IKIGAI_DATA_DIR env var)
START_TEST(test_data_dir_development)
{
    setenv("IKIGAI_BIN_DIR", "/tmp/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/tmp/test/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/tmp/test/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/tmp/test/libexec/ikigai", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&result));

    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_str_eq(data_dir, "/tmp/test/share/ikigai");
}
END_TEST

// Test 2: User install (XDG paths)
START_TEST(test_data_dir_user_install)
{
    setenv("IKIGAI_BIN_DIR", "/home/user/.local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/home/user/.config/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/home/user/.local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/home/user/.local/libexec/ikigai", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&result));

    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_str_eq(data_dir, "/home/user/.local/share/ikigai");
}
END_TEST

// Test 3: System install
START_TEST(test_data_dir_system_install)
{
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&result));

    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_str_eq(data_dir, "/usr/local/share/ikigai");
}
END_TEST
```

### 2. Database Integration Tests

**File:** `tests/unit/db/connection_paths_test.c`

Test that `ik_db_init()` correctly uses data_dir parameter:

```c
// Test: db_init constructs correct migrations path from data_dir
START_TEST(test_db_init_uses_data_dir)
{
    // Skip if no PostgreSQL available
    if (getenv("IKIGAI_SKIP_DB_TESTS")) {
        ck_assert(1);
        return;
    }

    // Create temp directory for fake migrations
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/ikigai_test_%d", getpid());
    mkdir(temp_dir, 0755);

    char migrations_dir[512];
    snprintf(migrations_dir, sizeof(migrations_dir), "%s/migrations", temp_dir);
    mkdir(migrations_dir, 0755);

    // Create a minimal migration file
    char migration_file[768];
    snprintf(migration_file, sizeof(migration_file), "%s/001-test.sql", migrations_dir);
    FILE *f = fopen(migration_file, "w");
    fprintf(f, "CREATE TABLE test_table (id INTEGER);\n");
    fclose(f);

    // Call ik_db_init with custom data_dir
    ik_db_ctx_t *db = NULL;
    res_t result = ik_db_init(ctx, "postgresql://localhost/test_db", temp_dir, &db);

    // Should succeed if migrations were found at data_dir/migrations
    // (or fail for other reasons, but not "Cannot open migrations directory")
    if (is_err(&result)) {
        const char *msg = error_message(result.err);
        ck_assert_msg(strstr(msg, "Cannot open migrations directory") == NULL,
                     "ik_db_init should use data_dir/migrations, not hardcoded path");
    }

    // Cleanup
    unlink(migration_file);
    rmdir(migrations_dir);
    rmdir(temp_dir);
}
END_TEST

// Test: db_init fails gracefully when migrations dir missing
START_TEST(test_db_init_missing_migrations)
{
    if (getenv("IKIGAI_SKIP_DB_TESTS")) {
        ck_assert(1);
        return;
    }

    ik_db_ctx_t *db = NULL;
    res_t result = ik_db_init(ctx, "postgresql://localhost/test_db", "/nonexistent", &db);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
    ck_assert(strstr(error_message(result.err), "Cannot open migrations directory") != NULL);
}
END_TEST
```

### 3. Integration Test

**File:** `tests/integration/paths_db_integration_test.c`

End-to-end test that paths → db_init works:

```c
START_TEST(test_paths_to_db_init_integration)
{
    if (getenv("IKIGAI_SKIP_DB_TESTS")) {
        ck_assert(1);
        return;
    }

    // Setup paths environment
    test_paths_setup_env();

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&result));

    // Get data_dir from paths
    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_ptr_nonnull(data_dir);

    // Verify data_dir/migrations exists (test helper should create it)
    char migrations_path[512];
    snprintf(migrations_path, sizeof(migrations_path), "%s/migrations", data_dir);

    struct stat st;
    ck_assert_int_eq(stat(migrations_path, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Now db_init should work
    ik_db_ctx_t *db = NULL;
    result = ik_db_init(ctx, "postgresql://localhost/test_db", data_dir, &db);

    // Should not fail with "Cannot open migrations directory"
    if (is_err(&result)) {
        const char *msg = error_message(result.err);
        ck_assert_msg(strstr(msg, "Cannot open migrations directory") == NULL,
                     "Integration: paths → db_init should find migrations");
    }

    test_paths_cleanup_env();
}
END_TEST
```

## Test Implementation

**Follow TDD workflow (Red/Green/Verify):**

**Step 1 - Red (Failing Test):**
1. Create test files listed above
2. Add to Makefile
3. Run `make check` - expect failures if paths functionality incomplete

**Step 2 - Green (Implementation):**
1. Verify `ik_paths_get_data_dir()` returns IKIGAI_DATA_DIR
2. Verify `ik_db_init()` takes `data_dir` parameter (already done)
3. Verify wrapper functions updated (already done)
4. Run `make check` - all new tests pass

**Step 3 - Verify:**
- Run `make check` - all tests pass
- Run `make coverage` - 100% coverage maintained
- Manual test: `PREFIX=$HOME/.local make install && ikigai` works

## Test Helpers Update

Update `tests/test_helpers.c` to support database testing with paths:

```c
const char *test_paths_setup_env(void) {
    static char test_dir[512];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", getpid());

    mkdir(test_dir, 0755);

    // Setup standard test paths
    char bin_dir[512], config_dir[512], data_dir[512], libexec_dir[512];
    snprintf(bin_dir, sizeof(bin_dir), "%s/bin", test_dir);
    snprintf(config_dir, sizeof(config_dir), "%s/config", test_dir);
    snprintf(data_dir, sizeof(data_dir), "%s/share/ikigai", test_dir);
    snprintf(libexec_dir, sizeof(libexec_dir), "%s/libexec/ikigai", test_dir);

    mkdir(bin_dir, 0755);
    mkdir(config_dir, 0755);

    // Create data_dir and migrations subdirectory
    char share_dir[512], migrations_dir[512];
    snprintf(share_dir, sizeof(share_dir), "%s/share", test_dir);
    snprintf(migrations_dir, sizeof(migrations_dir), "%s/migrations", data_dir);

    mkdir(share_dir, 0755);
    mkdir(data_dir, 0755);
    mkdir(migrations_dir, 0755);
    mkdir(libexec_dir, 0755);

    setenv("IKIGAI_BIN_DIR", bin_dir, 1);
    setenv("IKIGAI_CONFIG_DIR", config_dir, 1);
    setenv("IKIGAI_DATA_DIR", data_dir, 1);
    setenv("IKIGAI_LIBEXEC_DIR", libexec_dir, 1);

    return test_dir;
}
```

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-data-dir-tests.md): success - added data_dir tests

Added comprehensive tests for paths module data directory functionality:
- Unit tests for ik_paths_get_data_dir() with different install types
- Unit tests for ik_db_init() using data_dir parameter
- Integration test for paths → db_init workflow
- Updated test helpers to create migrations directory

Prevents regression of hardcoded "share/ikigai/migrations" bug.
EOF
)"
```

## Postconditions

- [ ] New test file `tests/unit/paths/data_dir_test.c` created
- [ ] New test file `tests/unit/db/connection_paths_test.c` created
- [ ] New test file `tests/integration/paths_db_integration_test.c` created
- [ ] Test helpers updated to create migrations directory
- [ ] All new tests added to Makefile
- [ ] All tests pass (`make check`)
- [ ] Coverage at 100% (`make coverage`)
- [ ] Manual verification: `PREFIX=$HOME/.local make install && ikigai` works
- [ ] All changes committed
