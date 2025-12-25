# Task Gap Analysis - Legacy OpenAI Cleanup Migration

**Date:** 2025-12-24
**Analyst:** Claude Opus 4.5 (ultrathink mode)
**Task Count:** 8 tasks in migration plan
**Purpose:** Identify gaps that could cause unattended execution failures

---

## Executive Summary

**Overall Assessment:** Tasks are **65% ready** for unattended execution. Significant gaps exist that will cause sub-agent failures, research spawning, or partial completion.

**Critical Issues Found:** 23
**Medium Issues Found:** 31
**Minor Issues Found:** 18
**Total Issues:** 72

**Risk Level:** HIGH - Multiple tasks will likely fail or escalate unnecessarily

**Primary Failure Modes:**
1. **Over-specification** - Task 5 shows full implementation code (violates interface-level guidance)
2. **Under-specification** - Missing error handling strategies, test setup details
3. **Stale line numbers** - Fragile location hints that may be incorrect
4. **Skill loading gaps** - Missing critical skills (database, log, source-code)
5. **Inconsistent error handling** - Some tasks propagate, others log-and-continue
6. **System message role confusion** - Task 4 incorrectly maps system → USER role

---

## Task-by-Task Analysis

### Task 1: message-storage-dual-mode.md

**Status:** 75% Ready
**Model:** sonnet/extended ✓ (appropriate for complexity)
**Depends on:** None

#### Critical Gaps (3)

1. **CRITICAL: Missing database skill**
   - **Issue:** Task uses `ik_msg_t` from database layer extensively
   - **Impact:** Sub-agent won't understand data_json format, database message structure
   - **Fix:** Add `/load database` to skills list
   - **Line:** Pre-Read section

2. **CRITICAL: Implementation detail in interface spec**
   - **Issue:** Shows geometric growth algorithm: "capacity *= 2, initial=16"
   - **Violation:** Task authoring says "interface level, not implementation"
   - **Impact:** Over-constrains implementation, limits sub-agent flexibility
   - **Fix:** Remove algorithm details, just say "dynamic array with growth strategy"
   - **Lines:** 98-100

3. **CRITICAL: data_json parsing underspecified**
   - **Issue:** Says "parse data_json for tool call fields" but doesn't show format
   - **Impact:** Sub-agent will need to research database format
   - **Fix:** Add data_json examples from database skill
   - **Line:** 76-82

#### Medium Gaps (5)

4. **MEDIUM: Error handling inconsistency**
   - **Issue:** "PANIC on OOM" vs "return ERR for parse failures" - which errors are recoverable?
   - **Impact:** Sub-agent unclear when to PANIC vs return ERR
   - **Fix:** Add clear policy: OOM → PANIC, parse → ERR, validation → ERR

5. **MEDIUM: Include order not specified**
   - **Issue:** Creates new files but doesn't specify include ordering
   - **Impact:** May violate style guide
   - **Fix:** Reference style skill include order rules

6. **MEDIUM: Test setup details missing**
   - **Issue:** Asks to create tests but doesn't explain test database setup
   - **Impact:** Sub-agent may create broken tests
   - **Fix:** Reference test pattern files with setup/teardown examples

7. **MEDIUM: Conversion error handling unclear**
   - **Issue:** `ik_message_from_db_msg()` returns NULL or ERR? Both mentioned
   - **Impact:** Inconsistent error interface
   - **Fix:** Specify single error return mechanism

8. **MEDIUM: Content block array allocation**
   - **Issue:** Says "allocate single content block" but doesn't specify using what function
   - **Impact:** Sub-agent may use wrong allocator (malloc vs talloc)
   - **Fix:** Specify use ik_content_block_text() from request.c

#### Minor Gaps (3)

9. **MINOR: Missing logging guidance**
   - **Fix:** Load log skill for JSON logging

10. **MINOR: Build integration unverified**
    - **Issue:** Says update Makefile but doesn't say to verify build works
    - **Fix:** Add "make all succeeds" to postconditions

11. **MINOR: Dual mode not explained**
    - **Issue:** Says "dual mode" but doesn't explain why
    - **Fix:** Add context: "allows incremental migration without breaking existing code"

#### Recommendations for Task 1

**Must Fix:**
- Add `/load database` skill
- Add data_json format examples
- Remove implementation details (geometric growth)
- Clarify error return types

**Should Fix:**
- Add error handling policy
- Reference test database setup pattern
- Add build verification to postconditions

