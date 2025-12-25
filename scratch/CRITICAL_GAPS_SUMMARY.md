# CRITICAL GAPS - Must Fix Before /orchestrate

**Date:** 2025-12-24
**Status:** ⚠️ TASKS NOT READY FOR UNATTENDED EXECUTION
**Risk Level:** HIGH

---

## Top 5 Critical Issues

### 1. ⛔ Task 4: System Message Role Mapping is WRONG

**File:** `scratch/tasks/migrate-restore.md`
**Line:** 60
**Issue:** Maps system messages to `IK_ROLE_USER` - this is incorrect

**Current Code (BROKEN):**
```c
// Note: System messages are typically handled via system_prompt field in requests,
// not as conversation messages. However, for dual mode consistency, we add it.
ik_message_t *new_system = ik_message_create_text(agent, IK_ROLE_USER, system_prompt);
```

**Why This Breaks:**
- System prompts are NOT user messages
- Provider APIs expect system prompts as separate field, not in message array
- This will cause request building to fail or send wrong role to LLM

**Fix:**
Either:
- A) Don't add system prompt to message array (handle via request->system_prompt field)
- B) Create IK_ROLE_SYSTEM role type

**Impact if Not Fixed:**
- Provider request building fails
- LLM receives malformed requests
- All three providers break

**Estimated Fix Time:** 30 minutes

---

### 2. ⛔ Task 5: Shows Full Implementation Code (Violates Task Authoring Rules)

**File:** `scratch/tasks/migrate-providers.md`
**Lines:** 125-203 (80 lines of implementation code)
**Issue:** Shows complete implementation of `ik_request_add_message_direct()`

**Violation:**
Task authoring skill explicitly says:
> "Task files specify what to build, not how to build it"
> "Interface level, not implementation code"

**Why This is Bad:**
1. Over-constrains implementation
2. Contains bugs (geometric growth comment but uses `+1`)
3. Has arbitrary 1024 limit
4. Error context misuse (allocates error on req, then frees req - use-after-free)
5. Prevents sub-agent from finding better approach

**Fix:**
Replace 80 lines of code with interface specification:
```markdown
### ik_request_add_message_direct()

**Purpose:** Deep copy message and content blocks into request

**Interface:**
```c
static res_t ik_request_add_message_direct(ik_request_t *req, const ik_message_t *msg);
```

**Behavior:**
- Grow request->messages array (geometric expansion recommended)
- Deep copy message struct
- Deep copy all content blocks and their string fields
- Handle all content types: TEXT, TOOL_CALL, TOOL_RESULT, THINKING
- Return OK with copied message, ERR on capacity issues

**Error Handling:**
- Allocate errors on parent context (not req context)
- PANIC on OOM
- ERR for invalid inputs

**Memory:**
- All allocations under request context
- Strings copied with talloc_strdup()
```

**Impact if Not Fixed:**
- Sub-agent follows buggy implementation exactly
- Creates use-after-free bugs
- Inefficient array growth
- Artificial limitations

**Estimated Fix Time:** 2 hours (rewrite interface specs)

---

### 3. ⛔ Task 7: Shim Replacement Completely Underspecified

**File:** `scratch/tasks/delete-legacy-files.md`
**Lines:** 118-145
**Issue:** Says "remove shim usage and work directly with request->messages" without showing how

**Current Guidance (INSUFFICIENT):**
```markdown
**Current (uses shim):**
ik_openai_conversation_t *conv = ik_openai_shim_build_conversation(ctx, request);
// ... use conv ...

**New (works directly with request->messages):**
// Request already has messages in provider format
for (size_t i = 0; i < request->message_count; i++) {
    ik_message_t *msg = &request->messages[i];
    // Serialize directly to OpenAI format
}
```

**Why This is Insufficient:**
- Shim is 200+ lines of complex conversion code
- Two files need refactoring: request_chat.c and request_responses.c
- OpenAI JSON format is complex (different for chat vs responses API)
- Tool calls have special accumulation logic
- Sub-agent will need to research OpenAI API

**What's Missing:**
1. Complete function signatures that change
2. OpenAI JSON format examples
3. Tool call accumulation strategy
4. Error handling during serialization
5. Message role to OpenAI role mapping
6. Content block to OpenAI content format conversion

**Fix Needed:**
Show complete before/after patterns for:
- `ik_openai_build_chat_request()`
- `ik_openai_build_responses_request()`
- Message serialization helpers
- Tool call serialization

**Impact if Not Fixed:**
- Sub-agent will fail or create broken implementation
- Will spawn research agents (high token usage)
- Will likely escalate to opus/ultrathink
- May need multiple attempts

**Estimated Fix Time:** 3-4 hours (need to show conversion patterns)

---

### 4. ⚠️ Missing Critical Skills

**Issue:** Several tasks are missing required skill loads

