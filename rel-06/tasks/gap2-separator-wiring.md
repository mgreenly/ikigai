# Task: Wire up Separator Navigation Context

## Target
Gap 8: Separator Navigation Context (from gap.md)

## Macro Context

This task addresses **Gap 8** from gap.md - the separator navigation context function exists but is never called in production code.

**Background:**

The function `ik_separator_layer_set_nav_context()` was implemented in `separator-nav-context.md` but the task specification didn't specify WHERE to call it. As a result:

- The separator shows navigation context area (arrows, UUID)
- But navigation context is never populated with actual agent data
- User sees empty/static navigation indicators

**What this task accomplishes:**

Wire up `ik_separator_layer_set_nav_context()` to be called at the appropriate points so that:
- Parent/sibling/child navigation indicators update correctly
- Indicators reflect actual agent hierarchy
- Grayed indicators show unavailable directions
- Context updates on agent switch, fork, and kill

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

## Pre-read Docs
- rel-06/gap.md (Gap 8 section)

## Pre-read Source

**The function to wire up:**
- src/layer_separator.c (READ - find `ik_separator_layer_set_nav_context()` implementation)
- src/layer_separator.h (READ - function declaration if exists)

**Potential call sites:**
- src/repl.c (READ - find `ik_repl_switch_agent()`, understand event loop)
- src/render.c (READ - find render frame logic if applicable)
- src/commands_agent.c (READ - find `/fork` and `/kill` handlers)

**Agent hierarchy data:**
- src/repl.h (READ - `ik_repl_ctx_t`, `repl->agents[]`, `repl->current`)
- src/agent.h (READ - `ik_agent_ctx_t`, `parent_uuid` field)

## Source Files to MODIFY
- src/repl.c (MODIFY - add helper function and call from switch_agent)
- src/commands_agent.c (MODIFY - call after fork/kill to update context)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 1 tasks completed (agent restoration working)
- `ik_separator_layer_set_nav_context()` exists and is functional

## Task

### Part 1: Create navigation context calculation helper

Create a helper function to calculate navigation context for an agent:

```c
// In src/repl.c (or new file src/repl_nav_context.c)

// Calculate and update navigation context for current agent's separator
static void update_nav_context(ik_repl_ctx_t *repl)
{
    if (repl->current == NULL) return;
    if (repl->current->separator_layer == NULL) return;

    const char *parent_uuid = repl->current->parent_uuid;
    const char *prev_sibling = NULL;
    const char *next_sibling = NULL;
    size_t child_count = 0;

    // Find siblings (agents with same parent_uuid)
    // Find children (agents with parent_uuid == current->uuid)
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        if (agent == repl->current) continue;

        // Count children
        if (agent->parent_uuid != NULL &&
            strcmp(agent->parent_uuid, repl->current->uuid) == 0) {
            child_count++;
        }

        // Find siblings (same parent)
        if ((parent_uuid == NULL && agent->parent_uuid == NULL) ||
            (parent_uuid != NULL && agent->parent_uuid != NULL &&
             strcmp(parent_uuid, agent->parent_uuid) == 0)) {
            // This is a sibling - determine prev/next based on created_at
            if (agent->created_at < repl->current->created_at) {
                // Potential prev sibling (keep the most recent one)
                if (prev_sibling == NULL) {
                    prev_sibling = agent->uuid;
                } else {
                    // Find the agent with this UUID and compare
                    // Keep the one closer to current
                    for (size_t j = 0; j < repl->agent_count; j++) {
                        if (strcmp(repl->agents[j]->uuid, prev_sibling) == 0) {
                            if (agent->created_at > repl->agents[j]->created_at) {
                                prev_sibling = agent->uuid;
                            }
                            break;
                        }
                    }
                }
            } else {
                // Potential next sibling (keep the earliest one)
                if (next_sibling == NULL) {
                    next_sibling = agent->uuid;
                } else {
                    for (size_t j = 0; j < repl->agent_count; j++) {
                        if (strcmp(repl->agents[j]->uuid, next_sibling) == 0) {
                            if (agent->created_at < repl->agents[j]->created_at) {
                                next_sibling = agent->uuid;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // Update the separator layer
    ik_separator_layer_set_nav_context(
        repl->current->separator_layer,
        parent_uuid,
        prev_sibling,
        repl->current->uuid,
        next_sibling,
        child_count
    );
}
```