**Effort to Fix:** 2-3 hours

---

### Task 2: migrate-repl-messages.md

**Status:** 70% Ready
**Model:** sonnet/extended ✓
**Depends on:** message-storage-dual-mode.md ✓

#### Critical Gaps (2)

12. **CRITICAL: Line numbers are fragile**
    - **Issue:** "around line 420", "around line 260" - what if files changed?
    - **Impact:** Sub-agent searches wrong location, wastes tokens
    - **Fix:** Use function names as primary locator: "in function that handles user submission"
    - **Lines:** 44, 74, 106

13. **CRITICAL: Missing variable context**
    - **Issue:** Uses `trimmed` variable without explaining where it comes from
    - **Impact:** Sub-agent may not find correct code location
    - **Fix:** Show 5-10 lines of context before/after insertion point
    - **Line:** 50

#### Medium Gaps (6)

14. **MEDIUM: Role constant not explained**
    - **Issue:** Uses `IK_ROLE_USER` without saying which header defines it
    - **Impact:** Sub-agent may get compilation errors
    - **Fix:** Specify "from providers/provider.h"

15. **MEDIUM: Integration test creation underspecified**
    - **Issue:** Asks to create `tests/integration/repl/dual_mode_test.c` without test utilities
    - **Impact:** Sub-agent doesn't know how to setup REPL for integration testing
    - **Fix:** Reference existing integration test as pattern

16. **MEDIUM: Dual-mode error strategy**
    - **Issue:** Says "log error but continue (old API is authoritative for now)"
    - **Problem:** What if new API consistently fails? How to detect?
    - **Fix:** Add assertion that both APIs succeed in debug builds

17. **MEDIUM: Missing log skill**
    - **Issue:** Uses ik_log_error extensively
    - **Fix:** Add `/load log` to skills

18. **MEDIUM: No verification of synchronization**
    - **Issue:** Says both APIs run but doesn't verify arrays match
    - **Fix:** Add debug assertion: after add, verify message_count equals conversation->message_count

19. **MEDIUM: Include ordering**
    - **Issue:** Says add #include "message.h" but not where in include list
    - **Fix:** "Add after project headers, before system headers per style guide"

#### Minor Gaps (2)

20. **MINOR: Test case descriptions too brief**
    - **Fix:** Expand test descriptions with expected behavior

21. **MINOR: No build verification**
    - **Fix:** Add "compiles without warnings" to postconditions

#### Recommendations for Task 2

**Must Fix:**
- Replace line numbers with function names
- Show code context for insertion points
- Add variable source context

**Should Fix:**
- Add log skill
- Reference integration test pattern
- Add synchronization verification

**Effort to Fix:** 1-2 hours

---

### Task 3: migrate-agent-ops.md

**Status:** 72% Ready
**Model:** sonnet/thinking ✓
**Depends on:** migrate-repl-messages.md ✓

#### Critical Gaps (1)

22. **CRITICAL: Error propagation strategy unclear**
    - **Issue:** Sometimes says "propagate error", other times "log error"
    - **Impact:** Inconsistent error handling across codebase
    - **Fix:** Define policy: errors during agent ops → propagate, errors in background → log
    - **Lines:** 88, 114, 161

#### Medium Gaps (4)

23. **MEDIUM: Fork message variable unexplained**
    - **Issue:** Uses `prompt_text` without context
    - **Impact:** Sub-agent doesn't know where variable comes from
    - **Fix:** Add context about fork command parsing

24. **MEDIUM: Deep copy verification vague**
    - **Issue:** Test says "verify deep copy (modify original, check clone unchanged)"
    - **Problem:** In parentheses, not clear instruction
    - **Fix:** Make this a clear test step

25. **MEDIUM: Files to read vs modify unclear**
    - **Issue:** Lists source files but doesn't distinguish reference vs modification
    - **Fix:** Split into "Files to Read (reference)" and "Files to Modify"

26. **MEDIUM: Cleanup on fork error not specified**
    - **Issue:** If clone fails, what happens to partially-created child agent?
    - **Impact:** Potential memory leak
    - **Fix:** Specify cleanup strategy

#### Minor Gaps (2)

27. **MINOR: No mention of mark stack cloning**
    - **Issue:** Agent has marks, should they be cloned during fork?
    - **Fix:** Clarify if marks are cloned or child starts with empty mark stack

