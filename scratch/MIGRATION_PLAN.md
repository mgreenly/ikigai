# OpenAI Legacy Code Migration Plan

**Status:** Ready to execute (tool call bug fixed, Task 0 created)
**Last Updated:** 2025-12-25
**Current Branch:** rel-07

## Executive Summary

Migrate from legacy OpenAI-specific conversation storage to provider-agnostic message format. Remove 21 files (~6500 LOC) while maintaining all functionality across three providers (Anthropic, OpenAI, Google).

## Current State (as of commit bc1efb1)

### What Exists
- ✅ `src/openai/` directory (18 files, ~3000 LOC) - old implementation
- ✅ `src/providers/openai/shim.c` (1000 LOC) - bridge layer, currently unused
- ✅ `agent->conversation` field - old storage mechanism
- ✅ Tool call bug FIXED (commit bc1efb1) - critical blocker resolved
- ✅ All tests pass (146 unit + integration tests)

### What Doesn't Exist Yet
- ❌ `src/message.c/h` - new message creation API
- ❌ `agent->messages` field - new storage mechanism
- ❌ Dual-mode population code

## The Problem We're Solving

### Current Architecture
```
User Input → agent->conversation (ik_openai_conversation_t)
             ↓
             ik_msg_t[] (OpenAI-specific)
             ↓
             ik_request_build_from_conversation()
             ↓
             src/providers/request.c (converts on-the-fly, DATA LOSS)
             ↓
             ik_request_t with ik_message_t[] (provider-agnostic)
             ↓
             Provider-specific serialization
```

**Problems:**
1. **Data loss:** Line 466 (NOW FIXED) was discarding tool call structure
2. **Type confusion:** ik_msg_t vs ik_message_t conversion is ad-hoc
3. **Coupling:** Non-provider code depends on `src/openai/client.h`
4. **Dead code:** 1000 LOC shim layer is unused
5. **Legacy burden:** 18 files to maintain for one provider

### Target Architecture
```
User Input → agent->messages (ik_message_t*[])
             ↓
             Provider-agnostic from the start
             ↓
             ik_request_build_from_conversation()
             ↓
             ik_request_t with ik_message_t[] (no conversion needed)
             ↓
             Provider-specific serialization
```

**Benefits:**
1. **No data loss:** Structured data preserved throughout
2. **Type safety:** Single message type (ik_message_t)
3. **Decoupling:** No OpenAI dependencies outside providers/openai/
4. **Simplicity:** Remove 21 files, ~6500 LOC
5. **Uniformity:** All 3 providers use identical interface

## Migration Strategy: Bottom-Up with Safety

### Key Insight
The "hinge point" is `src/providers/request.c:438-555` where old storage meets new format. This is where the migration succeeds or fails.

### Safety Measures
1. **Tool call bug fixed FIRST** (commit bc1efb1) - ensures hinge point works
2. **Dual-mode operation** (Tasks 0-4) - both storages populated in parallel
3. **Incremental switch** (Task 5) - one function at a time
4. **Verification gates** - tests must pass after each task
5. **Rollback plan** - each task is atomic and reversible

## Task Sequence (10 Tasks)

### Phase 1: Foundation (Tasks 0-1)
**Goal:** Create infrastructure without changing behavior

**Task 0: Assessment and Setup** [NEW]
- Document current state
- Create `src/message.c/h` with message creation API
- Add `agent->messages` field to struct (unpopulated)
- Add management functions (add, clear, clone)
- **Verify:** Build succeeds, all tests pass
- **Status:** Infrastructure exists but unused

**Task 1: Message Storage Dual Mode**
- Same as old Task 1, but depends on Task 0
- Agent struct already has fields (added in Task 0)
- Focus on database conversion helpers
- **Status:** Storage ready to populate

### Phase 2: Populate Dual Storage (Tasks 2-4)
**Goal:** Both `conversation` and `messages` stay synchronized

**Task 2: Migrate REPL Messages**
- Update REPL to call both old and new APIs
- Pattern: old API succeeds → continue; new API fails → log error
- **Verify:** Both storages populated after user input, assistant response

**Task 3: Migrate Agent Operations**
- Update fork, clear to maintain both storages
- **Verify:** Fork clones both, clear empties both

**Task 4: Migrate Restore**
- Load from DB into both storages
- **Verify:** Session restore populates both

**Critical Checkpoint After Task 4:**
```bash
# Both storages must stay synchronized
grep -n "agent->message_count == agent->conversation->message_count" tests/
# Must find synchronization verification tests
```

### Phase 3: Switch to New Storage (Task 5)
**Goal:** Make new storage authoritative

**Task 5: Migrate Provider Request Building**
- **THE CRITICAL TASK** - changes line 438 from old to new storage
- Add safety assertion: `assert(agent->messages != NULL || agent->message_count == 0)`
- **Verify:** All provider tests must pass BEFORE proceeding to Task 6
- **Rollback:** Revert just this function if it fails

**Success Gate:**
```bash
# These MUST pass before Task 6
./build/tests/integration/providers/anthropic/basic_test
./build/tests/integration/providers/openai/basic_test
./build/tests/integration/providers/google/basic_test
```

If ANY fail, STOP and debug. Do NOT proceed to Task 6.

### Phase 4: Remove Old Storage (Task 6)
**Goal:** Delete dual-mode code paths

**Task 6: Remove Legacy Conversation Field**
- Delete `agent->conversation` from struct
- Remove all `ik_openai_conversation_*` calls outside src/openai/
- Remove all `ik_openai_msg_create*` calls
- Update 11 files
- **Verify:** Build succeeds, tests pass, no legacy references

**Split this task?** Consider breaking into smaller atomic changes if it fails.

### Phase 5: Delete Legacy Files (Tasks 7-8)
**Goal:** Remove unused code