### Part 2: Call from ik_repl_switch_agent()

Add call to `update_nav_context()` at the end of `ik_repl_switch_agent()`:

```c
void ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *target)
{
    // ... existing switch logic ...

    repl->current = target;

    // Update navigation context for new current agent
    update_nav_context(repl);
}
```

### Part 3: Call after fork command

In `src/commands_agent.c`, after successfully creating a forked agent, call the context update:

```c
// After fork completes and child is added to array:
// Need to update parent's context (child_count changed)
// and child's context (new agent)

// Option A: Call update for both
update_nav_context(repl);  // Updates current (child after auto-switch)

// Note: If we auto-switch to child, parent's context is stale
// but will update when user switches back
```

### Part 4: Call after kill command

In `src/commands_agent.c`, after successfully killing an agent, update context:

```c
// After kill removes agent from array:
// Update current agent's navigation context
update_nav_context(repl);
```

### Part 5: Initial context on startup

In `src/repl_init.c`, after agents are restored, set initial context:

```c
// After ik_repl_restore_agents() returns:
update_nav_context(repl);
```

Or call from `ik_repl_restore_agents()` at the end.

### Implementation Notes

1. **Separator layer location:** Each agent has its own separator layer. Check if it's `agent->separator_layer` or accessed via `agent->layer_cake`.

2. **Visibility check:** Only update if separator layer is visible/exists.

3. **Performance:** This iterates all agents to find siblings/children. For small agent counts (<100), this is fine. For larger counts, consider caching.

4. **Lower separator:** The lower separator (owned by repl, not agent) may also need context. Check if it displays navigation info.

5. **Null safety:** Handle NULL parent_uuid (root agents), empty agent arrays, etc.

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Create a test that verifies navigation context is populated after agent switch
3. Test should fail (context not populated yet)

### Green
1. Add `update_nav_context()` helper function
2. Add call from `ik_repl_switch_agent()`
3. Add call from fork/kill command handlers
4. Add call from startup
5. Run `make` - should compile
6. Run `make check` - tests should pass
7. Run `make lint` - should pass

### Refactor
1. Consider extracting sibling-finding logic into helper
2. Ensure consistent null checking
3. Review for edge cases (single agent, no siblings, etc.)

## Unit Tests

Add tests to verify navigation context is set correctly:

### Test file: tests/unit/repl/nav_context_test.c

```c
// Test: Context populated after switch
START_TEST(test_nav_context_populated_after_switch)
{
    // Setup: Create repl with 2 agents (parent + child)
    // Switch to child
    // Verify separator has correct parent_uuid, child_count=0
}
END_TEST

// Test: Context shows correct sibling count
START_TEST(test_nav_context_shows_siblings)
{
    // Setup: Create parent with 3 children
    // Switch to middle child
    // Verify prev_sibling and next_sibling are set
}
END_TEST

// Test: Context updates after fork
START_TEST(test_nav_context_updates_after_fork)
{
    // Setup: Create agent, fork child
    // Verify parent's child_count increased (when switched back)
    // Verify child has correct parent_uuid
}
END_TEST

// Test: Context updates after kill
START_TEST(test_nav_context_updates_after_kill)
{
    // Setup: Create parent with 2 children
    // Kill one child
    // Verify surviving child's sibling info updated
}
END_TEST
```

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- `ik_separator_layer_set_nav_context()` is called from production code
- Navigation context updates on:
  - Agent switch
  - Fork (new child created)
  - Kill (agent removed)
  - Startup (after restoration)
- Parent/sibling/child indicators show correct UUIDs
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different call site locations, different helper function organization, additional edge cases), document the deviation and reasoning in your final report.