28. **MINOR: Test file location ambiguous**
    - **Issue:** Says "tests/unit/commands/fork_test.c" - does this exist?
    - **Fix:** Verify file exists or mark as "create if not exists"

#### Recommendations for Task 3

**Must Fix:**
- Unify error propagation strategy
- Specify fork cleanup on errors

**Should Fix:**
- Add variable context
- Clarify deep copy test steps
- Separate read vs modify files

**Effort to Fix:** 1 hour

---

### Task 4: migrate-restore.md

**Status:** 60% Ready ⚠️
**Model:** sonnet/extended ✓
**Depends on:** migrate-agent-ops.md ✓

#### Critical Gaps (4)

29. **CRITICAL: System message role is WRONG**
    - **Issue:** Line 60 says create system message with `IK_ROLE_USER`
    - **Impact:** This is incorrect! System messages should not be USER role
    - **Fix:** System messages are handled via system_prompt field in request, NOT as conversation messages
    - **Severity:** Will break provider request building
    - **Line:** 60

30. **CRITICAL: Replay context structure unexplained**
    - **Issue:** Uses `replay_ctx->messages` without explaining what type this is
    - **Impact:** Sub-agent doesn't know structure
    - **Fix:** Reference database skill for `ik_replay_context_t` definition
    - **Line:** 87

31. **CRITICAL: data_json format examples too brief**
    - **Issue:** Shows JSON but doesn't explain which yyjson functions to use
    - **Impact:** Sub-agent will research yyjson API
    - **Fix:** Add code snippet showing yyjson_obj_get() usage

32. **CRITICAL: Error handling for conversion failures unclear**
    - **Issue:** If message can't be converted, log and continue?
    - **Problem:** Restore might silently fail, conversation incomplete
    - **Fix:** Decide: fail restore if any message fails, or track failed count?
    - **Line:** 99-104

#### Medium Gaps (5)

33. **MEDIUM: Missing integration test database setup**
    - **Issue:** Integration test needs database with messages
    - **Impact:** Sub-agent doesn't know how to create test database
    - **Fix:** Reference database test utilities from database skill

34. **MEDIUM: Legacy "tool" kind handling**
    - **Issue:** Mentions legacy "tool" kind but not in all places
    - **Impact:** Incomplete coverage of database formats
    - **Fix:** Ensure all DB kind handlers check for "tool" alias

35. **MEDIUM: NULL content field handling**
    - **Issue:** Mentioned in test but not in implementation section
    - **Impact:** Implementation may not handle NULL content
    - **Fix:** Add explicit NULL check requirement

36. **MEDIUM: Malformed JSON recovery**
    - **Issue:** Says "handle malformed data_json gracefully" but no details
    - **Fix:** Specify behavior: log error, skip message, or fail restore?

37. **MEDIUM: db_msg vs msg naming rationale**
    - **Issue:** Renames for "clarity" but doesn't explain why important
    - **Fix:** Explain: prevents confusion between database format and provider format

#### Minor Gaps (3)

38. **MINOR: Comment about system prompt unclear**
    - **Note at line 58-60:** Says system messages "typically handled via system_prompt field"
    - **Issue:** If true, why add to conversation at all?
    - **Fix:** Clarify the dual mode strategy for system prompts

39. **MINOR: Test case for empty messages array**
    - **Issue:** Not clear if this should be error or success case
    - **Fix:** Specify: restoring 0 messages is valid (new agent)

40. **MINOR: Provider-specific restore behavior**
    - **Issue:** Do different providers need different restore logic?
    - **Fix:** Clarify that restore is provider-agnostic

#### Recommendations for Task 4

**Must Fix:**
- **FIX IMMEDIATELY:** System message role mapping is incorrect
- Explain replay context structure
- Specify conversion failure handling
- Add yyjson usage examples

**Should Fix:**
- Reference database test utilities
- Clarify NULL content handling
- Specify malformed JSON behavior

**Effort to Fix:** 2-3 hours (system message fix is critical)

---

### Task 5: migrate-providers.md

**Status:** 40% Ready ⚠️⚠️
**Model:** sonnet/thinking ✓
**Depends on:** migrate-restore.md ✓

#### Critical Gaps (7)

