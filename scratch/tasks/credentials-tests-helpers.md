# Task: Update Test Helpers for Credentials API

**Layer:** 0
**Model:** sonnet/none
**Depends on:** credentials-production.md

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

## Verification

```bash
# Verify no references remain
grep -r "openai_api_key" tests/test_utils.c tests/helpers/ tests/unit/helpers/
# Should return nothing

# Build and run helper tests
make build/tests/unit/helpers/test_contexts_test && ./build/tests/unit/helpers/test_contexts_test
# Should pass
```