**Task 7: Delete Legacy Files Part 1**
- Delete `src/openai/` directory (18 files)
- Delete `src/providers/openai/shim.*` (2 files)
- Update Makefile
- **Verify:** Build succeeds (may have warnings about missing shim)

**Task 8: Delete Legacy Files Part 2**
- Refactor OpenAI request serialization (direct JSON, no shim)
- Fix any remaining issues from Part 1
- **Verify:** Build clean, no warnings

### Phase 6: Final Verification (Task 9)
**Goal:** Comprehensive quality gate

**Task 9: Verify Migration Complete**
- Run all automated checks
- Verify no legacy references remain
- Create MIGRATION_COMPLETE.md
- **Verify:** 10/10 success criteria met

## Critical Dependencies

```
Task 0 (foundation)
  ↓
Task 1 (storage dual mode) ← depends on Task 0 infrastructure
  ↓
Task 2 (REPL) ← depends on Task 1 message creation
  ↓
Task 3 (agent ops) ← depends on Task 2 patterns
  ↓
Task 4 (restore) ← depends on Task 3 fork/clone
  ↓
**CHECKPOINT: Verify both storages synchronized**
  ↓
Task 5 (providers) ← depends on Task 4 completed
  ↓
**GATE: All provider tests MUST pass**
  ↓
Task 6 (remove old) ← depends on Task 5 switch complete
  ↓
Task 7 (delete files 1) ← depends on Task 6 no references
  ↓
Task 8 (delete files 2) ← depends on Task 7 deletions
  ↓
Task 9 (verify) ← depends on Task 8 refactoring
```

## What Could Go Wrong

### Most Likely Failure Points

**1. Task 5 (Provider Switch) - 60% risk**
- **What:** Request builder fails to work with new storage
- **Why:** Subtle differences between old and new message format
- **Mitigation:** Tool call bug already fixed, add safety assertions
- **Rollback:** Revert just request.c, keep dual mode

**2. Task 6 (Remove Old Storage) - 30% risk**
- **What:** Missed a reference to old API in large codebase
- **Why:** Updating 11 files atomically is complex
- **Mitigation:** Grep verification before and after
- **Rollback:** Revert entire task, re-add old field

**3. Task 4 (Restore) - 10% risk**
- **What:** Database format conversion edge cases
- **Why:** data_json parsing for tool calls/results
- **Mitigation:** Use patterns from fixed request.c
- **Rollback:** Revert just restore code

### Recovery Procedures

**If Task N fails:**
1. Run `git status` to see what changed
2. Check `make check` output for specific failures
3. Read error messages carefully
4. If quick fix not obvious:
   ```bash
   git checkout HEAD -- .  # Revert all uncommitted changes
   git log -1              # Verify we're back to before task
   make check              # Confirm tests pass again
   ```
5. Document what failed in scratch/task-N-failure.md
6. Fix the task file, retry

## Success Metrics

### After Each Task
- ✅ Build succeeds (`make clean && make all`)
- ✅ Tests pass (`make check`)
- ✅ No compiler warnings
- ✅ Git commit created with clear message

### After Task 5 (Critical Gate)
- ✅ All 3 provider integration tests pass
- ✅ Tool calls work end-to-end
- ✅ Request serialization correct for all message types

### After Task 9 (Final)
- ✅ `src/openai/` deleted (18 files removed)
- ✅ `src/providers/openai/shim.*` deleted (2 files removed)
- ✅ No legacy references: `grep -r "ik_openai_conversation" src/` returns empty
- ✅ All 3 providers work with unified interface
- ✅ Total reduction: 21 files, ~6500 LOC

## Estimated Effort

**Total:** ~10 tasks, ~2-4 hours per task = 20-40 hours

| Task | Complexity | Risk | Time Estimate |
|------|-----------|------|---------------|
| 0 - Foundation | Medium | Low | 2-3 hours |
| 1 - Storage | Low | Low | 1-2 hours |
| 2 - REPL | Medium | Low | 2-3 hours |
| 3 - Agent Ops | Low | Low | 1-2 hours |
| 4 - Restore | Medium | Medium | 2-4 hours |
| 5 - Providers | **HIGH** | **HIGH** | 3-5 hours |
| 6 - Remove Old | Medium | Medium | 2-3 hours |
| 7 - Delete 1 | Low | Low | 1 hour |
| 8 - Delete 2 | Medium | Medium | 2-4 hours |
| 9 - Verify | Low | Low | 2 hours |

**Critical path:** Task 0 → Task 5 (18-24 hours for success path)

## Decision Log

### 2025-12-25: Tool Call Bug Fixed First
**Decision:** Fix data loss bug in request.c before starting migration
**Rationale:** This bug would cause Task 5 to fail silently. Fixing it first:
- Validates we understand the data flow
- Improves test coverage
- Proves the hinge point concept works
**Commit:** bc1efb1

### 2025-12-25: Created Task 0
**Decision:** Add foundation task before original Task 1
**Rationale:** Previous attempts failed because:
- No assessment of starting state
- Tried to populate storage that didn't exist
- No verification that foundation was solid
**Commit:** 293cd14

## Next Steps

1. ✅ Review this migration plan
2. ✅ Fix tool call bug (DONE - commit bc1efb1)
3. ✅ Create Task 0 (DONE - commit 293cd14)
4. ⏭️ Execute Task 0 (create foundation)
5. ⏭️ Execute Tasks 1-9 in sequence
6. ⏭️ Create MIGRATION_COMPLETE.md when done

## References

- Original problem analysis: `scratch/problem.md`
- Task files: `scratch/tasks/*.md`
- Task order: `scratch/tasks/order.json`
- Tool call bug fix: commit bc1efb1
- Task 0 creation: commit 293cd14