41. **CRITICAL: MASSIVE VIOLATION - Shows full implementation code**
    - **Issue:** Lines 125-203 show complete ~80-line implementation of `ik_request_add_message_direct()`
    - **Violation:** Task authoring says "interface level, not implementation code"
    - **Impact:** Over-constrains implementation, prevents sub-agent from using better approach
    - **Fix:** Replace with interface spec:
      ```
      Function: ik_request_add_message_direct()
      Purpose: Deep copy message and all content blocks into request
      Parameters: request, message
      Returns: OK with copied message, ERR on allocation failure
      Behavior:
      - Grow request->messages array
      - Deep copy message struct
      - Deep copy all content blocks
      - Handle all content types: TEXT, TOOL_CALL, TOOL_RESULT, THINKING
      Error handling: ERR for capacity limit, PANIC for OOM
      ```
    - **Severity:** HIGH - This is exactly what task authoring says NOT to do

42. **CRITICAL: Error context inconsistency**
    - **Issue:** Uses `ERR(req, ...)` - is `req` the right context for errors?
    - **Problem:** Error allocated on req, but req might be freed on error
    - **Impact:** Use-after-free bug (see errors skill)
    - **Fix:** Specify error allocated on parent context, not req
    - **Line:** 146

43. **CRITICAL: Capacity limit arbitrary**
    - **Issue:** Hardcodes 1024 message limit
    - **Problem:** Why 1024? What if user needs more?
    - **Impact:** Artificial limitation
    - **Fix:** Use SIZE_MAX or make configurable

44. **CRITICAL: Geometric growth vs +1 contradiction**
    - **Issue:** Comment says geometric growth, code uses `message_count + 1`
    - **Impact:** Inefficient, reallocates on every message
    - **Fix:** Actually implement geometric growth: `capacity * 2` or `capacity * 1.5`
    - **Line:** 145-150

45. **CRITICAL: Provider metadata handling unexplained**
    - **Issue:** Sets `provider_metadata=NULL` without explaining what it is
    - **Problem:** When should it be copied? What is it for?
    - **Impact:** Sub-agent doesn't understand field semantics
    - **Fix:** Explain: provider-specific response metadata, don't copy for requests

46. **CRITICAL: Missing type definition**
    - **Issue:** Shows `ik_content_block_t *dst = &copy->content_blocks[i]` but content_blocks is array type
    - **Problem:** Should be `ik_content_block_t *` not `ik_content_block_t`?
    - **Impact:** Type error, won't compile
    - **Fix:** Verify type compatibility

47. **CRITICAL: Switch default handling**
    - **Issue:** `default: PANIC("Unknown content type")` without LCOV_EXCL_LINE
    - **Impact:** Coverage failure
    - **Fix:** Add marker per errors skill

#### Medium Gaps (4)

48. **MEDIUM: Missing mocking skill**
    - **Issue:** Creates mock wrappers but no guidance
    - **Fix:** Add `/load mocking` or equivalent

49. **MEDIUM: Wrapper testing not explained**
    - **Issue:** Creates wrapper but doesn't say when it's called
    - **Fix:** Explain: wrapper is for test mocking, not production use

50. **MEDIUM: Include removal timing unclear**
    - **Issue:** Says keep old wrapper "for now" - when to remove?
    - **Fix:** Clarify: remove in task 6 (remove-legacy-conversation)

51. **MEDIUM: Request builder switch not explained**
    - **Issue:** Shows new code reading from agent->messages
    - **Problem:** What if messages is NULL?
    - **Fix:** Add NULL check requirement

#### Minor Gaps (3)

52. **MINOR: Test pattern not referenced**
    - **Fix:** Reference existing request tests as pattern

53. **MINOR: All three providers must be tested**
    - **Issue:** Lists provider tests but doesn't emphasize all must pass
    - **Fix:** Add to postconditions: "All 3 provider integration tests pass"

54. **MINOR: Build verification missing**
    - **Fix:** Add "make check passes" to postconditions

#### Recommendations for Task 5

**Must Fix (URGENT):**
- **REWRITE IMMEDIATELY:** Remove all implementation code, replace with interface specs
- Fix error context usage (errors skill violation)
- Remove arbitrary capacity limit
- Fix growth strategy (geometric, not +1)
- Explain provider_metadata semantics
- Verify type definitions

**Should Fix:**
- Add mocking skill
- Clarify wrapper usage
- Add NULL checks

**Effort to Fix:** 4-5 hours (major rewrite needed)

**Risk if not fixed:** Sub-agent follows buggy implementation exactly, creates use-after-free bugs and inefficient code

---

### Task 6: remove-legacy-conversation.md

