# Task: Migrate input state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 3 (input fields migration)

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
- docs/rel-05/scratch.md (ik_agent_ctx_t input fields)

### Pre-read Source (patterns)
- src/agent.h (current agent context)
- src/agent.c (current agent create)
- src/repl.h (input fields to migrate)
- src/repl_init.c (input initialization)
- src/input_buffer/core.h (ik_input_buffer_t)

### Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- `make check` passes
- Display fields already migrated to agent context
- `repl->agent` exists

## Task
Migrate input state fields from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `input_buffer` - editable input area
- `separator_visible` - visibility flag (referenced by separator layer)
- `input_buffer_visible` - visibility flag (referenced by input layer)
- `input_text` - pointer to input text (for layer)
- `input_text_len` - length of input text (for layer)

After this task:
- Agent owns its input state
- Each agent can have independent partial input (preserved on switch)
- Access pattern becomes `repl->agent->input_buffer`, etc.

Note: `input_parser` stays in repl_ctx (it's stateless, shared).

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add forward declaration:
     ```c
     typedef struct ik_input_buffer ik_input_buffer_t;
     ```
   - Add fields:
     ```c
     // Input state (per-agent - preserves partial composition)
     ik_input_buffer_t *input_buffer;

     // Layer reference fields (updated before each render)
     bool separator_visible;
     bool input_buffer_visible;
     const char *input_text;
     size_t input_text_len;
     ```

2. Update `tests/unit/agent/agent_test.c`:
   - Test `agent->input_buffer` is initialized
   - Test `agent->separator_visible` initial value
   - Test `agent->input_buffer_visible` initial value

3. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Add include for input_buffer/core.h
   - Initialize input_buffer:
     ```c
     agent->input_buffer = ik_input_buffer_create(agent);
     ```
   - Initialize visibility flags:
     ```c
     agent->separator_visible = true;
     agent->input_buffer_visible = true;
     agent->input_text = NULL;
     agent->input_text_len = 0;
     ```

2. Update `src/repl.h`:
   - Remove `ik_input_buffer_t *input_buffer;` field
   - Remove `bool separator_visible;` field
   - Remove `bool input_buffer_visible;` field
   - Remove `const char *input_text;` field
   - Remove `size_t input_text_len;` field

3. Update `src/repl_init.c`:
   - Remove input_buffer creation (now in agent_create)
   - Remove visibility flag initialization (now in agent_create)

4. Update layer creation in `src/agent.c`:
   - Input layer needs pointers to agent's input fields:
     ```c
     agent->input_layer = ik_input_layer_create(agent,
         &agent->input_text,
         &agent->input_text_len,
         &agent->input_buffer_visible);
     ```
   - Separator layer needs pointer to agent's visibility flag:
     ```c
     agent->separator_layer = ik_separator_layer_create(agent,
         &agent->separator_visible);
     ```

5. Update ALL files that access input fields:
   - Change `repl->input_buffer` to `repl->agent->input_buffer`
   - Change `repl->separator_visible` to `repl->agent->separator_visible`
   - Change `repl->input_buffer_visible` to `repl->agent->input_buffer_visible`
   - Change `repl->input_text` to `repl->agent->input_text`
   - Change `repl->input_text_len` to `repl->agent->input_text_len`

6. Run `make check` - expect pass

### Refactor
1. Verify layer pointers reference agent fields (not stale repl fields)
2. Verify input_buffer is properly owned by agent (talloc child)
3. Verify no direct input field access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Input fields are in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- `ik_agent_create()` initializes all input state
- Layer pointers reference agent's fields
- All input access uses `repl->agent->*` pattern
- 100% test coverage maintained