| Task | Missing Skills | Impact |
|------|----------------|--------|
| Task 1 | `database`, `log` | Won't understand ik_msg_t format, data_json structure |
| Task 2 | `log` | Won't know how to use ik_log_error() |
| Task 5 | `mocking` | Won't understand wrapper pattern |
| Task 7 | `source-code` | Won't know file dependencies |

**Fix:**
Add skill loads to Pre-Read sections

**Estimated Fix Time:** 10 minutes

---

### 5. ⚠️ Line Numbers are Fragile

**Issue:** All tasks use "around line X" as location hints

**Examples:**
- Task 2: "around line 420", "around line 260"
- Task 3: "around line 252-300"
- Task 4: "around line 60"

**Why This is Bad:**
- Files may have changed since task authoring
- Sub-agent wastes tokens searching wrong location
- May modify wrong code if finds similar pattern

**Fix:**
Use function names as primary locators:
- "in `handle_user_input()` function"
- "in `http_completion_callback()` where assistant response is created"
- "in `restore_system_prompt()` helper"

**Estimated Fix Time:** 1 hour (update all tasks)

---

## Summary of Must-Fix Issues

| Issue | Severity | Fix Time | Impact if Not Fixed |
|-------|----------|----------|---------------------|
| Task 4 system message role | CRITICAL | 30 min | Providers break |
| Task 5 implementation code | CRITICAL | 2 hours | Buggy code, violations |
| Task 7 shim replacement | CRITICAL | 3-4 hours | Task failure |
| Missing skills | HIGH | 10 min | Research sub-agents |
| Line number fragility | MEDIUM | 1 hour | Token waste |

**Total Fix Time:** 7-8 hours

---

## Recommended Action

### Option A: Fix Then Execute (RECOMMENDED)

1. Fix critical issues (7-8 hours)
2. Run /orchestrate
3. Expected success rate: 85-95%
4. Expected token usage: 80-120K

**Total Time:** 8-10 hours (fixes + execution)

### Option B: Execute Then Debug

1. Run /orchestrate with current tasks
2. Deal with 3-4 failures
3. Debug and fix issues
4. Re-run failed tasks

**Total Time:** 15-20+ hours (includes debugging, token waste, re-runs)

### Option C: Fix Critical Only, Accept Medium Risk

1. Fix task 4 system message role (30 min)
2. Fix task 5 implementation code (2 hours)
3. Fix task 7 shim replacement (3-4 hours)
4. Run /orchestrate

**Total Time:** 6-8 hours
**Expected Success Rate:** 70-80%

---

## Decision Matrix

| Criterion | Option A | Option B | Option C |
|-----------|----------|----------|----------|
| Total Time | 8-10 hrs | 15-20+ hrs | 6-8 hrs |
| Success Rate | 85-95% | 40-60% | 70-80% |
| Token Usage | 80-120K | 250K+ | 120-180K |
| Rework Needed | Minimal | High | Medium |
| Risk | Low | High | Medium |

**Recommendation:** Option A (fix all critical issues)

---

## Next Steps

1. Review this analysis with user
2. Get decision on which option
3. If Option A: Fix tasks in this order:
   - Task 4 (30 min) - prevents provider breakage
   - Task 5 (2 hrs) - prevents buggy implementation
   - Task 7 (3-4 hrs) - prevents task failure
   - Skills (10 min) - prevents research sub-agents
   - Line numbers (1 hr) - reduces token waste

4. Run /orchestrate after fixes complete

---

## Detailed Fix Checklist

### Task 4 Fixes
- [ ] Remove system message from conversation array OR
- [ ] Create IK_ROLE_SYSTEM role type
- [ ] Update request builder to handle system prompts correctly
- [ ] Verify with all three providers

### Task 5 Fixes
- [ ] Remove all 80 lines of implementation code
- [ ] Replace with interface specification
- [ ] Show function signature and contracts only
- [ ] Specify error handling requirements
- [ ] Specify memory ownership rules
- [ ] Remove arbitrary 1024 limit
- [ ] Fix error context usage

### Task 7 Fixes
- [ ] Show complete shim replacement pattern
- [ ] Provide OpenAI JSON format examples
- [ ] Show message serialization for both APIs (chat, responses)
- [ ] Show tool call handling
- [ ] List all functions that change
- [ ] Show before/after for request_chat.c
- [ ] Show before/after for request_responses.c

### Skill Fixes
- [ ] Task 1: Add `database`, `log`
- [ ] Task 2: Add `log`
- [ ] Task 5: Add `mocking` or remove wrapper requirements
- [ ] Task 7: Add `source-code`

### Line Number Fixes
- [ ] Task 2: Replace line numbers with function names
- [ ] Task 3: Replace line numbers with function names
- [ ] Task 4: Replace line numbers with function names
- [ ] Task 5: Replace line numbers with function names (if any remain)
- [ ] Task 6: Verify function names used
- [ ] Task 7: Add function context

---

**End of Critical Gaps Summary**
