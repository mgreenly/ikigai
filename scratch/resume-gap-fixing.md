# Resume Prompt: Task Gap Fixing for Legacy OpenAI Cleanup

**Context:** We are preparing 8 task files in `scratch/tasks/` for unattended execution. These tasks migrate the ikigai codebase from legacy OpenAI-specific conversation storage to provider-agnostic message format, then delete 21 legacy files.

**Problem:** The original task files had critical gaps (missing struct definitions, JSON schemas, algorithms, inconsistent error handling) that would block unattended execution.

**Work Status:** 13/13 gap fixes completed. Tasks are now 100% ready. All fixes verified.

---

## Your Mission

Continue fixing gaps in the 8 task files to make them ready for unattended agent execution.

**Principle:** Tasks must be **completely self-contained** - no exploration, no guessing, no research. All context provided inline.

---

## ✅ ALL WORK COMPLETED

### Session Summary (2025-12-24)

**Completed all 6 remaining gap fixes:**
1. ✅ Added error handling policy to all 9 tasks (8 original + 2 split from task 7)
2. ✅ Removed all line number references from tasks
3. ✅ Added fallback search patterns to task 2 (3 instances)
4. ✅ Added test pattern reference to task 8 (with full Check library example)
5. ✅ Split task 7 into part1 (file deletions) and part2 (request serialization)
6. ✅ Ran and passed all verification checks

**Verification Results:**
- ✓ No line numbers remain in any task file
- ✓ Error handling policy present in all 9 tasks
- ✓ 3 fallback patterns in task 2
- ✓ Test implementation pattern in task 8
- ✓ Task 7 split correctly (2 files: part1.md, part2.md)
- ✓ Original delete-legacy-files.md removed
- ✓ order.json updated with 9 tasks (was 8, now has part1+part2)

**Tasks are ready for unattended execution via /orchestrate**

---

## ✅ PREVIOUSLY COMPLETED (From Earlier Session)

### Critical Specifications Added

**Task 1 (message-storage-dual-mode.md):**
- ✅ Lines 30-38: Complete struct definitions from `src/providers/provider.h`
- ✅ Lines 94-146: data_json schemas + yyjson parsing pattern
- ✅ Lines 160-166: Geometric growth algorithm (initial=16, 2x, realloc pattern)
- ✅ Lines 94: System message policy (return NULL, not stored in array)

**Task 5 (migrate-providers.md):**
- ✅ Lines 31-34: ik_request_t struct definition
- ✅ Lines 147-196: Complete deep copy algorithm (step-by-step for 4 content types)
- ✅ Lines 99-117: Tool schema documentation

**Task 7 (delete-legacy-files.md):**
- ✅ Lines 161-253: Complete OpenAI Chat API message formats (5 types with examples)

**Cross-Task:**
- ✅ System message handling now consistent across tasks 1, 4, 6

---

## 📋 REMAINING WORK (Your Tasks)

### HIGH PRIORITY (Blocking - Do These First)

#### 1. Standardize Error Handling Policy Across All 8 Tasks

**Problem:** Inconsistent error handling strategies during dual-mode vs post-migration:
- Task 1: "PANIC on OOM, ERR for parse failures"
- Task 2: "Log error but continue (old API authoritative)"
- Task 6: "Propagate error (new API authoritative)"

**Action Required:**

Create a **standard policy section** and add to ALL 8 tasks after the "Pre-Read" section:

```markdown
## Error Handling Policy

**Memory Allocation Failures:**
- All talloc allocations: PANIC with LCOV_EXCL_BR_LINE
- Rationale: OOM is unrecoverable, panic is appropriate

**Validation Failures:**
- Return ERR allocated on parent context (not on object being freed)
- Example: `return ERR(parent_ctx, INVALID_ARG, "message")`

**During Dual-Mode (Tasks 1-4):**
- Old API calls succeed: continue normally
- New API calls fail: log error, continue (old API is authoritative)
- Pattern: `if (is_err(&res)) { ik_log_error("Failed: %s", res.err->msg); }`

**After Migration (Tasks 5-8):**
- New API calls fail: propagate error immediately
- Pattern: `if (is_err(&res)) { return res; }`

**Assertions:**
- NULL pointer checks: `assert(ptr != NULL)` with LCOV_EXCL_BR_LINE
- Only for programmer errors, never for runtime conditions
```

