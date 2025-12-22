# Task: Update Test Helpers for Credentials API

**Model:** sonnet/none
**Depends on:** credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/credentials.h` - New credentials API
- `tests/test_utils.c` - Test helper that sets `cfg->openai_api_key`
- `tests/helpers/test_contexts.c` - Test context helper

## Objective

Update test helper infrastructure to remove `cfg->openai_api_key` references. This is the first credentials-tests task and must complete before other test files can be updated, as helper functions are used widely across the test suite.

## Interface

No new interfaces - this task removes obsolete code from existing helpers.

## Behaviors

### Test Utility Updates

- Remove `cfg->openai_api_key` assignment from `tests/test_utils.c`
- Tests that need credentials will set environment variable `OPENAI_API_KEY` in their own setup functions

### Test Context Updates

- Remove `cfg->openai_api_key = NULL` initialization from `tests/helpers/test_contexts.c`
- Remove assertions about `openai_api_key` field from `tests/unit/helpers/test_contexts_test.c`

### Files to Modify

| File | Change |
|------|--------|
| `tests/test_utils.c` | Remove `cfg->openai_api_key = talloc_strdup(cfg, "test-api-key")` line |
| `tests/helpers/test_contexts.c` | Remove `cfg->openai_api_key = NULL` line |
| `tests/unit/helpers/test_contexts_test.c` | Remove assertions like `ck_assert_ptr_null(ctx->cfg->openai_api_key)` |

## Test Scenarios

- **Test contexts creation**: Verify test context helpers create configs without `openai_api_key` field
- **Helper tests pass**: Run `test_contexts_test` to verify helper tests still pass after removal

## Postconditions

- [ ] `grep -r "openai_api_key" tests/test_utils.c` returns nothing
- [ ] `grep -r "openai_api_key" tests/helpers/` returns nothing
- [ ] `grep -r "openai_api_key" tests/unit/helpers/` returns nothing
- [ ] `make build/tests/unit/helpers/test_contexts_test` succeeds
- [ ] `./build/tests/unit/helpers/test_contexts_test` passes

- [ ] Changes committed to git with message: `task: credentials-tests-helpers.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Verify no references remain
grep -r "openai_api_key" tests/test_utils.c tests/helpers/ tests/unit/helpers/
# Should return nothing

# Build and run helper tests
make build/tests/unit/helpers/test_contexts_test && ./build/tests/unit/helpers/test_contexts_test
# Should pass
```


## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).