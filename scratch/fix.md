# Task Run Failure Analysis & Remediation Plan

**Date:** 2025-12-24
**Branch:** rel-07
**Failed Task:** cleanup-openai-source.md (task 205)
**Status:** 131 completed, 1 failed, 10 pending

---

## Executive Summary

The orchestration run completed 131 out of 142 tasks successfully. One task failed at maximum escalation level (opus/ultrathink). The failure is NOT just a precondition violation - **the build actually fails to link `bin/ikigai`**.

**Root cause:** `src/providers/openai/openai.c` exists in `MODULE_SOURCES` but is NOT being included in the link line, causing undefined reference to `ik_openai_create`.

---

## Build Failure

### Symptom
```
/usr/bin/ld: build/providers/factory.o: in function `ik_provider_create':
src/providers/factory.c:98: undefined reference to `ik_openai_create'
```

### Analysis
1. `src/providers/openai/openai.c` **exists** and contains `ik_openai_create()` implementation
2. File **is listed** in `Makefile` `MODULE_SOURCES`
3. File **can be compiled** manually without errors
4. File **does NOT appear** in the automatic build output
5. `build/providers/openai/openai.o` **does NOT exist** after `make clean && make bin/ikigai`
6. `build/providers/openai/openai.o` is **NOT in the link line** (checked gcc command)

### Hypothesis
The Makefile pattern rules are not picking up `src/providers/openai/openai.c` for some reason. Possible causes:
- Missing pattern rule for `src/providers/openai/*.c`
- Incorrect dependency tracking
- File permissions issue (though manual compile works)

### Immediate Fix Required
Add explicit compile rule in Makefile or ensure pattern rules cover `src/providers/openai/openai.c`.

---

## Secondary Issue: jansson Dependency

### Problem
During task execution, `tests-anthropic-basic.md` incorrectly created test files using **jansson** library instead of **yyjson**.

**Affected file:** `tests/unit/providers/anthropic/anthropic_client_test.c`

**Incorrect Makefile change:** Added `-ljansson` to `CLIENT_LIBS`

**Codebase standard:**
- 52 files in `src/` use `yyjson.h`
- 0 files in `src/` use `jansson.h` (before the test was added)

### Fix Required
1. Rewrite `anthropic_client_test.c` to use yyjson API
2. Remove `-ljansson` from `CLIENT_LIBS` in Makefile

**API Mappings:**
```c
// OLD (jansson)
json_t *root = json_loads(json, 0, &error);
json_t *model = json_object_get(root, "model");
const char *str = json_string_value(model);
json_decref(root);

// NEW (yyjson)
yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
yyjson_val *root = yyjson_doc_get_root(doc);
yyjson_val *model = yyjson_obj_get(root, "model");
const char *str = yyjson_get_str(model);
yyjson_doc_free(doc);
```

**Task created:** `scratch/tasks/fix-jansson-to-yyjson.md` (NEW - should run FIRST)

---

## Cleanup Task Failure: cleanup-openai-source.md

### Precondition Check Failed

The task has a hard precondition that must pass:

```bash
# Must return empty
grep -r '#include.*"openai/' src/ | grep -v 'src/providers/openai/'
grep -r 'ik_openai_' src/ | grep -v 'src/providers/openai/'
```

**Current state:** Returns 10+ external references (not empty)

### Why It Failed

The provider abstraction successfully migrated **HTTP/streaming operations** but did NOT migrate **conversation storage**.

**What Was Migrated ✅**
- HTTP request/response → provider vtable
- Streaming (fdset/perform/info_read) → provider vtable
- Request serialization → `ik_request_t` (provider-agnostic)
- Response parsing → `ik_message_t` (provider-agnostic)

**What Was NOT Migrated ❌**
- Conversation storage still uses `ik_openai_conversation_t`
- Message creation still uses `ik_openai_msg_create()`
- Agent struct still holds `ik_openai_conversation_t *conversation`

### Files with External Dependencies (10 files)

**Headers:**
1. `src/agent.h` - defines `ik_openai_conversation_t *conversation` field
2. `src/wrapper_internal.h` - includes `openai/client.h`

**Source files:**
1. `src/agent.c` - calls `ik_openai_conversation_create()`, `ik_openai_msg_create()`
2. `src/wrapper.c` - includes `openai/client.h`
3. `src/commands_fork.c` - calls `ik_openai_msg_create()`, `ik_openai_conversation_add_msg()`
4. `src/marks.c` - includes `openai/client.h`
5. `src/repl.c` - includes `openai/client_multi.h`
6. `src/repl_actions_llm.c` - includes `openai/client.h`
7. `src/repl/agent_restore.c` - calls `ik_openai_conversation_add_msg()`
8. `src/repl/agent_restore_replay.c` - calls `ik_openai_conversation_add_msg()`

### Legacy Functions Still in Use

