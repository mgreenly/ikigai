# Task: Update OpenAI Tests for Credentials API

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

Update all OpenAI unit tests to use environment variables instead of `cfg->openai_api_key`. Tests will set `OPENAI_API_KEY` environment variable in setup functions and clear it in teardown, enabling the credentials module to load keys from the environment during tests.

## Interface

No new interfaces - this task updates test infrastructure.

## Behaviors

### Universal Test Pattern

Each OpenAI test file needs:

**Setup function:**
```c
static void setup(void) {
    test_ctx = talloc_new(NULL);
    setenv("OPENAI_API_KEY", "test-api-key", 1);
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
cfg->openai_api_key = talloc_strdup(cfg, "test-api-key");
ctx->cfg->openai_api_key = "test-key";
```

The environment variable set in setup replaces these assignments.

### Files to Update

16 files total:

| File | Has setup/teardown? |
|------|---------------------|
| `tests/unit/openai/client_http_test.c` | Yes |
| `tests/unit/openai/client_http_mock_test.c` | Yes |
| `tests/unit/openai/client_http_sse_finish_test.c` | Yes |
| `tests/unit/openai/client_http_sse_streaming_test.c` | Yes |
| `tests/unit/openai/client_multi_add_request_test.c` | Yes |
| `tests/unit/openai/client_multi_add_request_debug_output_test.c` | Yes |
| `tests/unit/openai/client_multi_callback_error_test.c` | Yes |
| `tests/unit/openai/client_multi_callback_error_with_http_errors_test.c` | Yes |
| `tests/unit/openai/client_multi_callbacks_coverage_test.c` | Yes |
| `tests/unit/openai/client_multi_callbacks_metadata_test.c` | Yes |
| `tests/unit/openai/client_multi_callbacks_metadata_choices_test.c` | Yes |
| `tests/unit/openai/client_multi_http_success_test.c` | Yes |
| `tests/unit/openai/client_multi_write_callback_test.c` | Yes |
| `tests/unit/openai/client_multi_info_read_helpers.h` | Helper header |
| `tests/unit/openai/http_handler_error_paths_test.c` | Yes |
| `tests/unit/openai/http_handler_tool_calls_test.c` | Yes |

### Special Case: Helper Header

`client_multi_info_read_helpers.h` contains shared test helper functions. Remove `cfg->openai_api_key` assignments from helper functions like `create_test_multi_ctx()` since the environment variable is set by each test's setup function.

## Test Scenarios

All existing OpenAI tests should pass after updating to use environment variables. Tests verify:
- OpenAI client functions work with credentials loaded from environment
- Error handling for missing credentials
- HTTP interactions with API keys
- Streaming and multi-request functionality

## Postconditions

- [ ] `grep -r "cfg->openai_api_key" tests/unit/openai/` returns nothing
- [ ] `grep -r "openai_api_key" tests/unit/openai/` returns nothing
- [ ] All openai test files include `<stdlib.h>`
- [ ] All openai tests have `setenv("OPENAI_API_KEY", ...)` in setup
- [ ] All openai tests have `unsetenv("OPENAI_API_KEY")` in teardown
- [ ] All openai tests compile without errors
- [ ] All openai tests pass

- [ ] Changes committed to git with message: `task: credentials-tests-openai.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Verify no references remain
grep -r "openai_api_key" tests/unit/openai/
# Should return nothing

# Build and run a sample test
make build/tests/unit/openai/client_http_test && ./build/tests/unit/openai/client_http_test
# Should pass
```


## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).