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

### 1. tests/unit/paths/xdg_test.c

**Test scenarios:**

- `test_xdg_config_home_set` - Verify `XDG_CONFIG_HOME` used when set
- `test_xdg_config_home_default` - Verify `~/.config/ikigai/` when not set
- `test_xdg_data_home_set` - Verify `XDG_DATA_HOME` used when set
- `test_xdg_data_home_default` - Verify `~/.local/share/ikigai/` when not set
- `test_xdg_state_home_set` - Verify `XDG_STATE_HOME` used when set
- `test_xdg_state_home_default` - Verify `~/.local/state/ikigai/` when not set
- `test_xdg_with_ikigai_override` - Verify `IKIGAI_CONFIG_DIR` takes precedence over XDG
- `test_xdg_multiple_vars_set` - Verify all XDG vars can be set simultaneously

**Pattern:**
```c
START_TEST(test_xdg_config_home_set)
{
    // Setup
    test_paths_setup_env();
    setenv("XDG_CONFIG_HOME", "/custom/config", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_ok(&result));
    const char *config_dir = ik_paths_get_config_dir(paths);
    ck_assert_str_eq(config_dir, "/custom/config/ikigai");

    // Cleanup
    test_paths_cleanup_env();
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

- `test_tools_system_dir_dev_mode` - NULL in development mode
- `test_tools_system_dir_user_install` - NULL in user install
- `test_tools_system_dir_system_install` - `<prefix>/libexec/ikigai/tools/` in system install
- `test_tools_user_dir_always_set` - Always `~/.ikigai/tools/` regardless of install type
- `test_tools_project_dir_always_set` - Always `./.ikigai/tools/` regardless of install type
- `test_tools_all_three_accessible` - All getters work simultaneously
- `test_tools_system_dir_with_custom_prefix` - Verify prefix extraction works

**Pattern:**
```c
START_TEST(test_tools_system_dir_dev_mode)
{
    // Setup - development mode
    test_paths_setup_env();
    setenv("IKIGAI_DEV", "1", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - system tools dir should be NULL in dev mode
    const char *system_dir = ik_paths_get_tools_system_dir(paths);
    ck_assert_ptr_null(system_dir);

    // But user and project dirs should be set
    ck_assert_ptr_nonnull(ik_paths_get_tools_user_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_tools_project_dir(paths));

    test_paths_cleanup_env();
}
END_TEST
```

### 4. tests/unit/paths/config_search_test.c

**Test scenarios:**

- `test_config_dir_dev_mode` - Uses `IKIGAI_CONFIG_DIR` or project root
- `test_config_dir_user_install` - Uses `~/.config/ikigai/`
- `test_config_dir_system_install` - Uses `~/.config/ikigai/` (user config takes precedence)
- `test_config_dir_xdg_override` - Uses `XDG_CONFIG_HOME/ikigai/`
- `test_config_dir_ikigai_override` - `IKIGAI_CONFIG_DIR` overrides XDG

**Note:** This tests the config directory resolution, not the actual config file search (that's tested in config module tests).

### 5. tests/unit/paths/tool_precedence_test.c

**Test scenarios:**

- `test_tool_precedence_order` - Verify project > user > system order documented
- `test_tool_override_project_shadows_user` - Project tool shadows user tool
- `test_tool_override_project_shadows_system` - Project tool shadows system tool
- `test_tool_override_user_shadows_system` - User tool shadows system tool

**Note:** These are documentation/specification tests. The actual tool discovery precedence is implemented in the discovery infrastructure task, but paths module documents the intended order.

### 6. tests/unit/paths/env_override_test.c

**Test scenarios:**

- `test_ikigai_dev_forces_dev_mode` - Any value of `IKIGAI_DEV` forces development mode
- `test_ikigai_config_dir_override` - Overrides config directory
- `test_ikigai_data_dir_override` - Overrides data directory
- `test_ikigai_libexec_dir_override` - Overrides libexec directory
- `test_all_overrides_together` - All IKIGAI_*_DIR vars can be set simultaneously
- `test_override_priority` - IKIGAI_* vars take precedence over XDG vars

**Pattern:**
```c
START_TEST(test_ikigai_config_dir_override)
{
    test_paths_setup_env();
    setenv("IKIGAI_CONFIG_DIR", "/custom/config", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    const char *config_dir = ik_paths_get_config_dir(paths);
    ck_assert_str_eq(config_dir, "/custom/config");

    test_paths_cleanup_env();
}
END_TEST
```

### 7. tests/unit/paths/install_type_test.c

**Test scenarios:**

- `test_install_type_dev_mode_detection` - `IKIGAI_DEV` set → INSTALL_TYPE_DEV
- `test_install_type_user_install_detection` - Binary in `~/.local/bin/` → INSTALL_TYPE_USER
- `test_install_type_system_install_detection` - Binary in `/usr/bin/` → INSTALL_TYPE_SYSTEM
- `test_install_type_prefix_extraction_usr_local` - `/usr/local/bin/ikigai` → prefix `/usr/local`
- `test_install_type_prefix_extraction_usr` - `/usr/bin/ikigai` → prefix `/usr`
- `test_install_type_prefix_extraction_opt` - `/opt/ikigai/bin/ikigai` → prefix `/opt/ikigai`

**Note:** Install type detection may need mocking of `/proc/self/exe` readlink for testing different binary locations.

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
- `tests/unit/paths/xdg_test`
- `tests/unit/paths/tilde_test`
- `tests/unit/paths/tool_dirs_test`
- `tests/unit/paths/config_search_test`
- `tests/unit/paths/tool_precedence_test`
- `tests/unit/paths/env_override_test`
- `tests/unit/paths/install_type_test`

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

- [ ] All 7 test files created
- [ ] 50+ new test cases added
- [ ] All tests pass
- [ ] 100% coverage of paths module
- [ ] `make check` passes
- [ ] `make coverage` shows 100% for src/paths.c
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