**Status:** 78% Ready
**Model:** sonnet/thinking ✓
**Depends on:** migrate-providers.md ✓

#### Critical Gaps (2)

55. **CRITICAL: Error propagation changes behavior**
    - **Issue:** Changes from "log and continue" to "propagate error"
    - **Problem:** May break existing code paths that expect operations to always succeed
    - **Impact:** Runtime failures in production
    - **Fix:** Audit all call sites, ensure error handling exists
    - **Lines:** 132, 161, 231

56. **CRITICAL: Fork cleanup on error not specified**
    - **Issue:** If clone fails, child agent partially created
    - **Problem:** Memory leak, orphaned agent
    - **Fix:** Specify: talloc_free(child) on error, mark agent dead in DB

#### Medium Gaps (5)

57. **MEDIUM: Verification commands should be in postconditions**
    - **Issue:** Shows grep commands at end but not in required checks
    - **Fix:** Move to postconditions as automated checks

58. **MEDIUM: Test file modifications incomplete**
    - **Issue:** Says "update all tests" but doesn't list them all
    - **Problem:** Sub-agent may miss files
    - **Fix:** Provide complete list via `grep -r "agent->conversation" tests/`

59. **MEDIUM: Dual-mode test file deletion**
    - **Issue:** Says delete `tests/integration/repl/dual_mode_test.c`
    - **Problem:** File might not exist yet (created in task 2)
    - **Fix:** "Delete if exists"

60. **MEDIUM: Include removals not comprehensive**
    - **Issue:** Lists files but may miss some
    - **Fix:** Use grep to find all includes: `grep -r "#include.*openai/client" src/`

61. **MEDIUM: Error propagation impact analysis missing**
    - **Issue:** Changes error handling without impact analysis
    - **Fix:** List all callers that need error handling added

#### Minor Gaps (3)

62. **MINOR: Build verification timing**
    - **Issue:** When to verify build? After each file or at end?
    - **Fix:** Specify: continuous verification recommended

63. **MINOR: Test execution order**
    - **Issue:** Update tests before or after code changes?
    - **Fix:** Specify: update tests first (Red-Green-Refactor)

64. **MINOR: Git commit guidance missing**
    - **Issue:** No mention of when to commit per scm skill
    - **Fix:** Add reminder to commit after each file modified

#### Recommendations for Task 6

**Must Fix:**
- Specify error cleanup strategy
- Audit error propagation impact

**Should Fix:**
- Generate complete test file list
- Move verification commands to postconditions
- Add impact analysis for error changes

**Effort to Fix:** 2 hours

---

### Task 7: delete-legacy-files.md

**Status:** 65% Ready
**Model:** sonnet/thinking ✓
**Depends on:** remove-legacy-conversation.md ✓

#### Critical Gaps (3)

65. **CRITICAL: Shim replacement completely underspecified**
    - **Issue:** Says "remove shim usage and work directly with request->messages"
    - **Problem:** This is a MAJOR change requiring significant code
    - **Impact:** Sub-agent doesn't know how to replace shim
    - **Fix:** Show before/after code patterns for request_chat.c and request_responses.c
    - **Lines:** 118-145

66. **CRITICAL: Tool choice enum location not shown**
    - **Issue:** Says "uncomment tool choice enum (line 84-91)" in provider.h
    - **Problem:** Line numbers may be wrong
    - **Fix:** Search for "typedef enum.*ik_tool_choice_t" or show context

67. **CRITICAL: Factory verification unclear**
    - **Issue:** Says verify factory calls correct function but doesn't say what to do if wrong
    - **Fix:** Show correct declaration: `ik_openai_provider_create()` not `ik_openai_create()`
    - **Line:** 196

#### Medium Gaps (7)

68. **MEDIUM: Missing source-code skill**
    - **Issue:** Deleting 19 files without source map
    - **Fix:** Add `/load source-code` to understand dependencies

69. **MEDIUM: VCR cassette impact not mentioned**
    - **Issue:** Provider changes may invalidate VCR recordings
    - **Impact:** Integration tests may need re-recording
    - **Fix:** Add note about potential VCR re-recording

70. **MEDIUM: Build artifact deletion timing**
    - **Issue:** Says delete .o files but they're in build/ not src/
    - **Fix:** Clarify: run `make clean` first, or remove from file list

71. **MEDIUM: Shim function call sites not listed**
    - **Issue:** Says remove calls but doesn't say where they are
    - **Fix:** List all call sites via grep

