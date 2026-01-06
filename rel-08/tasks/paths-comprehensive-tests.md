# Task: Comprehensive Paths Module Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/extended
**Depends on:** paths-helper-migration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded. Only list additional skills.)

**Plan:**
- `rel-08/plan/paths-test-migration.md` - Category 2 (lines 67-135), Phase 2 (lines 290-310)
- `rel-08/plan/paths-module.md` - Complete specification

**Source:**
- `tests/unit/paths/paths_test.c` - Already created in paths-core.md task
- `tests/unit/config/tilde_test.c` - Existing tilde tests to migrate

## Libraries

Use only:
- Check framework - For test infrastructure
- Standard POSIX - For setenv, unsetenv, getenv

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] `paths-core.md` completed (basic tests exist)
- [ ] `paths-helper-migration.md` completed (helpers work)

## Objective

Achieve 100% test coverage of the paths module by creating comprehensive test files for XDG support, tool directory resolution, config search, tilde expansion, and environment variable overrides. This expands beyond the basic tests in paths-core.md to cover all edge cases and scenarios.

## Test Files to Create

### 1. tests/unit/paths/env_vars_test.c

**Test scenarios:**

- `test_env_all_vars_set` - Verify success when all 4 IKIGAI_*_DIR vars set
- `test_env_missing_bin_dir` - Verify ERR_INVALID_ARG when IKIGAI_BIN_DIR missing
- `test_env_missing_config_dir` - Verify ERR_INVALID_ARG when IKIGAI_CONFIG_DIR missing
- `test_env_missing_data_dir` - Verify ERR_INVALID_ARG when IKIGAI_DATA_DIR missing
- `test_env_missing_libexec_dir` - Verify ERR_INVALID_ARG when IKIGAI_LIBEXEC_DIR missing
- `test_env_empty_string` - Verify empty string treated as missing
- `test_env_with_spaces` - Verify paths with spaces handled correctly
- `test_env_with_trailing_slash` - Verify trailing slashes preserved

**Pattern:**
```c
START_TEST(test_env_all_vars_set)
{
    // Setup
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(paths);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST
```

### 2. tests/unit/paths/tilde_test.c

**Migrate from `tests/unit/config/tilde_test.c` (3 existing tests) and expand:**

- `test_expand_tilde_home_only` - `~` → `$HOME`
- `test_expand_tilde_with_path` - `~/foo/bar` → `$HOME/foo/bar`
- `test_expand_tilde_absolute_path` - `/absolute/path` → unchanged
- `test_expand_tilde_relative_path` - `relative/path` → unchanged
- `test_expand_tilde_not_at_start` - `foo~/bar` → unchanged
- `test_expand_tilde_home_not_set` - Return `ERR_IO` when HOME unset
- `test_expand_tilde_null_input` - Return `ERR_INVALID_ARG`
- `test_expand_tilde_empty_string` - `""` → `""`

**Migration notes:**
- Copy test patterns from `tests/unit/config/tilde_test.c`
- Adapt to use `ik_paths_expand_tilde()` instead of `ik_cfg_expand_tilde()`
- Add new edge case tests not previously covered

### 3. tests/unit/paths/tool_dirs_test.c

**Test scenarios:**

- `test_tools_system_dir` - Verify returns libexec_dir (same directory)
- `test_tools_user_dir` - Verify returns `~/.ikigai/tools/` (expanded)
- `test_tools_project_dir` - Verify returns `.ikigai/tools/`
- `test_tools_all_three_accessible` - All getters work simultaneously
- `test_tools_user_dir_expands_tilde` - Verify tilde expanded in user tools dir
- `test_tools_project_dir_relative` - Verify project dir is relative path

**Pattern:**
```c
START_TEST(test_tools_system_dir)
{
    // Setup
    test_paths_setup_env();

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - system tools dir should equal libexec dir
    const char *system_dir = ik_paths_get_tools_system_dir(paths);
    const char *libexec_dir = ik_paths_get_libexec_dir(paths);
    ck_assert_str_eq(system_dir, libexec_dir);

    test_paths_cleanup_env();
}
END_TEST
```

### 4. tests/unit/paths/getters_test.c

**Test scenarios:**

- `test_get_bin_dir` - Verify getter returns env var value
- `test_get_config_dir` - Verify getter returns env var value
- `test_get_data_dir` - Verify getter returns env var value
- `test_get_libexec_dir` - Verify getter returns env var value
- `test_getters_not_null` - Verify all getters never return NULL when initialized
- `test_getters_const_strings` - Verify strings remain valid while paths instance alive

## Behaviors

### Test Suite Setup Pattern

Each test file follows this structure:

```c
#include <check.h>
#include <stdlib.h>
#include "paths.h"
#include "test_utils.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    test_paths_cleanup_env();
    talloc_free(test_ctx);
}

START_TEST(test_case_name)
{
    // Test implementation
}
END_TEST

Suite *paths_xdg_suite(void)
{
    Suite *s = suite_create("Paths XDG");
    TCase *tc = tcase_create("XDG Variables");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_xdg_config_home_set);
    // ... more tests

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = paths_xdg_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
```

### Makefile Integration

Add new test binaries to Makefile:
- `tests/unit/paths/env_vars_test`
- `tests/unit/paths/tilde_test`
- `tests/unit/paths/tool_dirs_test`
- `tests/unit/paths/getters_test`

Follow existing pattern in Makefile for test compilation and linking.

## Test Implementation

**Follow TDD workflow (Red/Green/Verify):**

**Step 1 - Red (Failing Test):**

For each test file:
1. Create test file with all test cases
2. Implement test logic using Check framework
3. Add to Makefile
4. Build: `make check`
5. Verify tests FAIL (because they test edge cases not yet covered)

**Step 2 - Green (Minimal Implementation):**

For each failing test:
1. Update `src/paths.c` implementation to handle the tested scenario
2. Run test again
3. Verify it passes
4. Move to next failing test

STOP when all tests pass.

**Step 3 - Verify:**
- Run `make check` - all tests must pass (basic + comprehensive)
- Run `make coverage` - verify 100% coverage of paths module
- Run `make lint` - complexity under threshold

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-comprehensive-tests.md): [success|partial|failed] - [brief description]

[Optional: Details about what was accomplished, failures, or remaining work]
EOF
)"
```

Report status to orchestration:
- Success: Task complete, 100% paths module coverage achieved
- Partial/Failed: Describe what's incomplete or failing

## Postconditions

- [ ] All 4 test files created (env_vars, tilde, tool_dirs, getters)
- [ ] 30+ new test cases added (beyond paths-core.md's 19 basic tests)
- [ ] All tests pass
- [ ] 100% coverage of paths module
- [ ] `make check` passes
- [ ] `make coverage` shows 100% for src/paths.c
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