**Files to Edit:**
- `scratch/tasks/message-storage-dual-mode.md` - after line 27
- `scratch/tasks/migrate-repl-messages.md` - after line 27
- `scratch/tasks/migrate-agent-ops.md` - after line 25
- `scratch/tasks/migrate-restore.md` - after line 27
- `scratch/tasks/migrate-providers.md` - after line 26
- `scratch/tasks/remove-legacy-conversation.md` - after line 26
- `scratch/tasks/delete-legacy-files.md` - after line 24
- `scratch/tasks/verify-migration-complete.md` - after line 24

**Verification:** Grep for "PANIC\|ERR\|error" in each task, ensure all error handling matches policy.

---

#### 2. Remove All Line Numbers from Tasks 1-8

**Problem:** Tasks reference specific line numbers that become invalid after first task modifies files.

**Examples of problematic references:**
- Task 1: "agent.h (line 54-120)", "line 96"
- Task 2: "line ~420", "line ~260"
- Task 5: "request.c (line 418-516)"

**Action Required:**

Search and replace in ALL 8 tasks:

**Pattern 1 - Remove line number ranges:**
```markdown
BEFORE: src/agent.h (line 54-120)
AFTER:  src/agent.h - locate ik_agent_ctx_t struct definition

BEFORE: request.c (line 418-516)
AFTER:  request.c - locate ik_request_build_from_conversation() function
```

**Pattern 2 - Remove specific line references:**
```markdown
BEFORE: (line 96)
AFTER:  - locate conversation field in struct

BEFORE: around line 540, 600
AFTER:  in tool execution handlers - search for where tool result messages are created
```

**Pattern 3 - Use unique code markers:**
```markdown
INSTEAD OF: "Update agent.h Line 95-98"
USE: "Update agent.h - find 'Conversation state (per-agent)' comment section"

INSTEAD OF: "In repl_tool.c around line 520"
USE: "In repl_tool.c - search for ik_openai_msg_create_tool_call calls"
```

**Files to Edit:** All 8 tasks in `scratch/tasks/`

**Search Terms:** `line \d+`, `lines \d+-\d+`, `~\d+`, `around line`

**Verification:** Run `grep -n "line [0-9]" scratch/tasks/*.md` - should return NO matches

---

### MEDIUM PRIORITY (Improves Reliability)

#### 3. Add Fallback Search Patterns to Task 2

**File:** `scratch/tasks/migrate-repl-messages.md`

**Problem:** Exact pattern matching may fail if variable names differ:
```c
// Task expects:
ik_msg_t *msg = ik_openai_msg_create(agent->conversation, IK_ROLE_USER, trimmed);

// Actual code might be:
ik_msg_t *user_msg = ik_openai_msg_create(agent->conversation, IK_ROLE_USER, text);
```

**Action Required:**

Add fallback instructions after each "Current Code Pattern" section:

```markdown
**If exact pattern not found:**
Use grep to find all occurrences:
```bash
grep -n "ik_openai_msg_create" src/repl_actions_llm.c
grep -n "ik_openai_conversation_add_msg" src/repl_actions_llm.c
```

Then apply transformation to ALL found instances:
- After each `ik_openai_msg_create()` → add new API call with `ik_message_create_text()`
- After each `ik_openai_conversation_add_msg()` → add new API call with `ik_agent_add_message()`
```

**Locations:** Add after each pattern in sections 1-3 (lines ~49, ~75, ~109)

---

#### 4. Add Test Pattern Reference to Task 8

**File:** `scratch/tasks/verify-migration-complete.md`

**Problem:** Task says "create tests/integration/full_conversation_test.c" but doesn't specify test framework structure.

**Action Required:**

Add to section 5 (around line 151):

```markdown
**Test Implementation Pattern:**

Use existing integration test as template:
- **Copy from:** `tests/integration/providers/anthropic/basic_test.c`
- **Test framework:** Check library (CK_* macros)
- **Structure pattern:**
  ```c
  #include <check.h>
  #include "agent.h"
  #include "providers/factory.h"

  START_TEST(test_anthropic_full_flow) {
      // 1. Create agent
      ik_agent_ctx_t *agent = ik_agent_create(...);

      // 2. Add messages using ik_message_create_text()
      ik_message_t *msg = ik_message_create_text(agent, IK_ROLE_USER, "test");
      ik_agent_add_message(agent, msg);

      // 3. Build request and verify
      ik_request_t *req;
      res_t res = ik_request_build_from_conversation(agent, agent, &req);
      ck_assert(is_ok(&res));
      ck_assert_int_eq(req->message_count, 1);

      // 4. Clean up
      talloc_free(agent);
  }
  END_TEST

  Suite *suite(void) {
      Suite *s = suite_create("Full Conversation");
      TCase *tc = tcase_create("Core");
      tcase_add_test(tc, test_anthropic_full_flow);
      suite_add_tcase(s, tc);
      return s;
  }

  int main(void) {
      Suite *s = suite();
      SRunner *sr = srunner_create(s);
      srunner_run_all(sr, CK_NORMAL);
      int failed = srunner_ntests_failed(sr);
      srunner_free(sr);
      return failed;
  }
  ```
```

