# Plan: Lower Separator Missing in Sub-agents

## Problem

The lower separator layer is created in `repl_init.c` and added only to Agent 0's layer cake. Child agents (created via `/fork` or restored from DB) don't have it in their layer cakes. When rendering switches to a child agent's layer cake, the lower separator is missing.

## Root Cause

The lower separator is owned by `repl` (shared across agents), but only added to Agent 0's layer cake during REPL initialization.

**Code path:**
1. `repl_init.c:95` - Creates `repl->lower_separator_layer`
2. `repl_init.c:98` - Adds to `repl->current->layer_cake` (Agent 0 only)
3. `agent.c` - `ik_agent_create()` and `ik_agent_restore()` don't add lower separator
4. `repl_viewport.c:230` - Renders `repl->current->layer_cake` (missing lower separator for child agents)

## Design Decision

**Approach: Share the single lower separator across all agent layer cakes**

The lower separator is intentionally repl-owned (not agent-owned) because:
- It displays repl-level debug info (viewport offset, render time)
- Its visibility is controlled by `repl->lower_separator_visible`
- It should behave consistently across all agents

The fix is to add the shared `repl->lower_separator_layer` to each agent's layer cake after creation.

## Fix Locations

| Location | When | Change |
|----------|------|--------|
| `src/commands_fork.c` | After `ik_agent_create()` | Add `repl->lower_separator_layer` to child's layer cake |
| `src/repl/agent_restore.c` | After `ik_agent_restore()` | Add `repl->lower_separator_layer` to restored agent's layer cake |

## Layer Cake After Fix

All agents will have the same layer stack:
1. scrollback_layer
2. spinner_layer
3. separator_layer (upper)
4. input_layer
5. completion_layer
6. lower_separator_layer (shared from repl)

## Implementation

### commands_fork.c

After line ~202 (after `ik_agent_create()` succeeds and `child->repl = repl` is set):

```c
// Add shared lower separator to child's layer cake
res = ik_layer_cake_add_layer(child->layer_cake, repl->lower_separator_layer);
if (is_err(&res)) PANIC("allocation failed");
```

### repl/agent_restore.c

In `restore_child_agent()`, after line ~157 (after `agent->repl = repl` is set):

```c
// Add shared lower separator to restored agent's layer cake
res = ik_layer_cake_add_layer(agent->layer_cake, repl->lower_separator_layer);
if (is_err(&res)) PANIC("allocation failed");
```

## Testing

1. Create child agent via `/fork`
2. Switch to child agent
3. Verify lower separator is visible
4. Restart application with existing child agents in DB
5. Switch to restored child agent
6. Verify lower separator is visible

## Complexity

**Straightforward fix** - Two call sites, same one-line pattern.

**Model:** sonnet/thinking
