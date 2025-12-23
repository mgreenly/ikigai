# Task: Implement Multi-Provider Configuration

**Model:** sonnet/none
**Depends on:** credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result type patterns

**Source:**
- `src/config.c` - Existing configuration loading
- `src/config.h` - Configuration struct definition
- `src/credentials.h` - Credentials API (separate from config)

**Plan:**
- `scratch/plan/configuration.md` - Full specification

## Objective

Extend config.json to support multi-provider settings by adding default provider selection. Per-provider defaults (model, thinking level) are hardcoded in provider implementations, not stored in config files. This task focuses on configuration structure only - credentials loading is handled by the separate credentials API implemented in credentials-core.md.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_config_load(TALLOC_CTX *ctx, const char *path, ik_config_t **out)` | Load and parse config.json with new multi-provider format |
| `const char *ik_config_get_default_provider(ik_config_t *config)` | Get default provider name with env var override support |

Structs to update:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_config_t` | `char *db_connection_string` - Database connection string (existing)<br>`char *default_provider` - Default provider name ("anthropic", "openai", "google") | Main configuration including existing and new fields |

### Struct Definitions

**ik_config_t** - Main configuration struct:
```c
typedef struct {
    char *db_connection_string;           // Existing field (unchanged)
    char *default_provider;               // NEW: "anthropic", "openai", "google"
    // ... other existing fields unchanged
} ik_config_t;
```

**Note:** Per-provider defaults (model, thinking level) are hardcoded in provider implementations, not stored in the config struct.

Files to update:

- `src/config.c` - Parse new JSON format
- `src/config.h` - Add new struct members

## Behaviors

### Configuration Loading
- Parse existing fields: `db_connection_string`, etc. (unchanged)
- Parse new field: `default_provider` (string)
- Return ERR_PARSE if JSON is malformed
- Return OK with partial config if optional fields missing

### Backward Compatibility
- Support old config format (no `default_provider`)
- Fall back to hardcoded defaults if new fields missing
- Emit warning if using deprecated format (optional)

### Default Provider Selection
- Check `IKIGAI_DEFAULT_PROVIDER` environment variable first
- Use config.json `default_provider` if env var not set
- Fall back to hardcoded default ("openai") if neither present
- Return ERR_INVALID_ARG if provider name is empty string

### Provider Defaults

Per-provider defaults (model, thinking level) are hardcoded in provider implementations:
- anthropic: model="claude-sonnet-4-5", thinking="med"
- openai: model="gpt-4o", thinking="none"
- google: model="gemini-2.5-flash", thinking="med"

These defaults are NOT stored in config files. Config only specifies which provider to use by default.

### Memory Management
- Config allocated on provided talloc context
- All strings allocated on config context
- Caller owns returned config

### Environment Variable Override
- `IKIGAI_DEFAULT_PROVIDER` overrides config file setting
- Empty string treated as unset (fall through to config file)
- No other environment variables for config (credentials use separate env vars)

## Configuration Format

### New Format (rel-07)
```json
{
  "db_connection_string": "postgresql://...",
  "default_provider": "anthropic"
}
```

**Note:** Per-provider defaults (model, thinking level) are hardcoded in provider implementations. Config only specifies which provider to use by default.

### Legacy Format (backward compatible)
```json
{
  "db_connection_string": "postgresql://..."
}
```

### Supported Providers (rel-07 scope)
- `anthropic` - Claude models
- `openai` - GPT and reasoning models
- `google` - Gemini models

**Note:** Plan documents mention xAI and Meta providers, but these are out of scope for rel-07.

## Test Scenarios

### Configuration Loading
- Load config with new format: returns OK with all fields
- Load config with legacy format: returns OK with defaults
- Load malformed JSON: returns ERR_PARSE

### Default Provider Selection
- Config specifies "anthropic": returns "anthropic"
- Env var set to "google": overrides config, returns "google"
- Neither set: returns hardcoded default "openai"
- Empty string in config: treated as unset, uses hardcoded default


### Backward Compatibility
- Old config format loads successfully
- Existing fields (db_connection_string) still accessible
- Missing new fields don't cause errors

## Migration Strategy

**CRITICAL: This is an ATOMIC change. All call sites must be updated in the same commit.**

This task makes breaking API changes that affect 65+ call sites across 13 files. The changes cannot be made incrementally - any partial migration will break the build. The entire migration must happen atomically in a single commit.

### Migration Approach

1. **Atomic Update**: All call sites updated simultaneously in one commit
2. **No Deprecation Period**: No backward compatibility layer (would add complexity)
3. **Single Commit**: Function rename, signature change, type rename, and all call site updates in one atomic commit
4. **Build Validation**: `make check` must pass after the atomic update

### Breaking Changes

This migration includes THREE breaking changes that must all be applied together:

1. **Function Rename**: `ik_cfg_load` → `ik_config_load`
2. **Signature Change**: Added output parameter `ik_config_t **out` (third parameter)
3. **Type Rename**: `ik_cfg_t` → `ik_config_t`

All three changes are interdependent and must be applied atomically.

## Call Site Updates

