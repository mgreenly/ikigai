# Task: Update REPL Tests for Credentials API

**Model:** sonnet/none
**Depends on:** credentials-tests-helpers.md

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

## Objective

Update REPL unit tests to use environment variables instead of `cfg->openai_api_key`. Apply the same pattern used in OpenAI tests: set `OPENAI_API_KEY` in setup, clear in teardown, and remove direct field assignments.

## Interface

No new interfaces - this task updates test infrastructure.

## Behaviors

### Test Pattern

Each REPL test file needs:

**Setup function:**
```c
static void setup(void) {
    test_ctx = talloc_new(NULL);
    setenv("OPENAI_API_KEY", "test-key", 1);
    // ... other setup ...
}
```

**Teardown function:**
```c
static void teardown(void) {
    unsetenv("OPENAI_API_KEY");
    talloc_free(test_ctx);
}
```

**Include requirement:**
```c
#include <stdlib.h>  // for setenv/unsetenv
```

### Field Assignment Removal

Remove all lines like:
```c
cfg->openai_api_key = talloc_strdup(cfg, "...");
ctx->cfg->openai_api_key = "...";
```

### Files to Update

| File | Type | Changes |
|------|------|---------|
| `tests/unit/repl/repl_llm_submission_test.c` | Test file | Add env var in setup/teardown, remove field assignments |
| `tests/unit/repl/repl_streaming_test_common.c` | Shared helper | Add env var in setup/teardown, remove field assignments |
| `tests/unit/repl/repl_tool_loop_integration_test.c` | Test file | Add env var in setup/teardown, remove field assignments |

### Shared Helper Considerations

`repl_streaming_test_common.c` is a shared test helper used by multiple tests. Ensure setup/teardown functions in this helper properly manage the environment variable.

## Test Scenarios

All existing REPL tests should pass after updating to use environment variables. Tests verify:
- REPL LLM submission works with credentials from environment
- REPL streaming functionality works
- REPL tool loop integration works
- Error handling for missing credentials

## Postconditions

- [ ] `grep -r "openai_api_key" tests/unit/repl/` returns nothing
- [ ] All REPL test files include `<stdlib.h>`
- [ ] All REPL tests have `setenv("OPENAI_API_KEY", ...)` in setup
- [ ] All REPL tests have `unsetenv("OPENAI_API_KEY")` in teardown
- [ ] All REPL tests compile without errors
- [ ] All REPL tests pass

- [ ] Changes committed to git with message: `task: credentials-tests-repl.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Verify no references remain
grep -r "openai_api_key" tests/unit/repl/
# Should return nothing

# Build and run tests
make build/tests/unit/repl/repl_llm_submission_test && ./build/tests/unit/repl/repl_llm_submission_test
# Should pass
```


## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).