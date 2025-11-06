# Plan: Config Module Implementation

## Overview

Implement the config module (`src/config.c/h`) for loading and validating server configuration from `~/.ikigai/config.json`. Depends on logger module.

## Prerequisites

- Logger module completed (plan-logger.md)
- Review docs/phase-1.md and docs/phase-1-details.md for config specifications
- Review docs/memory.md for talloc patterns
- Review docs/error_handling.md for Result type usage

## Task List

### Task 1: Review and Verify Prerequisites

**Description**: Review previous work decisions and verify that logger module is complete and no architectural changes affect config module.

**Actions**:
- Verify logger module is complete and tested
- Read through docs/decisions.md for config-related decisions
- Verify docs/phase-1-details.md config section is current
- Check that talloc and error handling patterns are established
- Confirm no changes to config API or behavior

**Acceptance**:
- Logger module available for use
- No conflicts with previous decisions
- Config specification is clear and unchanged
- Ready to proceed with implementation

---

### Task 2: Create Config Header and Types (Red Phase)

**Description**: Define config structure and API in header file.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Create `tests/unit/config_test.c`
- Write test that attempts to use `ik_cfg_t` struct
- Write test that attempts to call `ik_cfg_load()`
- Test should fail to compile (types/functions don't exist)
- Run `make check` to verify test fails

**GREEN (Implement Minimum Code)**:
- Create `src/config.h`
- Add struct definition:
  ```c
  typedef struct {
    char *openai_api_key;
    char *listen_address;
    int listen_port;
  } ik_cfg_t;
  ```
- Add function declaration:
  ```c
  ik_result_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);
  ```
- Add include guards and necessary includes
- Update Makefile if needed
- Run `make check` to verify test now compiles

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add config.h types and function declarations"

---

### Task 3: Implement Tilde Expansion (Red Phase)

**Description**: Implement tilde expansion for HOME directory paths.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_config_expand_tilde()` that:
  - Creates temp context
  - Calls internal `expand_tilde()` helper with "~/test/path"
  - Verifies result is "$HOME/test/path"
  - Tests path without tilde (should return unchanged)
- Add test `test_config_expand_tilde_home_unset()` that:
  - Temporarily unsets HOME env var
  - Calls `expand_tilde()`
  - Verifies it returns NULL
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Create `src/config.c`
- Implement `static char *expand_tilde(TALLOC_CTX *ctx, const char *path)`:
  - Check if path starts with '~'
  - If not, return `talloc_strdup(ctx, path)`
  - Get HOME from `getenv("HOME")`
  - If HOME is NULL, return NULL
  - Return `talloc_asprintf(ctx, "%s%s", home, path + 1)`
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement tilde expansion for config paths"

---

### Task 4: Implement Config File Auto-Creation (Red Phase)

**Description**: Create config directory and file with defaults if missing.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_config_auto_create_directory()` that:
  - Uses temporary test directory
  - Deletes test config file if exists
  - Calls `ik_cfg_load()` with path to non-existent config
  - Verifies directory is created (mode 0755)
  - Verifies config file is created
  - Verifies file contains default JSON
- Add test `test_config_auto_create_defaults()` that:
  - Verifies default values are correct:
    - `openai_api_key: "YOUR_API_KEY_HERE"`
    - `listen_address: "127.0.0.1"`
    - `listen_port: 1984`
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Implement `ik_cfg_load()` skeleton:
  - Expand tilde in path
  - Check if file exists
  - If not, create directory with `mkdir()` (mode 0755)
  - Create default config file with:
    ```json
    {
      "openai_api_key": "YOUR_API_KEY_HERE",
      "listen_address": "127.0.0.1",
      "listen_port": 1984
    }
    ```
  - Return after creation (let next call load it)
- Use logger to log "Created default config at <path>"
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement config auto-creation with defaults"

---

### Task 5: Implement JSON Parsing (Red Phase)

**Description**: Parse config file and extract values.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_config_load_valid()` that:
  - Creates temp config file with valid JSON
  - Calls `ik_cfg_load()`
  - Verifies result is OK
  - Verifies cfg fields match file contents
  - Verifies memory is on provided context
- Add test `test_config_load_invalid_json()` that:
  - Creates temp config file with invalid JSON
  - Calls `ik_cfg_load()`
  - Verifies result is ERR with IK_ERR_PARSE
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Complete `ik_cfg_load()` implementation:
  - Read file contents (use `fopen()`, `fread()`)
  - Parse JSON with `json_loads()`
  - Check for parse errors, return `ERR(ctx, PARSE, ...)`
  - Extract fields with `json_object_get()`
  - Allocate `ik_cfg_t` on context
  - Copy strings to context with `talloc_strdup()`
  - Call `json_decref()` when done
  - Return `OK(config)`
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement JSON parsing for config file"

---

### Task 6: Implement Field Validation (Red Phase)

**Description**: Validate required fields exist and have correct types.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_config_missing_field_openai_key()` that:
  - Creates config missing `openai_api_key`
  - Verifies returns ERR with IK_ERR_PARSE
- Add test `test_config_missing_field_listen_address()` - similar
- Add test `test_config_missing_field_listen_port()` - similar
- Add test `test_config_wrong_type_port()` that:
  - Creates config with `listen_port` as string
  - Verifies returns ERR with IK_ERR_PARSE
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Add validation in `ik_cfg_load()`:
  - Check `openai_api_key` exists and is string
  - Check `listen_address` exists and is string
  - Check `listen_port` exists and is integer
  - Return `ERR(ctx, PARSE, "Missing field: ...")` if validation fails
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add config field validation"

---

### Task 7: Implement Port Range Validation (Red Phase)

**Description**: Validate port is in range 1024-65535 (non-privileged).

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_config_port_too_low()` that:
  - Creates config with `listen_port: 80`
  - Verifies returns ERR with IK_ERR_OUT_OF_RANGE
- Add test `test_config_port_too_high()` that:
  - Creates config with `listen_port: 70000`
  - Verifies returns ERR with IK_ERR_OUT_OF_RANGE
- Add test `test_config_port_valid_range()` that:
  - Tests port 1024 (minimum valid)
  - Tests port 65535 (maximum valid)
  - Tests port 1984 (default)
  - Verifies all return OK
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Add port validation in `ik_cfg_load()`:
  - After extracting port value
  - Check `port < 1024 || port > 65535`
  - Return `ERR(ctx, OUT_OF_RANGE, "Port must be 1024-65535, got %d", port)` if invalid
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add port range validation"

---

### Task 8: Test Memory Management (Red Phase)

**Description**: Verify proper talloc memory management and cleanup.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_config_memory_cleanup()` that:
  - Creates talloc context
  - Loads config
  - Verifies all strings are on correct context
  - Frees context with `talloc_free()`
  - Verifies no memory leaks (use talloc reports)
- Add test `test_config_oom_handling()` that:
  - Injects allocation failure
  - Verifies function returns ERR with IK_ERR_OOM
  - Verifies partial allocations are cleaned up
- Run `make check` to verify tests pass or fail appropriately

**GREEN (Fix Any Issues)**:
- Ensure all allocations are on provided context
- Ensure jansson objects are properly freed with `json_decref()`
- Add error path cleanup if needed
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add memory management tests for config"

---

### Task 9: Integration Tests (Red Phase)

**Description**: Test complete config loading flow end-to-end.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Create `tests/integration/config_integration_test.c`
- Add test `test_config_full_flow()` that:
  - Deletes test config if exists
  - Calls `ik_cfg_load()` - should create defaults
  - Calls `ik_cfg_load()` again - should load created file
  - Modifies file with valid values
  - Calls `ik_cfg_load()` - should load modified values
  - Verifies all values correct
- Run `make check` to verify test runs

**GREEN (Fix Any Issues)**:
- Fix any issues discovered during integration testing
- Run `make check` to verify all tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add config integration tests"

---

### Task 10: Manual Verification and Documentation Review

**Description**: Human verification of config functionality and code quality.

**Manual Tests**:
1. Delete `~/.ikigai/config.json` if exists
2. Run test suite: `make check`
3. Verify `~/.ikigai/` directory was created
4. Verify `~/.ikigai/config.json` has default values
5. Modify config with valid values, run tests again
6. Modify config with invalid JSON, verify error handling
7. Test with missing fields, wrong types, invalid ports

**Code Inspection**:
- Review config.c implementation for:
  - Proper talloc memory management
  - All allocations on provided context
  - jansson cleanup (json_decref)
  - Error handling on all paths
  - Logger usage for informational messages
  - No memory leaks in error paths

**Documentation**:
- Verify config.h has appropriate comments
- Ensure implementation matches specification exactly
- Check error messages are helpful

**Acceptance**:
- All manual tests pass
- Code quality is high
- Implementation matches specification
- Ready to proceed to next module (protocol)

---

## Success Criteria

- All tests pass (`make check`)
- Code complexity passes (`make lint`)
- Coverage is 100% for Lines, Functions, and Branches
- All commits follow format: "Brief description of change"
- No architectural changes made without explicit approval
- Config module ready for use by other modules
- Config auto-creation works correctly
- All validation rules implemented per specification

## Notes

- Config depends on logger module (must be complete first)
- Use talloc for all memory management
- Use Result types for error handling
- Do NOT validate API key content - server will start successfully
- Config is loaded once at startup (no hot-reload in Phase 1)