The task renames `ik_cfg_load()` to `ik_config_load()` with a new signature. All existing call sites must be updated.

### Signature Change

**Old:**
```c
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);
```

**New:**
```c
res_t ik_config_load(TALLOC_CTX *ctx, const char *path, ik_config_t **out);
```

**Key differences:**
- Function renamed: `ik_cfg_load` → `ik_config_load`
- Added output parameter: `ik_config_t **out` (third parameter)
- Config struct now returned via output parameter instead of embedded in result

### Type Rename

**Old:**
```c
typedef struct { ... } ik_cfg_t;
```

**New:**
```c
typedef struct { ... } ik_config_t;
```

All declarations of `ik_cfg_t *cfg` must be changed to `ik_config_t *config`.

### Files to Update

**IMPORTANT:** The type rename `ik_cfg_t` → `ik_config_t` affects many more files than just those calling `ik_cfg_load()`. The type is used throughout the codebase in function parameters, struct members, and local variables.

**Core source files (type references and function calls):**
- `src/config.h` - Type definition and function declarations
- `src/config.c` - Function implementations
- `src/client.c` - 1 `ik_cfg_load()` call site
- `src/shared.h` - Struct member: `ik_cfg_t *cfg`
- `src/shared.c` - Function parameter references
- `src/openai/client.h` - Function parameter: `const ik_cfg_t *cfg`
- `src/openai/client.c` - Function implementations
- `src/openai/client_multi.h` - Struct member references
- `src/openai/client_multi_request.c` - Local variables
- `src/repl/agent_restore.c` - Function parameter references

**Test helper files:**
- `tests/test_utils.h` - Helper function declarations
- `tests/test_utils.c` - Helper function implementations
- `tests/helpers/test_contexts.h` - Test context struct definitions
- `tests/helpers/test_contexts.c` - Test context implementations

**Unit tests (tests/unit/config/) - direct ik_cfg_load() call sites:**
- `basic_test.c` - 6 call sites
- `config_test.c` - 7 call sites
- `filesystem_test.c` - 2 call sites
- `history_size_test.c` - 7 call sites
- `tilde_test.c` - 1 call site
- `tool_limits_test.c` - 7 call sites
- `validation_missing_test.c` - 7 call sites
- `validation_ranges_test.c` - 10 call sites
- `validation_types_test.c` - 9 call sites

**Other unit tests (type references in test setup):**
- All tests under `tests/unit/openai/` (20+ files) - use test helper functions that take `ik_cfg_t`
- All tests under `tests/unit/repl/` (20+ files) - use test contexts with `ik_cfg_t`
- All tests under `tests/unit/commands/` (30+ files) - use test contexts with `ik_cfg_t`
- Other unit test files - various helper function calls

**Integration tests:**
- `tests/integration/config_integration_test.c` - 8 `ik_cfg_load()` call sites
- All other integration tests (20+ files) - use test contexts with `ik_cfg_t`

**Total impact:**
- **Direct ik_cfg_load() calls:** 65+ call sites across 13 files
- **Type references (ik_cfg_t):** 130+ files throughout the codebase
- **Critical path:** 10 core source files + 4 test helper files must be updated for build to succeed

### Update Pattern

**Example 1: Basic usage in src/client.c**

Old pattern:
```c
res_t cfg_result = ik_cfg_load(root_ctx, "~/.config/ikigai/config.json");
if (is_err(&cfg_result)) {
    // handle error
    return EXIT_FAILURE;
}
ik_cfg_t *cfg = cfg_result.ok;
// use cfg
```

New pattern:
```c
ik_config_t *config = NULL;
res_t cfg_result = ik_config_load(root_ctx, "~/.config/ikigai/config.json", &config);
if (is_err(&cfg_result)) {
    // handle error
    return EXIT_FAILURE;
}
// use config
```

**Example 2: Test usage pattern**

Old pattern:
```c
res_t result = ik_cfg_load(ctx, test_file);
ck_assert(!result.is_err);
ik_cfg_t *cfg = result.ok;
ck_assert_str_eq(cfg->db_connection_string, expected_value);
```

New pattern:
```c
ik_config_t *config = NULL;
res_t result = ik_config_load(ctx, test_file, &config);
ck_assert(!result.is_err);
ck_assert_str_eq(config->db_connection_string, expected_value);
```

**Example 3: Error checking pattern**

Old pattern:
```c
res_t result = ik_cfg_load(ctx, test_file);
ck_assert(result.is_err);
ck_assert_int_eq(result.err->code, ERR_PARSE);
```

New pattern:
```c
ik_config_t *config = NULL;
res_t result = ik_config_load(ctx, test_file, &config);
ck_assert(result.is_err);
ck_assert_int_eq(result.err->code, ERR_PARSE);
```

**Key Pattern Changes:**

1. **Declare output variable first**: `ik_config_t *config = NULL;` (initialize to NULL)
2. **Pass output parameter**: Add `&config` as third argument to `ik_config_load()`
3. **Remove result extraction**: Delete `ik_cfg_t *cfg = result.ok;` line
4. **Update variable names**: Change `cfg` to `config` throughout
5. **Update type references**: Change all `ik_cfg_t` to `ik_config_t`

