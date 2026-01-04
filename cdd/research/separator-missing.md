# Sub-agent Lower Separator Missing

## Problem

When running inside a sub-agent (child agent created via `/fork`), the lower separator line is not drawn.

## Root Cause

The lower separator layer is only added to Agent 0's layer cake during REPL initialization. Child agents don't have the lower separator in their layer cake.

## Code References

### REPL Initialization (Agent 0 only)

`src/repl_init.c:90-99`:
```c
// Initialize layer-based rendering (Phase 1.3)
// Initialize reference fields
repl->lower_separator_visible = true;  // Lower separator initially visible

// Create lower separator layer (not part of agent - stays in repl)
repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &repl->lower_separator_visible);

// Add lower separator to agent's layer cake
result = ik_layer_cake_add_layer(repl->current->layer_cake, repl->lower_separator_layer);
```

The lower separator is:
1. Created as `repl->lower_separator_layer`
2. Added only to `repl->current->layer_cake` (Agent 0's layer cake)

### Agent Creation (no lower separator)

`src/agent.c:102-128` - Layer setup in `ik_agent_create()`:
```c
// Create and add layers (following pattern from repl_init.c)
agent->scrollback_layer = ik_scrollback_layer_create(agent, "scrollback", agent->scrollback);
result = ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);

// Create spinner layer
agent->spinner_layer = ik_spinner_layer_create(agent, "spinner", &agent->spinner_state);
result = ik_layer_cake_add_layer(agent->layer_cake, agent->spinner_layer);

// Create separator layer (upper) - pass pointer to agent field
agent->separator_layer = ik_separator_layer_create(agent, "separator", &agent->separator_visible);
result = ik_layer_cake_add_layer(agent->layer_cake, agent->separator_layer);

// Create input layer
agent->input_layer = ik_input_layer_create(agent, "input", ...);
result = ik_layer_cake_add_layer(agent->layer_cake, agent->input_layer);

// Create completion layer
agent->completion_layer = ik_completion_layer_create(agent, "completion", &agent->completion);
result = ik_layer_cake_add_layer(agent->layer_cake, agent->completion_layer);
```

Note: Only the **upper separator** (`agent->separator_layer`) is added. No lower separator.

### Fork Command (uses ik_agent_create)

`src/commands_fork.c:193-202`:
```c
// Create child agent
ik_agent_ctx_t *child = NULL;
res = ik_agent_create(repl, repl->shared, parent->uuid, &child);
...
// Set repl backpointer on child agent
child->repl = repl;
```

Child agents are created via `ik_agent_create()` which doesn't add the lower separator.

### Agent Switching

`src/repl_navigation.c:30-55` - `ik_repl_switch_agent()`:
```c
res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent)
{
    ...
    // Switch current pointer
    repl->current = new_agent;

    // Update navigation context for new current agent
    ik_repl_update_nav_context(repl);

    return OK(NULL);
}
```

When switching agents, `repl->current` changes but no layers are modified.

### Rendering Path

`src/repl_viewport.c:230-233`:
```c
// Render layers to output buffer
ik_output_buffer_t *output = ik_output_buffer_create(repl, 4096);

ik_layer_cake_render(repl->current->layer_cake, output, (size_t)repl->shared->term->screen_cols);
```

Rendering uses `repl->current->layer_cake`. Since child agents' layer cakes don't have the lower separator, it's not rendered.

## Layer Stack Comparison

### Agent 0 (root) layer cake:
1. scrollback_layer
2. spinner_layer
3. separator_layer (upper)
4. input_layer
5. completion_layer
6. **lower_separator_layer** (added in repl_init.c)

### Child agent layer cake:
1. scrollback_layer
2. spinner_layer
3. separator_layer (upper)
4. input_layer
5. completion_layer

The lower separator is missing from child agents.

## Related Files

- `src/repl_init.c` - REPL initialization, adds lower separator to Agent 0
- `src/agent.c` - Agent creation, layer setup
- `src/commands_fork.c` - Fork command creates child agents
- `src/repl_navigation.c` - Agent switching
- `src/repl_viewport.c` - Rendering
- `src/layer_separator.c` - Separator layer implementation
- `src/layer.c` - Layer cake implementation
