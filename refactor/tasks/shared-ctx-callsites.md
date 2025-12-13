# Task: Update All Call Sites for ik_shared_ctx_init

## Target

Update all call sites across the codebase to use the new `ik_shared_ctx_init()` signature with logger and ikigai_path parameters.

## Context

The previous task (`shared-ctx-signature.md`) updated the function signature to accept logger and ikigai_path as explicit parameters. Now we need to find all places that call this function and update them to:

1. Create a logger before calling `ik_shared_ctx_init()` (if not already created)
2. Pass `.ikigai` as the ikigai_path parameter (standard default)
3. Pass the created logger

This task involves updating multiple files across the codebase. To avoid context overload, you should **use sub-agents** to handle groups of call sites.

## Pre-read

### Skills
- `di.md` - Dependency injection patterns
- `tdd.md` - Test-driven development
- `naming.md` - Naming conventions
- `errors.md` - Error handling patterns
- `scm.md` - Git commit practices
- `default.md` - Project overview

### Source Code
- `src/shared.h` - New function signature (reference)
- `src/logger.c` - Logger creation function (`ik_logger_create`)
- `src/client.c` - Main entry point (likely has a call site)

### Test Patterns
- Various test files that create shared contexts

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. `shared-ctx-signature.md` task completed (signature updated)
3. All tests pass with updated signature in test file

## Task

Find and update all call sites for `ik_shared_ctx_init()` across the codebase.

## Process

### Step 1: Find All Call Sites

Use Grep to find all calls to `ik_shared_ctx_init()`:

```bash
# Search for all call sites (exclude the definition and tests/unit/shared/shared_test.c which is already done)
```

You should find call sites in:
- Production code (e.g., `src/client.c`)
- Integration tests (e.g., `tests/integration/`)
- Other unit tests that create shared contexts

**IMPORTANT**: Exclude `tests/unit/shared/shared_test.c` from your updates - that was already updated in the previous task.

### Step 2: Group Call Sites

Group the call sites by file or logical area. For example:
- Group 1: Production code (`src/client.c`, etc.)
- Group 2: Integration tests
- Group 3: Other unit tests

### Step 3: Use Sub-Agents for Updates

**DO NOT update all call sites yourself** - use the Task tool to spawn sub-agents for each group.

For each group, spawn a sub-agent with this prompt template:

```
## Task

Update call sites for ik_shared_ctx_init() in the following files:
[LIST OF FILES]

## Context

The function signature has been updated to:
```c
res_t ik_shared_ctx_init(
    TALLOC_CTX *ctx,
    ik_cfg_t *cfg,
    const char *working_dir,
    const char *ikigai_path,     // e.g., ".ikigai"
    ik_logger_t *logger,          // Pre-created logger
    ik_shared_ctx_t **out
);
```

## Pre-read

Read these files before starting:
- `src/shared.h` - New function signature
- `src/logger.c` - Logger creation
- [FILES TO UPDATE]

## Steps

For each file:

1. **Read the file** to understand the current call site
2. **Check if logger already exists** in scope
   - If yes: Use existing logger
   - If no: Create logger before the call using `ik_logger_create(ctx, working_dir)`
3. **Update the call** to include ikigai_path and logger:
   ```c
   // Before:
   res_t result = ik_shared_ctx_init(ctx, cfg, working_dir, &shared);

   // After:
   ik_logger_t *logger = ik_logger_create(ctx, working_dir);
   res_t result = ik_shared_ctx_init(ctx, cfg, working_dir, ".ikigai", logger, &shared);
   ```
4. **Handle errors** from logger creation if needed
5. **Run tests** after each file update:
   ```bash
   make check
   ```
6. **Commit each file** separately:
   ```bash
   git add [FILE]
   git commit -m "refactor(callsite): update ik_shared_ctx_init call in [FILE]"
   ```

## Success Criteria

- All call sites in assigned files updated
- Tests pass after each update
- Each file committed separately
- Working tree clean

Report status: "completed" or "stuck with reason"
```

### Step 4: Monitor Sub-Agents

For each sub-agent:
- Wait for completion
- Verify tests still pass
- If stuck, intervene or escalate

### Step 5: Final Verification

After all sub-agents complete:

1. **Search again** to verify no call sites were missed:
   ```bash
   # Search for old pattern (should find none except the definition)
   ```

2. **Run full test suite**:
   ```bash
   make check
   make lint
   ```

3. **Create a summary commit** (if needed) documenting the migration:
   ```bash
   git commit --allow-empty -m "$(cat <<'EOF'
chore: complete ik_shared_ctx_init callsite migration

All call sites updated to use new signature with logger and ikigai_path DI.

Updated files:
[LIST OF FILES]

All tests passing, working tree clean.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
EOF
)"
   ```

## Expected Patterns

### Production Code Example

**Before** (`src/client.c`):
```c
ik_shared_ctx_t *shared = NULL;
res_t result = ik_shared_ctx_init(root_ctx, cfg, working_dir, &shared);
if (is_err(&result)) {
    return result;
}
```

**After** (`src/client.c`):
```c
ik_logger_t *logger = ik_logger_create(root_ctx, working_dir);
ik_shared_ctx_t *shared = NULL;
res_t result = ik_shared_ctx_init(root_ctx, cfg, working_dir, ".ikigai", logger, &shared);
if (is_err(&result)) {
    return result;
}
```

### Test Code Example

**Before** (test file):
```c
ik_shared_ctx_t *shared = NULL;
res_t result = ik_shared_ctx_init(ctx, cfg, test_dir, &shared);
ck_assert(is_ok(&result));
```

**After** (test file):
```c
ik_logger_t *logger = ik_logger_create(ctx, test_dir);
ik_shared_ctx_t *shared = NULL;
res_t result = ik_shared_ctx_init(ctx, cfg, test_dir, ".ikigai", logger, &shared);
ck_assert(is_ok(&result));
```

## Post-conditions

1. All call sites for `ik_shared_ctx_init()` updated with new signature
2. Logger created before each call (or existing logger used)
3. Default `.ikigai` path passed in all cases
4. All tests pass (`make check`)
5. Lint passes (`make lint`)
6. Each updated file committed separately
7. Working tree is clean

## Notes

- **Use sub-agents** - don't try to update all files yourself
- **One file per commit** - makes it easy to review and revert if needed
- **Test after each file** - catch issues early
- **Exclude shared_test.c** - already updated in previous task

## Expected Outcome

```json
{"ok": true}
```

Or if blocked:

```json
{"ok": false, "reason": "clear explanation of what's blocking progress"}
```
