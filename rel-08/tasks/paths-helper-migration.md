# Task: Migrate Test Helpers to Use Paths Module

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/extended
**Depends on:** paths-test-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded. Only list additional skills.)
- `/load di` - For dependency injection patterns

**Plan:**
- `rel-08/plan/paths-test-migration.md` - Category 1 (lines 13-65), Integration Points (lines 430-492)
- `rel-08/plan/paths-module.md` - Updated signature specification

**Source:**
- `tests/helpers/test_contexts.c` - Helper functions to update
- `tests/helpers/test_contexts.h` - Header declarations
- `src/shared.h` - Current `ik_shared_ctx_init()` signature
- `src/shared.c` - Current implementation

## Libraries

Use only:
- talloc - For memory management
- Existing ikigai modules (config, logger, paths, shared)

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] `paths-core.md` task completed
- [ ] `paths-test-infrastructure.md` task completed

## Objective

Update the test helper functions to use the paths module instead of hardcoded path strings. This is the **highest impact change** - these 2 helpers are used by ~160 other tests. Once fixed, most downstream tests will work without changes.

## Interface Changes

### Production Code: ik_shared_ctx_init()

**BEFORE (current signature):**
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

**AFTER (new signature):**
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_config_t *cfg,
    ik_paths_t *paths,          // NEW: replaces working_dir + ikigai_subdir
    ik_logger_t *logger,
    ik_shared_ctx_t **out
);
```

**Changes needed:**
1. Update signature in `src/shared.h`
2. Update implementation in `src/shared.c` - store paths pointer, use paths for directory resolution
3. Update production call site in `src/client.c`

### Test Helpers

**Helper 1: test_shared_ctx_create()**

**BEFORE:**
```c
res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    ik_config_t *cfg = test_cfg_create(ctx);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    return ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, out);
}
```

**AFTER:**
```c
res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    // Setup test environment
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

**Helper 2: test_shared_ctx_create_with_cfg()**

**BEFORE:**
```c
res_t test_shared_ctx_create_with_cfg(TALLOC_CTX *ctx, ik_config_t *cfg, ik_shared_ctx_t **out)
{
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    return ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, out);
}
```

**AFTER:**
```c
res_t test_shared_ctx_create_with_cfg(TALLOC_CTX *ctx, ik_config_t *cfg, ik_shared_ctx_t **out)
{
    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    if (is_err(&result)) return result;

    // Create logger
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // New signature: paths replaces working_dir + ikigai_subdir
    return ik_shared_ctx_init(ctx, cfg, paths, logger, out);
}
```

**Helper 3: test_repl_create()**

No changes needed - delegates to `test_shared_ctx_create()` internally, inherits fix automatically.

## Behaviors

### ik_shared_ctx_init() Implementation Update

**Current behavior:**
- Receives `working_dir` and `ikigai_subdir` as separate strings
- Constructs paths internally by concatenating strings
- No central path management

**New behavior:**
- Receives `ik_paths_t *paths` instance
- Stores paths pointer in shared context
- Uses paths module for all directory resolution
- No string concatenation

**Struct update:**
```c
// src/shared.h
struct ik_shared_ctx_t {
    ik_config_t *cfg;
    ik_paths_t *paths;      // NEW: replaces working_dir/ikigai_subdir strings
    ik_logger_t *logger;
    // ... other fields
};
```

### Test Environment Setup

**Pattern:**
1. Call `test_paths_setup_env()` to create isolated directories
2. Call `ik_paths_init()` to create paths instance
3. Paths instance will use environment variables set by setup
4. Pass paths instance to `ik_shared_ctx_init()`

**PID isolation:**
- Each test process gets `/tmp/ikigai_test_${PID}/` directories
- Parallel test execution doesn't interfere
- Environment variables scoped to process

### Error Handling

**If paths initialization fails:**
- Return error immediately from helper
- Do NOT continue with NULL paths
- Error propagates to calling test (test will fail, showing the error)

**Memory ownership:**
- Paths instance allocated on provided context
- Freed when context is freed
- Test helpers don't need explicit cleanup

## Test Implementation

**Follow TDD workflow (Red/Green/Verify):**

**Step 1 - Red (Failing Test):**

Tests for helpers already exist in `tests/helpers/test_contexts_test.c`. These will break when we update signatures.

Expected compilation errors:
- `src/client.c` - Wrong parameter count for `ik_shared_ctx_init()`
- 87+ test files - Wrong parameter count for `ik_shared_ctx_init()`
- All files using test helpers will break until helpers are fixed

**Fix order:**
1. Update `src/shared.h` and `src/shared.c` signatures first
2. Update `src/client.c` to compile
3. Update test helpers
4. Verify helper tests compile and pass
5. Verify dependent tests start passing

**Step 2 - Green (Minimal Implementation):**

1. **Update production signature:**
   - Modify `ik_shared_ctx_init()` signature in `src/shared.h`
   - Update implementation in `src/shared.c`:
     - Change parameters from `working_dir` + `ikigai_subdir` to `paths`
     - Store `paths` pointer in struct
     - Use paths module getters for directory access

2. **Update production call site:**
   - In `src/client.c`, create paths instance before calling `ik_shared_ctx_init()`
   - Remove hardcoded `cwd` and `".ikigai"` arguments
   - Pass paths instance instead

3. **Update test helper #1:**
   - In `tests/helpers/test_contexts.c`, update `test_shared_ctx_create()`
   - Add `test_paths_setup_env()` call
   - Add `ik_paths_init()` call
   - Pass paths to `ik_shared_ctx_init()`

4. **Update test helper #2:**
   - Update `test_shared_ctx_create_with_cfg()` similarly
   - Same pattern: setup env, create paths, pass to init

STOP when helper tests pass and dependent tests compile.

**Step 3 - Verify:**
- Run `make check` - verify helper tests pass
- Count compilation errors - should be much lower (only direct callers remain)
- Verify ~160 helper-using tests now compile

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-helper-migration.md): [success|partial|failed] - [brief description]

[Optional: Details about what was accomplished, failures, or remaining work]
EOF
)"
```

Report status to orchestration:
- Success: Task complete, helpers migrated, ~160 tests compile again
- Partial/Failed: Describe what's incomplete or failing

## Postconditions

- [ ] `src/shared.h` signature updated
- [ ] `src/shared.c` implementation updated
- [ ] `src/client.c` updated and compiles
- [ ] Test helpers updated (2 functions)
- [ ] Helper tests pass
- [ ] Compilation errors reduced significantly
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
