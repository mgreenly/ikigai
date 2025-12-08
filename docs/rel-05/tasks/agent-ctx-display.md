# Task: Migrate display state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 2 (display fields migration)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

### Pre-read Docs
- docs/backlog/shared-context-di.md (design document)
- docs/rel-05/scratch.md (ik_agent_ctx_t display fields)

### Pre-read Source (patterns)
- src/agent.h (current agent context with identity)
- src/agent.c (current agent create)
- src/repl.h (display fields to migrate)
- src/repl_init.c (display initialization)
- src/scrollback.h (ik_scrollback_t)
- src/layer.h (ik_layer_t)
- src/layer_cake.h (ik_layer_cake_t)

### Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- `make check` passes
- `ik_agent_ctx_t` exists with identity fields
- `ik_agent_create()` works

## Task
Migrate display state fields from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `scrollback` - conversation history display
- `layer_cake` - layer stack manager
- `scrollback_layer` - scrollback layer
- `spinner_layer` - spinner layer
- `separator_layer` - separator layer
- `input_layer` - input buffer layer
- `completion_layer` - completion layer
- `viewport_offset` - scroll position

After this task:
- Agent owns its display state
- Access pattern becomes `repl->agent->scrollback`, etc.

Note: This is a large migration. Layer creation moves from repl_init to agent_create.

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add forward declarations:
     ```c
     typedef struct ik_scrollback ik_scrollback_t;
     typedef struct ik_layer_cake ik_layer_cake_t;
     typedef struct ik_layer ik_layer_t;
     ```
   - Add fields:
     ```c
     // Display state (per-agent)
     ik_scrollback_t *scrollback;
     ik_layer_cake_t *layer_cake;
     ik_layer_t *scrollback_layer;
     ik_layer_t *spinner_layer;
     ik_layer_t *separator_layer;
     ik_layer_t *input_layer;
     ik_layer_t *completion_layer;

     // Viewport state
     size_t viewport_offset;
     ```

2. Update `ik_agent_create()` signature to receive shared context (needed for terminal dimensions):
   ```c
   res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared, size_t numeric_id, ik_agent_ctx_t **out);
   ```

3. Update `tests/unit/agent/agent_test.c`:
   - Test `agent->scrollback` is initialized
   - Test `agent->layer_cake` is initialized
   - Test all layer pointers are non-NULL
   - Test `agent->viewport_offset` is 0 initially

4. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Add includes for scrollback.h, layer.h, layer_cake.h, layer_wrappers.h
   - Initialize scrollback:
     ```c
     agent->scrollback = ik_scrollback_create(agent, shared->term->screen_cols);
     ```
   - Initialize layer_cake:
     ```c
     agent->layer_cake = ik_layer_cake_create(agent);
     ```
   - Create and add layers (following pattern from repl_init.c):
     ```c
     agent->scrollback_layer = ik_scrollback_layer_create(agent, agent->scrollback);
     ik_layer_cake_add(agent->layer_cake, agent->scrollback_layer);
     // ... etc for other layers
     ```
   - Initialize viewport_offset to 0

2. Update `src/repl.h`:
   - Add forward declaration: `typedef struct ik_agent_ctx ik_agent_ctx_t;`
   - Add field: `ik_agent_ctx_t *agent;`
   - Remove display fields (scrollback, layer_cake, all layers, viewport_offset)

3. Update `src/repl_init.c`:
   - Create agent after shared context setup:
     ```c
     res_t result = ik_agent_create(repl, repl->shared, 0, &repl->agent);
     ```
   - Remove scrollback creation (now in agent_create)
   - Remove layer_cake creation (now in agent_create)
   - Remove layer creation (now in agent_create)

4. Update ALL files that access display fields:
   - Change `repl->scrollback` to `repl->agent->scrollback`
   - Change `repl->layer_cake` to `repl->agent->layer_cake`
   - Change `repl->scrollback_layer` to `repl->agent->scrollback_layer`
   - Change `repl->spinner_layer` to `repl->agent->spinner_layer`
   - Change `repl->separator_layer` to `repl->agent->separator_layer`
   - Change `repl->input_layer` to `repl->agent->input_layer`
   - Change `repl->completion_layer` to `repl->agent->completion_layer`
   - Change `repl->viewport_offset` to `repl->agent->viewport_offset`

5. Run `make check` - expect pass

### Refactor
1. Verify layer creation order matches original repl_init.c
2. Verify scrollback dimensions come from shared->term
3. Verify no direct display field access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Display fields are in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- `ik_agent_create()` initializes all display state
- `repl->agent` pointer exists
- All display access uses `repl->agent->*` pattern
- 100% test coverage maintained