### Automated Migration Tools

Given the massive scope (130+ files), use automated tools to perform the bulk of the migration safely:

**Recommended approach using `sed` for global replacements:**

```bash
# Phase 1: Global type rename (ik_cfg_t → ik_config_t)
find src/ tests/ -type f \( -name "*.c" -o -name "*.h" \) -exec sed -i 's/ik_cfg_t/ik_config_t/g' {} +

# Verify type rename completed
grep -r "ik_cfg_t" src/ tests/  # Should return no results

# Phase 2: Function rename (ik_cfg_load → ik_config_load)
find src/ tests/ -type f \( -name "*.c" -o -name "*.h" \) -exec sed -i 's/ik_cfg_load/ik_config_load/g' {} +

# Verify function rename completed
grep -r "ik_cfg_load" src/ tests/  # Should return no results
```

**After automated replacements:**
- Manually update function signatures in `src/config.h` and `src/config.c` (add output parameter)
- Manually update all 65+ call sites to use output parameter pattern
- Run `make check` to verify

**CRITICAL:** Do not commit until ALL changes complete and `make check` passes.

### Implementation Order (Atomic Migration)

Given the scope (130+ files affected), a systematic approach is essential:

**Phase 1: Global type rename (affects all 130+ files)**
1. Use automated search-and-replace (see "Automated Migration Tools" above)
   - Replace `ik_cfg_t` → `ik_config_t` throughout entire codebase
   - This is safe because it's a pure type rename with no behavioral changes
   - Verify: `grep -r "ik_cfg_t" src/ tests/` should return no results after this step

**Phase 2: Update API signatures (14 core files)**
2. **Update src/config.h**:
   - Update function declaration: `ik_cfg_load` → `ik_config_load` with new signature
   - Keep internal helper functions unchanged

3. **Update src/config.c**:
   - Update function implementation to match new signature
   - Change return pattern from `OK(config)` to output parameter pattern

4. **Update core source files** (10 files):
   - `src/client.c` - Update single `ik_cfg_load()` call site
   - `src/shared.h`, `src/shared.c` - Already updated by type rename
   - `src/openai/*.{h,c}` - Already updated by type rename
   - `src/repl/agent_restore.c` - Already updated by type rename

5. **Update test helpers** (4 files):
   - `tests/test_utils.{h,c}` - Update any helper functions that call `ik_cfg_load()`
   - `tests/helpers/test_contexts.{h,c}` - Update test context creation functions

**Phase 3: Update all test call sites (100+ files)**
6. **Update config unit tests** (9 files with direct call sites):
   - Each file: update all `ik_cfg_load()` calls following the pattern

7. **Update config integration test** (1 file):
   - Update all 8 `ik_cfg_load()` call sites

8. **Update other test files** (100+ files):
   - These should work automatically after Phase 1 type rename
   - Only need updates if they directly call `ik_cfg_load()` (most don't)

**Phase 4: Verification**
9. **Compile and test**:
   - Run `make check` to verify all changes work together
   - All tests must pass
   - Fix any remaining issues discovered by compiler/tests

10. **Final verification**:
    - Verify no `ik_cfg_t` references remain: `grep -r "ik_cfg_t" src/ tests/`
    - Verify no `ik_cfg_load` references remain: `grep -r "ik_cfg_load" src/ tests/`
    - All changes in single commit - no intermediate commits

### Migration Checklist

Before committing the atomic migration, verify:

**Type Rename:**
- [ ] All 130+ `ik_cfg_t` references changed to `ik_config_t` (verify: `grep -r "ik_cfg_t" src/ tests/` returns nothing)
- [ ] All variable names updated (`cfg` → `config` recommended)

**Function Rename:**
- [ ] All 65+ `ik_cfg_load()` call sites changed to `ik_config_load()` (verify: `grep -r "ik_cfg_load" src/ tests/` returns nothing)
- [ ] All call sites use new output parameter pattern (`&config` as third arg)
- [ ] All `result.ok` extractions removed

**Core Files:**
- [ ] `src/config.h` - Declaration updated with new signature
- [ ] `src/config.c` - Implementation updated with output parameter pattern
- [ ] `src/client.c` - Single call site updated
- [ ] Test helper files updated (test_utils, test_contexts)

**Build Verification:**
- [ ] `make check` passes
- [ ] No compiler warnings or errors
- [ ] All unit tests pass
- [ ] All integration tests pass

**Git Verification:**
- [ ] All changes in single atomic commit
- [ ] No intermediate commits
- [ ] Commit message follows convention: `task: configuration.md - <summary>`

## Postconditions

- [ ] Config loads new format with `default_provider`
- [ ] `ik_config_get_default_provider()` returns correct value
- [ ] Existing config fields (db_connection_string, etc.) still work
- [ ] `IKIGAI_DEFAULT_PROVIDER` env var overrides config file
- [ ] Legacy config format still loads successfully
- [ ] All call sites updated to use `ik_config_load()` with new signature
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: configuration.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).