---

#### 5. Split Task 7 into 7a and 7b

**Problem:** Task 7 combines risky deletion (19 files) with complex refactoring (rewrite request serialization). If refactoring fails, can't rollback deleted files.

**Action Required:**

**Step 1:** Create `scratch/tasks/delete-legacy-files-part1.md`
- Copy sections 1-6 from current task 7 (file deletion only)
- End after "Delete Shim Layer"
- Postconditions: files deleted, build still works (uses shim temporarily)

**Step 2:** Create `scratch/tasks/delete-legacy-files-part2.md`
- Copy sections 7-10 from current task 7 (refactor request serialization)
- Depends on: delete-legacy-files-part1.md
- Start with "Update src/providers/openai/request_chat.c"
- Postconditions: shim removed, direct JSON serialization works

**Step 3:** Update `scratch/tasks/order.json`
- Find delete-legacy-files.md entry
- Replace with two entries:
  ```json
  {
    "task": "delete-legacy-files-part1.md",
    "group": "Cleanup",
    "model": "sonnet",
    "thinking": "thinking"
  },
  {
    "task": "delete-legacy-files-part2.md",
    "group": "Cleanup",
    "model": "sonnet",
    "thinking": "extended"
  }
  ```

**Step 4:** Delete original `scratch/tasks/delete-legacy-files.md`

---

### VERIFICATION CHECKLIST

After completing all 6 tasks, verify:

```bash
# 1. No line numbers remain
grep -E "line [0-9]|lines [0-9]+-[0-9]+|~[0-9]+" scratch/tasks/*.md
# Expected: no matches

# 2. Error handling policy present in all tasks
grep -c "Error Handling Policy" scratch/tasks/*.md
# Expected: 8 matches (one per task)

# 3. Fallback patterns in task 2
grep -c "If exact pattern not found" scratch/tasks/migrate-repl-messages.md
# Expected: 3 matches

# 4. Test pattern in task 8
grep -c "Test Implementation Pattern" scratch/tasks/verify-migration-complete.md
# Expected: 1 match

# 5. Task 7 split correctly
ls -1 scratch/tasks/delete-legacy-files*.md
# Expected: part1.md, part2.md (NOT original delete-legacy-files.md)
```

---

## SUCCESS CRITERIA

All 8 tasks (now 9 after split) are ready when:

1. ✅ No line number references anywhere
2. ✅ Consistent error handling policy in all tasks
3. ✅ All struct definitions inline with line references
4. ✅ All JSON schemas fully specified
5. ✅ All algorithms have step-by-step pseudocode
6. ✅ Fallback search patterns for fragile matches
7. ✅ Test patterns reference existing working code
8. ✅ Risky operations separated from complex refactoring

---

## HOW TO RESUME

1. Load developer persona: `/persona developer`
2. Load task skills: `/load task task-authoring`
3. Read this file to understand context
4. Start with HIGH PRIORITY tasks (error handling, line numbers)
5. Use TodoWrite to track progress through the 6 remaining fixes
6. Run verification checklist before declaring complete

**Estimated time:** 2-3 hours for remaining 6 tasks

---

## FILES YOU'LL MODIFY

**Task Files (primary work):**
- `scratch/tasks/message-storage-dual-mode.md`
- `scratch/tasks/migrate-repl-messages.md`
- `scratch/tasks/migrate-agent-ops.md`
- `scratch/tasks/migrate-restore.md`
- `scratch/tasks/migrate-providers.md`
- `scratch/tasks/remove-legacy-conversation.md`
- `scratch/tasks/delete-legacy-files.md` (will be split into part1/part2)
- `scratch/tasks/verify-migration-complete.md`

**Supporting Files:**
- `scratch/tasks/order.json` (update for task 7 split)

**This File:**
- Update this file's "COMPLETED" section as you finish each task
- Mark TODOs complete in this file to track progress

---

## NOTES

- **Do NOT re-add struct definitions** - already complete in tasks 1, 5, 7
- **Do NOT change system message handling** - already resolved consistently
- **Do NOT add new JSON schemas** - already complete in tasks 1, 7
- Focus ONLY on the 6 remaining tasks listed above
- Use Edit tool to modify existing files, not Write (preserve existing content)
- Test your regex patterns before bulk replacing line numbers