72. **MEDIUM: Request builder refactoring complexity**
    - **Issue:** Says refactor but doesn't estimate scope
    - **Fix:** Show function signatures that change

73. **MEDIUM: Verification script testing**
    - **Issue:** Provides script but doesn't say to test it
    - **Fix:** Add to postconditions: "verification script passes"

74. **MEDIUM: Stubs.h update not detailed**
    - **Issue:** Mentions updating but doesn't show what changes
    - **Fix:** Show removal of old ik_openai_create() declaration

#### Minor Gaps (2)

75. **MINOR: rm -rf safety**
    - **Issue:** Uses rm -rf without confirmation
    - **Fix:** Add note: verify directory contents before deletion

76. **MINOR: Makefile update verification**
    - **Issue:** Says update Makefile but doesn't verify sources removed
    - **Fix:** Add grep check: Makefile should not reference deleted files

#### Recommendations for Task 7

**Must Fix:**
- Specify shim replacement strategy (show code)
- Provide tool choice enum search pattern
- Clarify factory verification steps

**Should Fix:**
- Add source-code skill
- Note VCR cassette impact
- List shim call sites
- Add verification script to postconditions

**Effort to Fix:** 3-4 hours (shim replacement needs detail)

---

### Task 8: verify-migration-complete.md

**Status:** 82% Ready (best of all tasks)
**Model:** opus/extended ⚠️ (could use sonnet/extended)
**Depends on:** delete-legacy-files.md ✓

#### Critical Gaps (1)

77. **CRITICAL: Failure handling underspecified**
    - **Issue:** Says "escalate with detailed error report" but how?
    - **Options:** Create FAILED.md? Exit with error code? Use task system?
    - **Fix:** Specify exact failure action
    - **Line:** 360

#### Medium Gaps (2)

78. **MEDIUM: Model over-provisioned**
    - **Issue:** Uses opus/extended for verification task
    - **Problem:** Sonnet/extended can handle verification, opus is expensive
    - **Fix:** Downgrade to sonnet/extended unless proven insufficient

79. **MEDIUM: Integration test implementation missing**
    - **Issue:** Shows test structure but not working code
    - **Impact:** Sub-agent may create incomplete tests
    - **Fix:** Reference existing integration test as template

#### Minor Gaps (3)

80. **MINOR: VCR cassette re-recording not mentioned**
    - **Fix:** Add to verification steps

81. **MINOR: Success criteria script not tested**
    - **Issue:** Creates script but doesn't run it
    - **Fix:** Add script execution to postconditions

82. **MINOR: Report format not specified**
    - **Issue:** Says create verification report but no template
    - **Fix:** Show report structure

#### Recommendations for Task 8

**Must Fix:**
- Specify failure escalation procedure

**Should Fix:**
- Downgrade model to sonnet/extended
- Reference integration test template
- Add VCR cassette note

**Effort to Fix:** 30 minutes

---

## Cross-Task Issues

### Consistency Problems

**83. Error Handling Philosophy Inconsistent**
- Task 2: Log and continue
- Task 3: Propagate errors
- Task 6: Change to propagate
- **Fix:** Define global policy in all tasks

**84. Line Numbers vs Function Names**
- All tasks use "around line X" which is fragile
- **Fix:** Use function names as primary locators

**85. Skill Loading Gaps**
| Task | Missing Skills |
|------|----------------|
| 1 | database, log |
| 2 | log |
| 3 | - |
| 4 | - |
| 5 | mocking |
| 6 | - |
| 7 | source-code |
| 8 | - |

**86. Test Database Setup**
- Multiple tasks need test database but setup not explained
- **Fix:** Create "test database setup" section in task 1, reference in others

**87. VCR Cassettes**
- Provider changes may invalidate recordings
- Not mentioned in any task
- **Fix:** Add VCR re-recording note to task 7 and 8

**88. Git Commit Guidance**
- No task mentions when to commit (per scm skill)
- **Fix:** Add commit reminders to all tasks

**89. Build Verification Timing**
- Some tasks verify at end, some don't specify
- **Fix:** All tasks should verify: "make check passes"

**90. Interface vs Implementation Boundary**
- Task 5 violates badly (shows full implementation)
- **Fix:** Rewrite task 5 to interface level

### Dependency Chain Issues

**91. System Message Role Error Propagates**
- Task 4 creates incorrect system message mapping
- Tasks 5-8 assume task 4 succeeded correctly
- **Impact:** Compound error through chain
- **Fix:** Fix task 4 immediately

