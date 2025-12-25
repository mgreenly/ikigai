# Orchestration Failure Summary

## What Happened

Orchestration stopped after completing 1/7 tasks. The second task (`tests-openai-streaming.md`) failed at all 4 escalation levels due to a missing implementation dependency.

## Root Cause

**Task:** `openai-streaming-chat.md` (marked complete)
**Expected:** Full async vtable integration for OpenAI streaming
**Delivered:** Only SSE parser implementation
**Missing:** `openai_start_stream()` vtable method (left as stub)

The task specification explicitly required vtable integration (see `scratch/tasks/done/openai-streaming-chat.md` lines 93-176), but the implementation only created the parser logic and left a stub with comment "Will be implemented in openai-streaming.md" (different task name).

## Evidence

**Stub location:** `src/providers/openai/openai.c:384-406`

```c
static res_t openai_start_stream(/* ... */) {
    // Stub: Will be implemented in openai-streaming.md
    return ERR(tmp, NOT_IMPLEMENTED, "openai_start_stream not yet implemented");
}
```

**What exists:**
- ✓ `src/providers/openai/streaming.h` - Parser interface
- ✓ `src/providers/openai/streaming_chat.c` - SSE parsing logic (413 lines)
- ✓ 18 unit tests for parser (all passing)

**What's missing:**
- ✗ HTTP request lifecycle
- ✗ SSE write callback for curl_multi
- ✗ Completion callback handler
- ✗ Vtable method implementation

## Why Tests Can't Run

Test task requires:
```c
r = provider->vt->start_stream(provider->ctx, req, stream_cb, ...);
while (running > 0) {
    provider->vt->perform(provider->ctx, &running);
}
provider->vt->info_read(provider->ctx, logger);
```

Current state:
- `start_stream()` returns `ERR(NOT_IMPLEMENTED)` immediately
- Cannot test async integration when integration doesn't exist
- All 4 escalation levels hit the same blocker

## Impact

**Blocked tasks:**
- tests-openai-streaming.md (currently failed)
- Potentially 5 remaining tasks if they depend on streaming

**Wasted resources:**
- 4 escalation attempts (sonnet/thinking → sonnet/extended → opus/extended → opus/ultrathink)
- ~25 minutes of agent time
- No progress toward solution

## The Fix

**Complexity:** Medium (~150 lines of code)
**Pattern:** Follow existing `openai_start_request()` (lines 285-382)
**Time estimate:** 2-4 hours

**Implementation:**
1. Create stream request context struct
2. Implement SSE write callback (parse "data:" lines, feed to parser)
3. Implement stream completion handler (invoke user callback)
4. Replace stub with full `openai_start_stream()` implementation
5. Add 3 async integration tests
6. Run `make check` to verify

**Files to modify:**
- `src/providers/openai/openai.c` (+150 lines, -20 lines)
- `tests/unit/providers/openai/openai_streaming_test.c` (+60 lines)

**Detailed proposal:** See `scratch/fix-proposal.md`

## Lessons Learned

### 1. Postcondition Verification Failed

**Problem:** Task marked complete despite stub implementation

**Root cause:**
- No tests existed yet (test task hadn't run)
- `make check` passed (zero test failures = success)
- No stub detection in postconditions

**Fix:**
- Add postcondition: "No ERR(NOT_IMPLEMENTED) in modified functions"
- Add smoke test requirement in implementation tasks
- Verify expected functionality exists, not just "compiles"

### 2. Dependency Contract Unclear

**Problem:** Task split across two efforts without explicit boundary

**Evidence:**
- Stub comment: "Will be implemented in openai-streaming.md"
- But only "openai-streaming-chat.md" exists
- Test task depends on "openai-streaming-chat.md" expecting full implementation

**Fix:**
- If task only implements part A, clearly document "does NOT implement part B"
- Test task should verify preconditions before executing
- Fail fast with: "Dependency incomplete: openai_start_stream is stubbed"

### 3. Escalation Didn't Help

**Problem:** All 4 levels failed the same way

**Analysis:**
- Level 1 (sonnet/thinking): Found stub, failed
- Level 2 (sonnet/extended): Found stub, failed
- Level 3 (opus/extended): Found stub, failed
- Level 4 (opus/ultrathink): Found stub, failed

**Conclusion:**
- Smarter models can't implement missing code
- Escalation useful for complex logic, not missing prerequisites
- Need precondition checking BEFORE task execution

### 4. Task Authoring Ambiguity

**Problem:** Spec said implement vtable, commit message said "async streaming"

**Commit message:**
> "Add OpenAI Chat Completions streaming support with async pattern that integrates
> with select()-based event loop."

**Reality:**
- Created parser that CAN integrate
- Did NOT create integration
- Commit message overpromised

**Fix:**
- Commit messages should accurately reflect deliverables
- If implementing parser only, say "Implement SSE parser" not "async streaming support"
- Test that descriptions match reality

## Recommendations

### Immediate Actions

1. **Implement the fix** (Option A from diagnosis.md)
   - Follow `fix-proposal.md` implementation guide
   - Should take 2-4 hours
   - Low risk (clear pattern exists)

2. **Resume orchestration**
   - After `make check` passes
   - Continue with remaining 5 tasks

### Process Improvements

1. **Add stub detection**
   - Postcondition: Grep for `ERR(.*NOT_IMPLEMENTED)` in modified files
   - Fail if found in expected implementation

2. **Precondition verification**
   - Test tasks should check dependencies
   - Example: Verify `openai_start_stream()` doesn't return NOT_IMPLEMENTED
   - Exit with clear error if precondition unmet

3. **Smoke tests in implementation tasks**
   - Don't wait for dedicated test task
   - Basic "does it work" test in same task
   - Catches stubs immediately

4. **Better completion criteria**
   - "make check passes" when no tests exist = meaningless
   - Require: "coverage increased" or "smoke test added"
   - Or: "All expected functions implemented (no stubs)"

## Files Created

- **scratch/stoppage.md** - Initial orchestration report
- **scratch/diagnosis.md** - Deep dive root cause analysis (4000+ words)
- **scratch/fix-proposal.md** - Concrete implementation guide with code
- **scratch/summary.md** - This file

## Next Steps

1. Review fix-proposal.md
2. Implement openai_start_stream() integration
3. Run make check
4. Commit fix
5. Resume orchestration: `/orchestrate scratch/tasks`
