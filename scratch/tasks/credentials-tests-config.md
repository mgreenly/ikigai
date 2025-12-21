# Task: Update Config Tests for Credentials API

**Layer:** 0
**Model:** sonnet/none
**Depends on:** credentials-tests-helpers.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/credentials.h` - New credentials API
- `src/config.h` - Config struct (no longer has `openai_api_key`)

## Objective

Update all config unit tests to remove `openai_api_key` references. Two types of changes are required: removing `cfg->openai_api_key` struct field access and removing `"openai_api_key"` from JSON test fixtures. This ensures config tests align with the new credential-free config structure.

## Interface

No new interfaces - this task removes obsolete test code.

## Behaviors

### Struct Field Access Removal

Remove all lines that:
- Assign to `cfg->openai_api_key`
- Assert on `cfg->openai_api_key` values

### JSON Fixture Cleanup

Remove `"openai_api_key"` from all JSON test strings in config tests.

### Test Deletion

Delete or repurpose tests that specifically validated `openai_api_key`:
- Tests for missing `openai_api_key` (no longer applicable)
- Tests for `openai_api_key` type validation (no longer applicable)

### Files to Update

| File | Changes |
|------|---------|
| `tests/unit/config/basic_test.c` | Remove struct access + 1 JSON fixture |
| `tests/unit/config/config_test.c` | 7 JSON fixtures |
| `tests/unit/config/validation_missing_test.c` | Delete 1 test + 6 JSON fixtures |
| `tests/unit/config/validation_types_test.c` | Delete 1 test + 10 JSON fixtures |
| `tests/unit/config/validation_ranges_test.c` | 11 JSON fixtures |
| `tests/unit/config/tool_limits_test.c` | 7 JSON fixtures |
| `tests/unit/config/history_size_test.c` | 7 JSON fixtures |
| `tests/unit/config/filesystem_test.c` | 1 JSON fixture |

### Removal Pattern

From each test file, remove patterns like:

**Struct access:**
```c
cfg->openai_api_key = talloc_strdup(ctx, "test_key");
ck_assert_str_eq(cfg->openai_api_key, "test_key");
ck_assert_ptr_nonnull(cfg->openai_api_key);
```

**JSON fixtures:**
```c
"{\"openai_api_key\": \"test-key\", \"openai_model\": \"gpt-4\", ...}"
// Remove the openai_api_key field entirely
"{\"openai_model\": \"gpt-4\", ...}"
```

## Test Scenarios

All existing config tests should continue to pass after removing `openai_api_key` references. Tests verify:
- Config parsing still works without `openai_api_key`
- Validation of other fields continues to function
- Default config creation works
- File loading and error handling work

## Postconditions

- [ ] `grep -r "openai_api_key" tests/unit/config/` returns nothing
- [ ] All config tests compile without errors
- [ ] `make build/tests/unit/config/basic_test` succeeds (and all other config tests)
- [ ] `./build/tests/unit/config/basic_test` passes (and all other config tests)
- [ ] No test validates presence or type of `openai_api_key`

## Verification

```bash
# Verify no references remain
grep -r "openai_api_key" tests/unit/config/
# Should return nothing

# Build all config tests
make build/tests/unit/config/basic_test
make build/tests/unit/config/config_test
make build/tests/unit/config/validation_missing_test
make build/tests/unit/config/validation_types_test
make build/tests/unit/config/validation_ranges_test
make build/tests/unit/config/tool_limits_test
make build/tests/unit/config/history_size_test
make build/tests/unit/config/filesystem_test

# Run one to verify
./build/tests/unit/config/basic_test
# Should pass
```
