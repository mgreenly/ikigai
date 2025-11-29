# Task Plan Review Issues

Issues identified during deep review of rel-04 task plan.

---

## Critical Issues

### 1. Message Type Bridge Missing ✅ RESOLVED

**Problem**: Database events (`ik_message_t`) and API messages (`ik_openai_msg_t`) are separate types with no transformation between them. Session restore populates scrollback but doesn't rebuild `repl->conversation`.

**Resolution**: Created `conversation-rebuild.md` (task 19) which:
- Adds `ik_openai_msg_from_db()` transformation function
- Updates `session_restore.c` to rebuild conversation from replay context
- Updated `order.md` with renumbered tasks (now 39 total)

---

### 2. TDD Red Phase Pattern Violation ✅ RESOLVED

**Problem**: Nearly all tasks say:
```
Run `make check` - expect compile failure
```

But `.agents/skills/tdd.md` explicitly states:
> "A compilation error is NOT a failing test - you need a stub that compiles and runs"

**Impact**: Sub-agents may skip creating stubs, missing the discipline of seeing actual assertion failures before implementing.

**Correct pattern**:
1. Write tests
2. Add header declaration + stub implementation
3. Run `make check` - expect **assertion failure** (not compile failure)

**Resolution**: Updated all 20 affected task files to include stub creation steps in Red phase:
- tool-glob-schema.md, tool-call-struct.md, tool-output-limit.md
- tool-build-array.md, tool-all-schemas.md, parse-tool-calls.md
- tool-argument-parser.md, glob-execute.md, tool-result-msg.md
- scrollback-tool-display.md, tool-dispatcher.md, file-read-execute.md
- grep-execute.md, file-write-execute.md, bash-execute.md
- tool-result-limit-metadata.md, tool-loop-limit-config.md
- conversation-rebuild.md, tool-choice-config.md, assistant-tool-calls-msg.md

Each Red phase now includes: write tests → add declaration → add stub → expect assertion failure

---

### 3. Config Fields Missing ✅ RESOLVED

**Problem**: Tasks reference config fields that don't exist in `ik_cfg_t`:
- `max_tool_turns` (task 27: tool-loop-limit-config.md)
- `max_output_size` (task 6: tool-output-limit.md)

No task explicitly adds these to `ik_cfg_t` or updates `ik_cfg_load()`.

**Resolution**: Created `tool-config-fields.md` (task 6 in Story 02) which:
- Adds `max_tool_turns` (int32_t, default: 50, range: 1-1000) to `ik_cfg_t`
- Adds `max_output_size` (int64_t, default: 1048576, range: 1KB-100MB) to `ik_cfg_t`
- Updates `create_default_config()` to write both fields with defaults
- Updates `ik_cfg_load()` to validate both fields, error on missing/invalid
- Deleted redundant `tool-loop-limit-config.md` (was task 28)
- Updated `tool-loop-counter.md` to reference config field via `repl->cfg->max_tool_turns`
- Updated `tool-output-limit.md` pre-conditions to reference new config task
- Renumbered order.md (now 39 tasks total)

---

### 4. Pre-read References to Non-Existent Files ✅ RESOLVED