**Conversation management:**
- `ik_openai_conversation_create()` - creates conversation objects
- `ik_openai_conversation_add_msg()` - adds messages (8+ call sites)
- `ik_openai_conversation_clear()` - clears conversations
- `ik_openai_msg_create()` - creates messages
- `ik_openai_msg_create_tool_call()` - creates tool call messages
- `ik_openai_msg_create_tool_result()` - creates tool result messages

**Legacy type in core struct:**
```c
// src/agent.h:96
struct ik_agent {
    ...
    ik_openai_conversation_t *conversation;  // ← Still OpenAI-specific!
    ...
};
```

### Why This Matters

The bridge between old and new is `ik_request_build_from_conversation()` in `src/providers/request.c`:
- **Input:** Legacy `ik_openai_conversation_t` from agent
- **Output:** Provider-agnostic `ik_request_t`

This works, but prevents deletion of legacy files.

---

## Remediation Options

### Option 1: Fix Build + jansson, Skip Cleanup (RECOMMENDED)

**Scope:** Minimal, low risk

**Actions:**
1. Fix Makefile to include `build/providers/openai/openai.o` in link
2. Execute `fix-jansson-to-yyjson.md` task
3. Mark `cleanup-openai-source.md` as permanently blocked/deferred
4. Execute remaining 10 pending tasks
5. Document conversation migration as future epic

**Rationale:**
- Gets the build working
- Fixes the jansson inconsistency
- Allows remaining tasks to complete
- Legacy code works correctly alongside new provider abstraction
- Cleanup is cosmetic, not functional

**Time estimate:** 2-3 hours
**Risk:** Very low

### Option 2: Full Conversation Migration

**Scope:** Large architectural change

**Actions:**
1. Fix Makefile + jansson (same as Option 1)
2. Create provider-agnostic conversation types
3. Migrate agent structure from `ik_openai_conversation_t` to `ik_conversation_t`
4. Update 10+ files to use new conversation API
5. Then execute cleanup task
6. Execute remaining pending tasks

**Required work:**
- Rename `ik_openai_conversation_t` → `ik_conversation_t`
- Move to `src/conversation.{c,h}` or `src/providers/conversation.{c,h}`
- Update agent.h: `ik_conversation_t *conversation`
- Migrate all `ik_openai_conversation_*` → `ik_conversation_*`
- Update 10 files with new API calls

**Time estimate:** 6-10 hours
**Risk:** High (many files touched, potential for regressions)

### Option 3: Rename-in-Place

**Scope:** Medium refactoring

**Actions:**
1. Fix Makefile + jansson
2. Keep `src/openai/` files but make them provider-agnostic
3. Rename `src/openai/client.{c,h}` → `src/conversation.{c,h}`
4. Update includes throughout codebase
5. Types remain same, just semantically provider-agnostic

**Time estimate:** 4-6 hours
**Risk:** Medium

---

## Recommended Execution Plan

### Phase 1: Critical Fixes (IMMEDIATE)

**Goal:** Get build working, fix jansson inconsistency

**Tasks:**
1. ✅ **Fix Makefile link issue**
   - Add explicit rule for `build/providers/openai/openai.o`
   - OR fix pattern rules to pick up the file
   - Verify: `make clean && make bin/ikigai` succeeds

2. ✅ **Execute fix-jansson-to-yyjson.md**
   - Rewrite `tests/unit/providers/anthropic/anthropic_client_test.c` to use yyjson
   - Remove `-ljansson` from `CLIENT_LIBS`
   - Verify: Tests compile and pass

