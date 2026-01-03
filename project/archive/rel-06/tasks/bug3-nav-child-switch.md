# Task: Fix Ctrl+Down not switching to child agent

## Target
Bug 3 from gap.md: Ctrl+Down doesn't switch to child agent

## Macro Context

**What:** Pressing Ctrl+Down when a child agent exists doesn't switch to it, even though the navigation indicator shows children exist (↓N displayed).

**Why this matters:**
- Breaks keyboard navigation between agents
- UI shows children exist but user can't navigate to them
- Confusing user experience - indicator says it should work, but it doesn't

**Observed behavior:**
1. Start with root agent that has a child
2. Navigation shows `↓-` or `↓1` indicating child exists
3. Press Ctrl+Down
4. Nothing happens (no switch, no feedback, no error)

## Root Cause (from gap.md investigation)

The bug is in `src/repl.c` `ik_repl_nav_child()` function (lines 519-543):

```c
ik_agent_ctx_t *newest = NULL;
int64_t newest_time = 0;

for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *a = repl->agents[i];
    if (a->parent_uuid &&
        strcmp(a->parent_uuid, repl->current->uuid) == 0 &&
        a->created_at > newest_time) {  // BUG: 0 > 0 is false!
        newest = a;
        newest_time = a->created_at;
    }
}
```

**Problem:**
- Legacy agent data has `created_at = 0` (from before this field was added)
- The comparison `a->created_at > newest_time` fails when `created_at = 0`
- `0 > 0` is `false`, so the child is never selected
- Meanwhile, `update_nav_context()` correctly counts children (doesn't use time comparison)

**Mismatch:**
- Child count for indicator: Counts ANY agent with matching parent_uuid (works)
- Child selection for navigation: Requires `created_at > newest_time` (fails for created_at=0)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

## Pre-read Source
- src/repl.c:519-543 (READ - `ik_repl_nav_child()` function with the bug)
- src/repl.c:569-633 (READ - `update_nav_context()` for comparison - it correctly finds children)

## Source Files to MODIFY
- src/repl.c (MODIFY - fix `ik_repl_nav_child()`)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task

### Part 1: Fix the child selection logic

Change `ik_repl_nav_child()` to handle `created_at = 0` correctly.

**Option A (Simplest):** Use `>=` and take first match if tied:
```c
// Change from:
if (a->created_at > newest_time) {

// To:
if (a->created_at >= newest_time) {
```

This works but changes semantics slightly (if multiple children have same timestamp, takes last one in array).

**Option B (Recommended):** Treat 0 as "very old but valid":
```c
// Find most recent running child
ik_agent_ctx_t *newest = NULL;
int64_t newest_time = -1;  // -1 instead of 0, so 0 > -1 is true

for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *a = repl->agents[i];
    if (a->parent_uuid &&
        strcmp(a->parent_uuid, repl->current->uuid) == 0 &&
        a->created_at > newest_time) {
        newest = a;
        newest_time = a->created_at;
    }
}
```

**Option C (Most Explicit):** Special case for first child:
```c
ik_agent_ctx_t *newest = NULL;
int64_t newest_time = 0;

for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *a = repl->agents[i];
    if (a->parent_uuid &&
        strcmp(a->parent_uuid, repl->current->uuid) == 0) {
        // If no candidate yet, take this one regardless of time
        // Otherwise, take if newer
        if (newest == NULL || a->created_at > newest_time) {
            newest = a;
            newest_time = a->created_at;
        }
    }
}
```

**Recommendation:** Use Option C - it's explicit and handles edge cases clearly.

### Part 2: Add test for the fix

Create a test that verifies navigation works with `created_at = 0`:

```c
// Test navigation to child with created_at = 0 (legacy data)
START_TEST(test_nav_child_with_zero_created_at)
{
    // Setup: Create parent agent
    // Setup: Create child agent with created_at = 0 (simulating legacy)
    // parent->created_at = 100;  // parent has valid timestamp
    // child->created_at = 0;     // child has legacy zero timestamp

    // Act: Navigate to child
    res_t res = ik_repl_nav_child(repl);

    // Assert: Should switch to child despite created_at = 0
    ck_assert(is_ok(&res));
    ck_assert_ptr_eq(repl->current, child);
}
END_TEST
```

### Part 3: Consider sibling navigation consistency

Check if `ik_repl_nav_prev_sibling()` and `ik_repl_nav_next_sibling()` have similar issues:
- Lines 426-461: `ik_repl_nav_prev_sibling()`
- Lines 463-497: `ik_repl_nav_next_sibling()`

These functions don't use created_at for selection, so they should be fine. But verify this.

## TDD Cycle

### Red
1. Create a test with a child agent that has `created_at = 0`
2. Test that `ik_repl_nav_child()` successfully navigates to it
3. Run `make check` - test should fail

### Green
1. Implement the fix in `ik_repl_nav_child()` using Option C
2. Run the test - should pass
3. Run `make check` - all tests pass
4. Run `make lint` - passes

### Refactor
1. Add comment explaining why we handle the "first child" case specially
2. Verify sibling navigation doesn't have the same issue

## Sub-agent Usage
- Use sub-agents to verify there are no other places in the codebase with similar time comparison issues
- Use sub-agents to check if there's a data migration that should set created_at for legacy agents

## Overcoming Obstacles
- If tests are hard to set up, create a minimal repl_ctx with just the necessary fields
- If the fix causes test failures elsewhere, investigate whether those tests rely on the buggy behavior

## Post-conditions
- `make` compiles successfully
- `make check` passes
- `make lint` passes
- Ctrl+Down successfully navigates to child agents, even with `created_at = 0`
- Navigation indicator and actual navigation behavior are consistent
- Working tree is clean (all changes committed)

## Deviations
Document any deviation from this plan with reasoning.