**Problem**: Several tasks reference test files incorrectly:
- `db-tool-replay.md` → `tests/unit/db/replay_test.c` (should be `replay_core_test.c`)
- `tool-loop-state-mutation.md` → `tests/unit/openai/conversation_test.c` (doesn't exist)

**Impact**: Sub-agents with blank context can't find referenced patterns.

**Resolution**: Fixed file references:
- `db-tool-replay.md`: Changed to `tests/unit/db/replay_core_test.c`
- `tool-loop-state-mutation.md`: Replaced with actual existing test files:
  - `tests/unit/repl/handle_request_success_test.c`
  - `tests/unit/repl/repl_http_completion_callback_test.c`

---

## Warnings (Lower Priority)

### 5. Story 10 Has No Verification Task ✅ ACCEPTED

`order.md` says "No new tasks needed" for Story 10 (multi-turn loop), relying on Story 04. But there's no E2E test for 3+ tool chains.

**Resolution**: Accepted as-is. The 2-tool test in Story 04 (`multi-tool-loop.md`) verifies the loop continues while `finish_reason == "tool_calls"`. This logic generalizes to N tools without needing explicit 3-tool verification.

### 6. Task Name Mismatch in Pre-conditions ✅ RESOLVED

`tool-choice-config.md` references `replay-tool-messages.md` but actual file is `replay-tool-e2e.md`.

**Resolution**: Fixed reference in `tool-choice-config.md` pre-conditions.

### 7. Missing Explicit Dependency ✅ RESOLVED

`glob-execute.md` should list `tool-output-limit.md` as pre-condition (must call truncation utility).

**Resolution**: Added `tool-output-limit.md` to `glob-execute.md` pre-conditions.

### 8. E2E Tests Reference Speculative File Names ✅ RESOLVED

Tasks like `file-read-error-e2e.md` reference `tests/unit/tool/test_file_read_execute.c` but implementer might use different name.

**Resolution**: Changed speculative filenames to reference directories and source tasks:
- `file-read-error-e2e.md`: Now references `tests/unit/tool/` and task `file-read-execute.md`
- `bash-command-error-e2e.md`: Now references `tests/unit/tool/` and task `bash-execute.md`
- `tool-dispatcher.md`: Now references `tests/unit/tool/` and task `glob-execute.md`

### 9. Inconsistent TDD Phase Naming ✅ RESOLVED

Some tasks use "Verify", others use "Refactor" as third phase. Should standardize.

**Resolution**: Standardized to "Refactor" across all tasks. Fixed 2 files:
- `db-tool-replay.md`
- `replay-tool-e2e.md`

### 10. E2E Tasks "Expect Pass" in Red Phase ✅ RESOLVED

Several E2E tasks say "expect pass" in Red phase - these are verification tests, not TDD.

**Resolution**: Added clarifying note to all 8 E2E task files explaining they are verification tests:
> "This is a verification test. The functionality is implemented in earlier tasks. If previous tasks are complete, the test should pass. If it fails, identify and fix gaps in the implementation."

### 11. Inconsistent Test Directory Structure ✅ RESOLVED

- Tool-choice tests use `tests/unit/test_tool_choice_config.c` (missing module subdirectory)
- E2E tasks mix `tests/e2e/` and `tests/integration/`

**Resolution**: Standardized test paths:
- `tool-choice-config.md`, `tool-choice-serialize.md`: Changed to `tests/unit/openai/test_tool_choice.c`
- `tool-choice-*-e2e.md` (4 files): Changed `tests/e2e/` to `tests/integration/` (existing directory)

### 12. finish_reason Already Exists ✅ RESOLVED

Task `tool-loop-finish-detection.md` says "Add field to appropriate context struct" but `repl->response_finish_reason` already exists.

**Resolution**: Updated `tool-loop-finish-detection.md` to reference existing infrastructure:
- `repl->response_finish_reason` (src/repl.h:75)
- `ik_openai_http_extract_finish_reason()` (src/openai/http_handler_internal.h)
- Task now focuses on verifying existing code works for "tool_calls" and adding accessor if needed

---

## Progress Tracking

| Issue | Status | Notes |
|-------|--------|-------|
| 1. Message Bridge | ✅ RESOLVED | conversation-rebuild.md created |
| 2. TDD Red Phase | ✅ RESOLVED | 20 task files updated |
| 3. Config Fields | ✅ RESOLVED | tool-config-fields.md created, redundant task deleted |
| 4. File References | ✅ RESOLVED | Fixed db-tool-replay.md, tool-loop-state-mutation.md |
| 5. Story 10 E2E | ✅ ACCEPTED | 2-tool test generalizes to N tools |
| 6. Task Name | ✅ RESOLVED | Fixed tool-choice-config.md reference |
| 7. Missing Dep | ✅ RESOLVED | Added to glob-execute.md pre-conditions |
| 8. Speculative Files | ✅ RESOLVED | Fixed 3 files to reference dirs + tasks |
| 9. Phase Naming | ✅ RESOLVED | Standardized to "Refactor" (2 files) |
| 10. E2E Red Pass | ✅ RESOLVED | Added verification notes to 8 E2E files |
| 11. Test Dirs | ✅ RESOLVED | Standardized to existing directories |
| 12. finish_reason | ✅ RESOLVED | Updated task to reference existing field |
