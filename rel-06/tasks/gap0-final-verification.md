# Task: Final Verification

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is the final step of Gap 0 - verifying the type unification is complete. We confirm:
1. No remaining `ik_message_t` references in the codebase
2. No remaining `ik_msg_from_db` references in the codebase
3. All tests pass
4. Code is lint-clean
5. The unified `ik_msg_t` type is used everywhere

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section - testing requirements)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- All prior Gap 0 tasks completed:
  - gap0-add-id-field.md
  - gap0-init-id-creators.md
  - gap0-migrate-replay.md
  - gap0-migrate-agent-replay.md
  - gap0-migrate-message.md
  - gap0-update-session-restore.md
  - gap0-remove-obsolete.md
  - gap0-update-replay-tests.md
  - gap0-update-session-restore-tests.md
  - gap0-update-message-tests.md
  - gap0-update-integration-tests.md
  - gap0-delete-obsolete-tests.md
  - gap0-update-docs.md

## Task

### Part 1: Verify no ik_message_t references remain

```bash
# Should return NO results (except possibly in gap.md itself or git history)
grep -r "ik_message_t" src/ tests/ --include="*.c" --include="*.h"
```

If any results found, they are bugs from prior tasks and must be fixed.

### Part 2: Verify no ik_msg_from_db references remain

```bash
# Should return NO results (except possibly in gap.md or docs)
grep -r "ik_msg_from_db" src/ tests/ --include="*.c" --include="*.h"
```

If any results found, they are bugs from prior tasks and must be fixed.

### Part 3: Verify ik_msg_t has id field

Check that `src/msg.h` defines `ik_msg_t` with `int64_t id` as first field.

### Part 4: Run full test suite

```bash
make check
```

All tests must pass. If any fail, investigate and fix.

### Part 5: Run linter

```bash
make lint
```

Must pass with no errors.

### Part 6: Verify testing requirements from Gap 0 spec

From gap.md Testing section, verify:
1. `ik_msg_t` has `id` field - CHECK via grep
2. DB queries populate `id` correctly - covered by agent_replay tests
3. In-memory created messages have `id = 0` - covered by client_msg code review
4. `ik_replay_context_t` uses `ik_msg_t**` - CHECK via grep
5. OpenAI serialization ignores `id` field - CHECK existing serialization tests
6. All existing tests pass - CHECK via `make check`
7. No `ik_message_t` references remain - CHECK via grep
8. `ik_msg_from_db()` is removed - CHECK via grep

## TDD Cycle

### Red
Not applicable - this is verification.

### Green
1. Run grep commands to verify no obsolete references
2. Run `make check` - all tests must pass
3. Run `make lint` - must pass
4. Document verification results

### Refactor
If issues found, create fix commits before marking complete.

## Sub-agent Usage
- Use haiku sub-agents to run verification commands

## Post-conditions
- `make check` passes (all tests green)
- `make lint` passes
- `grep -r "ik_message_t" src/ tests/` returns no results
- `grep -r "ik_msg_from_db" src/ tests/` returns no results
- `ik_msg_t` struct has `id` field
- `ik_replay_context_t.messages` is `ik_msg_t**`
- Working tree is clean (all changes committed)
- Gap 0 is complete and ready for Gap 1

## Success Criteria

Gap 0 is complete when:
- Single message type (`ik_msg_t`) used throughout codebase
- No conversion overhead between DB and in-memory formats
- All tests pass
- Code is lint-clean
- Ready for Gap 1 (Startup Agent Restoration)
