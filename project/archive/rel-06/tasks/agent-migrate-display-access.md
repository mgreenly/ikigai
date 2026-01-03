# Task: Migrate Display Field Access

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 3 (Migration)

## Model

sonnet/extended (11 fields, many files - use sub-agents)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm

### Docs

- project/memory.md (talloc ownership)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t)

### Source Files

- src/agent_display.h (display struct with layer enum)
- src/agent.h (current ik_agent_ctx_t with both legacy and display fields)
- src/layer.h (layer types)

### Test Patterns

- tests/unit/agent/agent_test.c (agent tests)
- tests/unit/layer/*.c (layer tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_agent_display_t *display` field exists in ik_agent_ctx_t
- Both legacy and display fields populated during creation
- Identity field migration complete (from agent-migrate-identity-access.md)

## Task

Migrate all access to display fields from legacy to sub-context. This is the largest migration (11 fields) requiring careful coordination.

### What

Replace all occurrences:

**Scrollback and Layer Cake:**
- `agent->scrollback` -> `agent->display->scrollback`
- `agent->layer_cake` -> `agent->display->layer_cake`

**Individual Layers -> Array Access:**
- `agent->scrollback_layer` -> `agent->display->layers[IK_LAYER_SCROLLBACK]`
- `agent->spinner_layer` -> `agent->display->layers[IK_LAYER_SPINNER]`
- `agent->separator_layer` -> `agent->display->layers[IK_LAYER_SEPARATOR]`
- `agent->input_layer` -> `agent->display->layers[IK_LAYER_INPUT]`
- `agent->completion_layer` -> `agent->display->layers[IK_LAYER_COMPLETION]`

**State Fields:**
- `agent->viewport_offset` -> `agent->display->viewport_offset`
- `agent->spinner_state` -> `agent->display->spinner`
- `agent->separator_visible` -> `agent->display->separator_visible`
- `agent->input_buffer_visible` -> `agent->display->input_buffer_visible`
- `agent->input_text` -> `agent->display->input_text`
- `agent->input_text_len` -> `agent->display->input_text_len`

### How

**Step 1: Identify all files with display field access**

```bash
# Find all files accessing display fields
grep -rn "->scrollback\|->layer_cake\|->scrollback_layer\|->spinner_layer\|->separator_layer\|->input_layer\|->completion_layer\|->viewport_offset\|->spinner_state\|->separator_visible\|->input_buffer_visible\|->input_text" src/ tests/ \
    | grep -v "display->" \
    > /tmp/display_access.txt
```

**Step 2: Handle layer array migration**

The most complex part is migrating individual layer pointers to array access:

```c
// Before
ik_layer_t *layer = agent->scrollback_layer;
ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);

// After
ik_layer_t *layer = agent->display->layers[IK_LAYER_SCROLLBACK];
ik_layer_cake_add_layer(agent->display->layer_cake, agent->display->layers[IK_LAYER_SCROLLBACK]);
```

**Alternative:** Use inline accessors during migration:

```c
// Using accessor (simpler during migration)
ik_layer_t *layer = ik_agent_display_scrollback_layer(agent->display);
```

**Step 3: Handle spinner state rename**

Note the field rename: `spinner_state` -> `spinner`:

```c
// Before
agent->spinner_state = IK_SPINNER_VISIBLE;
if (agent->spinner_state == IK_SPINNER_HIDDEN) { ... }

// After
agent->display->spinner = IK_SPINNER_VISIBLE;
if (agent->display->spinner == IK_SPINNER_HIDDEN) { ... }
```

### Why

The display sub-context is the largest group. Key migration considerations:

1. **Layer array vs individual pointers:** Array enables iteration, but existing code uses named pointers
2. **Accessor functions:** Optional bridge during migration - can be removed later
3. **State field rename:** `spinner_state` -> `spinner` for brevity in new struct

### Files Likely Affected

Based on codebase analysis:
- `src/agent.c` - layer creation, state transitions
- `src/repl.c` - render calls, layer setup
- `src/repl_callbacks.c` - render callbacks
- `src/render.c` - rendering
- `src/layer_*.c` - layer implementations
- `src/commands.c` - state changes
- `tests/unit/layer/*.c` - layer tests
- `tests/unit/render/*.c` - render tests

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior. Run `make check` before migration to establish baseline.

### Green

**CRITICAL: Use sub-agents for parallel migration**

This is a large migration. Spawn sub-agents to work in parallel:

1. **Sub-agent 1:** Migrate `src/agent.c` (layer creation, transitions)
2. **Sub-agent 2:** Migrate `src/repl.c`, `src/repl_callbacks.c`
3. **Sub-agent 3:** Migrate `src/render.c`
4. **Sub-agent 4:** Migrate `src/layer_*.c` files
5. **Sub-agent 5:** Migrate `src/commands.c`
6. **Sub-agent 6:** Migrate test files

Each sub-agent:
1. Opens assigned file(s)
2. Replaces scrollback/layer_cake access
3. Replaces layer pointer access with array access
4. Replaces state field access (note spinner_state -> spinner rename)
5. Runs `make check` to verify

**Migration Order Within File:**
1. First: scrollback, layer_cake (simple pointer redirect)
2. Second: state fields (viewport_offset, spinner, visibility)
3. Third: layer pointers (most complex - individual to array)

### Verify

After all sub-agents complete:
- `make check` passes
- `make lint` passes
- Rendering works correctly (manual verification)
- No remaining legacy display field access

```bash
# Verify no remaining legacy access
grep -rn "agent->scrollback[^_]" src/ tests/ | grep -v "display->"
grep -rn "agent->layer_cake" src/ tests/ | grep -v "display->"
grep -rn "agent->scrollback_layer\|agent->spinner_layer" src/ tests/
```

## Post-conditions

- All display field access uses `agent->display->*`
- Layer access uses array indices or accessor functions
- `spinner_state` renamed to `spinner` in all access
- All existing tests pass
- Rendering works correctly
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)

## Rollback

If migration fails partway:
1. `git checkout -- .` to revert all changes
2. Debug failing test
3. Try migration again with fix

**Alternative:** Migrate one concern at a time:
1. First commit: scrollback + layer_cake
2. Second commit: state fields
3. Third commit: layer pointers

This enables granular rollback.

## Sub-agent Usage

**MANDATORY:** Use sub-agents for parallel file migration.

```
Sub-agent assignments:
- Agent A: src/agent.c (layer creation in ik_agent_create)
- Agent B: src/repl.c, src/repl_callbacks.c
- Agent C: src/render.c
- Agent D: src/layer_input.c, src/layer_scrollback.c, src/layer_spinner.c, src/layer_separator.c, src/layer_completion.c
- Agent E: src/commands.c, src/repl_event_handlers.c
- Agent F: tests/unit/layer/*.c, tests/unit/render/*.c

Coordination:
1. Agent A completes first (defines source of truth)
2. Other agents work in parallel
3. Main agent runs final `make check`
```

**Tip:** If using accessor functions as bridge, migrate to them first, then optionally inline later.
