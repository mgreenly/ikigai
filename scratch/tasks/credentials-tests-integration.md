# Task: Update Integration Tests and Docs for Credentials API

**Model:** sonnet/none
**Depends on:** credentials-migrate.md, credentials-tests-helpers.md, credentials-tests-config.md, credentials-tests-openai.md, credentials-tests-repl.md

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

Update all integration tests and project documentation to remove `openai_api_key` references. This is the final credentials-tests task, completing the migration from config-based API keys to the credentials module.

## Interface

No new interfaces - this task removes obsolete code and updates documentation.

## Behaviors

### Integration Test Updates

Apply the same environment variable pattern used in unit tests:

**Setup pattern:**
```c
static void setup(void) {
    // ... existing setup ...
    setenv("OPENAI_API_KEY", "test-key", 1);
}
```

**Teardown pattern:**
```c
static void teardown(void) {
    unsetenv("OPENAI_API_KEY");
    // ... existing teardown ...
}
```

**Include requirement:**
```c
#include <stdlib.h>  // for setenv/unsetenv
```

### Configuration Integration Tests

Remove `openai_api_key` from JSON config fixtures in integration tests.

### Documentation Updates

Update project documentation to reflect the credentials module:
- Remove references to `openai_api_key` in config struct
- Add documentation about credentials loading from environment or file
- Update memory management examples if they reference `openai_api_key`

### Files to Update

| File | Type | Changes |
|------|------|---------|
| `tests/integration/config_integration_test.c` | Test | Remove from JSON fixtures |
| `tests/integration/openai_mock_verification_test.c` | Test | Add env var setup/teardown |
| `tests/integration/tool_choice_auto_test.c` | Test | Add env var setup/teardown |
| `tests/integration/tool_choice_none_test.c` | Test | Add env var setup/teardown |
| `tests/integration/tool_choice_required_test.c` | Test | Add env var setup/teardown |
| `tests/integration/tool_choice_specific_test.c` | Test | Add env var setup/teardown |
| `tests/integration/tool_loop_limit_test.c` | Test | Add env var setup/teardown |
| `project/architecture.md` | Documentation | Update credential documentation |
| `project/memory.md` | Documentation | Remove outdated examples |

## Test Scenarios

All existing integration tests should pass after updating to use environment variables. Tests verify:
- Config integration works without `openai_api_key`
- OpenAI mock verification works with credentials from environment
- Tool choice integration tests work with credentials
- Tool loop limits work with credentials

## Postconditions

- [ ] `grep -r "cfg->openai_api_key" tests/integration/` returns nothing
- [ ] `grep -r "openai_api_key" tests/integration/` returns nothing (except comments)
- [ ] `grep -r "cfg->openai_api_key" src/` returns nothing
- [ ] All integration tests include `<stdlib.h>`
- [ ] All integration tests set `OPENAI_API_KEY` in setup
- [ ] All integration tests clear `OPENAI_API_KEY` in teardown
- [ ] All integration tests compile without errors
- [ ] All integration tests pass
- [ ] `make check` passes (all tests green)
- [ ] No compiler warnings about `openai_api_key`
- [ ] Documentation accurately reflects credentials module

- [ ] Changes committed to git with message: `task: credentials-tests-integration.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Full verification that openai_api_key is gone from tests and src
grep -r "openai_api_key" src/ tests/
# Should return nothing (or only comments)

# Build and run all tests
make check
# Should pass

# Verify no warnings
make 2>&1 | grep -i "openai_api_key"
# Should return nothing
```

## Note

This is the final task in the credentials migration. After completion:
- All source code uses the credentials module
- All tests use environment variables for credentials
- Documentation reflects the new credential management approach
- The codebase is ready for multi-provider support (Anthropic, Google)


## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).