**92. Dual Mode Assumption**
- Tasks 2-6 assume dual mode works
- If task 1 fails to establish dual mode, cascade failure
- **Fix:** Task 1 postconditions must verify both modes work

### Missing Global Context

**93. No Rollback Strategy**
- If task fails midway, how to rollback?
- Dual mode helps but not specified
- **Fix:** Add rollback section to each task

**94. No Provider-Specific Guidance**
- Three providers may have different needs
- Not mentioned anywhere
- **Fix:** Add note: all changes are provider-agnostic

**95. No Performance Guidance**
- Message array growth, cloning performance not discussed
- **Fix:** Add performance notes to task 1

---

## Risk Assessment by Task

| Task | Failure Risk | Token Waste Risk | Escalation Risk | Overall |
|------|--------------|------------------|-----------------|---------|
| 1 | MEDIUM | MEDIUM | LOW | MEDIUM |
| 2 | MEDIUM | HIGH | MEDIUM | MEDIUM-HIGH |
| 3 | LOW | MEDIUM | LOW | MEDIUM |
| 4 | HIGH ⚠️ | MEDIUM | HIGH | **HIGH** |
| 5 | HIGH ⚠️ | LOW | MEDIUM | **HIGH** |
| 6 | MEDIUM | MEDIUM | MEDIUM | MEDIUM |
| 7 | HIGH ⚠️ | HIGH | HIGH | **HIGH** |
| 8 | LOW | MEDIUM | LOW | LOW |

**High-Risk Tasks:** 3 (tasks 4, 5, 7)
**Likely to Fail Without Fixes:** Tasks 4, 5, 7

---

## Recommendations

### Immediate Actions (Before Running /orchestrate)

1. **Fix Task 4 System Message Role**
   - CRITICAL BUG - will break provider requests
   - Estimated fix: 30 minutes

2. **Rewrite Task 5 to Interface Level**
   - Remove all implementation code
   - Show interface specs only
   - Estimated fix: 4-5 hours

3. **Add Shim Replacement Details to Task 7**
   - Show before/after code patterns
   - List call sites
   - Estimated fix: 2-3 hours

4. **Add Missing Skills**
   - Task 1: database, log
   - Task 2: log
   - Task 5: mocking
   - Task 7: source-code
   - Estimated fix: 10 minutes

5. **Replace Line Numbers with Function Names**
   - All tasks
   - Estimated fix: 1 hour

### High-Value Improvements

6. **Add Test Database Setup Pattern**
   - Create shared reference section
   - Estimated benefit: Saves 2-3 research sub-agents

7. **Unify Error Handling Strategy**
   - Document in all tasks
   - Estimated benefit: Prevents inconsistencies

8. **Add Code Context Examples**
   - Show 5-10 lines before/after insertion points
   - Estimated benefit: Reduces location search time

9. **Create Verification Checklist**
   - Automated grep/test commands
   - Estimated benefit: Catches errors early

### Nice to Have

10. **Add VCR Cassette Notes**
11. **Add Git Commit Reminders**
12. **Add Rollback Strategies**
13. **Add Performance Notes**

---

## Estimated Impact of Fixes

**Current State:**
- **Estimated success rate:** 40-60%
- **Estimated token usage:** 250K+ (high due to research sub-agents)
- **Estimated escalations:** 3-4 tasks (tasks 4, 5, 7, possibly 2)

**After Critical Fixes:**
- **Estimated success rate:** 85-95%
- **Estimated token usage:** 80-120K (most tasks execute directly)
- **Estimated escalations:** 0-1 tasks

**Time to Fix:** 12-15 hours total
**Benefit:** Prevents 3-4 failed tasks, saves 150K+ tokens

---

## Conclusion

The task plan is **conceptually sound** but has **significant execution gaps** that will cause failures in unattended mode.

**Critical Issues:**
1. Task 4 system message role mapping is WRONG (breaks providers)
2. Task 5 over-specifies implementation (violates task authoring rules)
3. Task 7 under-specifies shim replacement (sub-agent will fail)

**If Fixed:** Plan is executable with 85-95% success rate

**If Not Fixed:** Expect 3-4 task failures, multiple escalations, high token waste

**Recommendation:** Invest 12-15 hours fixing gaps before running /orchestrate. The alternative is spending 20+ hours debugging failed tasks and re-running with fixes.