3. ✅ **Verify build health**
   - `make clean && make all` succeeds
   - `make check` runs (some tests may fail, that's OK for now)
   - `bin/ikigai` launches

**Duration:** 1-2 hours

### Phase 2: Complete Task Run (NEXT)

**Goal:** Execute remaining 10 pending tasks

**Tasks:**
1. Mark `cleanup-openai-source.md` as **permanently deferred** (blocked by conversation migration)
2. Import/register `fix-jansson-to-yyjson.md` as highest priority
3. Resume orchestration with remaining 11 tasks (1 new + 10 pending):
   - fix-jansson-to-yyjson.md (NEW - PRIORITY 1)
   - tests-openai-basic.md
   - tests-openai-streaming.md
   - verify-providers.md
   - cleanup-openai-adapter.md (may also fail due to same issue)
   - cleanup-openai-tests.md
   - cleanup-openai-docs.md
   - verify-cleanup.md
   - tests-integration-switching.md
   - tests-integration-flows.md
   - vcr-makefile.md

**Expected outcomes:**
- Most tasks should succeed
- Other cleanup tasks may also fail (same conversation dependency issue)
- That's acceptable - mark them as deferred too

**Duration:** 4-6 hours (automated orchestration)

### Phase 3: Future Work (DEFERRED)

**Epic:** Conversation Migration

**Scope:** Create provider-agnostic conversation storage layer

**Tasks to create:**
1. Design conversation abstraction
2. Implement `src/conversation.{c,h}`
3. Migrate agent.h to use new type
4. Update 10+ files with new API
5. Deprecate `src/openai/client.{c,h}`
6. Execute cleanup tasks

**Estimate:** 1-2 weeks of careful work

---

## Task Inventory

### Completed (131 tasks)
Moved to `scratch/tasks/done/` - includes all provider implementations, tests, and migrations

### Failed (1 task)
- `cleanup-openai-source.md` - blocked by conversation storage migration

### New (1 task)
- `fix-jansson-to-yyjson.md` - CRITICAL PRIORITY

### Pending (10 tasks)
1. tests-openai-basic.md
2. tests-openai-streaming.md
3. verify-providers.md
4. cleanup-openai-adapter.md (likely will fail)
5. cleanup-openai-tests.md (likely will fail)
6. cleanup-openai-docs.md (may succeed)
7. verify-cleanup.md (will fail if cleanup failed)
8. tests-integration-switching.md
9. tests-integration-flows.md
10. vcr-makefile.md

### Not Tracked (1 task)
- `google-thought-signatures.md` (exists in scratch/tasks but not in task DB)

---

## Critical Path Forward

### Immediate Actions (TODAY)

1. **Investigate Makefile issue** with `src/providers/openai/openai.c`
   - Check pattern rules in Makefile
   - Verify build variable construction
   - Add explicit rule if needed
   - Test: `make clean && make bin/ikigai`

2. **Execute fix-jansson-to-yyjson.md**
   - Automated task execution
   - Should take 15-20 minutes
   - Verify tests pass

3. **Verify system health**
   - `make clean && make all`
   - `make check` (accept pre-existing failures)
   - Manual smoke test of `bin/ikigai`

### Next Session (FRESH CONTEXT)

1. Import tasks to orchestration DB
2. Resume orchestration: `/orchestrate` with remaining tasks
3. Accept that some cleanup tasks may fail
4. Document what succeeded

---

## Known Issues to Document

### Conversation Storage Architecture Debt

**Issue:** Conversation storage uses legacy OpenAI-specific types

**Impact:**
- Cannot delete `src/openai/client.{c,h}` without breaking 10+ files
- Cleanup tasks will fail
- Technically works but semantically incorrect

**Solution:** Create provider-agnostic conversation layer (future epic)

**Workaround:** Keep legacy files indefinitely, rename to make purpose clear

### Build System Gap

**Issue:** `src/providers/openai/openai.c` not auto-compiling despite being in MODULE_SOURCES

**Impact:** Link failure, undefined reference to `ik_openai_create`

**Solution:** Fix Makefile pattern rules or add explicit rule

### Test Library Inconsistency

**Issue:** One test file uses jansson instead of yyjson

**Impact:** Non-standard dependency added to build

**Solution:** Migrate test to yyjson (task created)

---

## Success Criteria

### Minimum Viable (Phase 1)
- [ ] `make clean && make bin/ikigai` succeeds
- [ ] No jansson dependency in Makefile
- [ ] All tests use yyjson consistently
- [ ] `bin/ikigai` launches without errors

### Complete Task Run (Phase 2)
- [ ] fix-jansson-to-yyjson.md: ✅ succeeded
- [ ] 8-10 of remaining tasks succeed
- [ ] Cleanup tasks marked as deferred (expected failure)
- [ ] System builds and tests pass

### Future (Phase 3 - OPTIONAL)
- [ ] Conversation migration epic completed
- [ ] All cleanup tasks succeed
- [ ] No legacy `src/openai/` directory
- [ ] 100% provider abstraction

---

## Appendix: Technical Details

### Files Still Referencing Legacy Code

```bash
# Includes
src/wrapper_internal.h:#include "openai/client.h"
src/agent.c:#include "openai/client.h"
src/wrapper.c:#include "openai/client.h"
src/commands_fork.c:#include "openai/client.h"
src/repl.c:#include "openai/client_multi.h"
src/marks.c:#include "openai/client.h"
src/repl_actions_llm.c:#include "openai/client.h"

# Function calls
src/agent.c:88:    agent->conversation = ik_openai_conversation_create(agent);
src/agent.c:209:   agent->conversation = ik_openai_conversation_create(agent);
src/commands_fork.c: ik_openai_msg_create(...)
src/repl/agent_restore.c: ik_openai_conversation_add_msg(...)
# ... (8+ more call sites)
```

### Makefile Mystery

**Observation:** `src/providers/openai/openai.c` is in `MODULE_SOURCES` but doesn't compile

**Debug steps needed:**
1. Check if there's a `.d` file for it
2. Verify pattern rule matches
3. Check build target dependencies
4. Try explicit rule as workaround

### Test Migration Example

**File:** `tests/unit/providers/anthropic/anthropic_client_test.c`

**Lines to change:** ~50 lines using jansson API

**Pattern:**
- Replace `#include <jansson.h>` → `#include "yyjson.h"`
- Replace `json_loads()` → `yyjson_read()`
- Replace `json_object_get()` → `yyjson_obj_get()`
- Replace `json_string_value()` → `yyjson_get_str()`
- Replace `json_decref()` → `yyjson_doc_free()`

---

**End of Analysis**

Ready to proceed with Phase 1 critical fixes in fresh context.
