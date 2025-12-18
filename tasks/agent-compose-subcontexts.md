# Task: Refactor ik_agent_ctx_t to Compose Sub-contexts

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Composition

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/ddd.md
- .agents/skills/di.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/factory.md

## Pre-read Source (patterns)

- src/agent.h (current ik_agent_ctx_t and all sub-context definitions)
- src/agent.c (current ik_agent_create and ik_agent_restore)
- src/shared.h (example of composed context)

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (existing agent tests)
- tests/unit/agent/agent_identity_test.c (sub-context test pattern)
- tests/unit/agent/agent_display_test.c
- tests/unit/agent/agent_llm_test.c
- tests/unit/agent/agent_tool_executor_test.c

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- All four sub-context tasks are complete:
  - `agent-identity-struct` - `ik_agent_identity_t` exists
  - `agent-display-struct` - `ik_agent_display_t` exists
  - `agent-llm-struct` - `ik_agent_llm_t` exists
  - `agent-tool-executor-struct` - `ik_agent_tool_executor_t` exists

## Task

Refactor `ik_agent_ctx_t` to compose sub-contexts instead of having 35+ inline fields.

### What

Transform `ik_agent_ctx_t` from:
```c
typedef struct ik_agent_ctx {
    // 35+ inline fields...
    char *uuid;
    char *name;
    // ... etc
} ik_agent_ctx_t;
```

To:
```c
typedef struct ik_agent_ctx {
    // Sub-contexts (embedded, not pointers)
    ik_agent_identity_t identity;
    ik_agent_display_t display;
    ik_agent_llm_t llm;
    ik_agent_tool_executor_t tool;

    // Reference to shared infrastructure
    ik_shared_ctx_t *shared;

    // Backpointer to REPL context
    struct ik_repl_ctx_t *repl;

    // Input state (per-agent - preserves partial composition)
    ik_input_buffer_t *input_buffer;
    ik_completion_t *completion;

    // Layer reference fields (coupling between display and input)
    const char *input_text;
    size_t input_text_len;
} ik_agent_ctx_t;
```

**Slim agent retains (8 fields):**
1. `identity` - Embedded sub-context
2. `display` - Embedded sub-context
3. `llm` - Embedded sub-context
4. `tool` - Embedded sub-context
5. `shared` - Reference to shared infrastructure
6. `repl` - Backpointer to REPL
7. `input_buffer` - Input editing state
8. `completion` - Tab completion state
9. `input_text` - Layer reference (display coupling)
10. `input_text_len` - Layer reference (display coupling)

### How

1. In `src/agent.h`:
   - Modify `ik_agent_ctx_t` to embed sub-contexts
   - Sub-contexts are embedded (not pointers) for locality
   - Keep shared, repl, input_buffer, completion, input_text, input_text_len

2. In `src/agent.c`:
   - Modify `ik_agent_create()`:
     - Call sub-context factories with `&agent->identity`, `&agent->display`, etc.
     - Initialize remaining fields directly
   - Modify `ik_agent_restore()`:
     - Call `ik_agent_identity_restore()` with DB row values
     - Call other sub-context factories
   - Modify `agent_destructor()`:
     - Rely on embedded structs being freed with agent
     - Mutex cleanup happens via tool executor destructor

3. Update accessor pattern:
   - Old: `agent->uuid`
   - New: `agent->identity.uuid`

### Why

Composition brings several benefits:
- **Cohesion**: Related fields grouped together
- **Testability**: Sub-contexts testable in isolation
- **Readability**: Clear which concern a field belongs to
- **Maintainability**: Changes to one concern don't affect others
- **Documentation**: Structure documents relationships

Embedding (vs pointers) chosen because:
- Agent always needs all sub-contexts
- Avoids extra allocations and indirection
- Sub-contexts share agent's lifetime

## TDD Cycle

### Red

Update existing tests to use new accessor patterns:

1. Update `tests/unit/agent/agent_test.c`:
   - Change `agent->uuid` to `agent->identity.uuid`
   - Change `agent->scrollback` to `agent->display.scrollback`
   - Change `agent->conversation` to `agent->llm.conversation`
   - Change `agent->pending_tool_call` to `agent->tool.pending_tool_call`
   - etc. for all fields

2. Run `make check` - expect compilation failure

### Green

1. Modify `ik_agent_ctx_t` struct definition
2. Modify `ik_agent_create()` to use sub-context factories
3. Modify `ik_agent_restore()` to use sub-context factories
4. Update destructor as needed
5. Run `make check` - expect pass

### Refactor

1. Verify all 35+ original fields are accessible via sub-contexts
2. Verify no functionality lost
3. Run `make lint` - verify clean

## Post-conditions

- `make check` passes
- `make lint` passes
- `ik_agent_ctx_t` composes 4 sub-contexts
- Slim `ik_agent_ctx_t` has ~10 fields (down from 35+)
- All original tests pass with updated accessors
- Working tree is clean (all changes committed)

## Sub-agent Usage

- Use sub-agents to update test files (many accessor changes needed)
- Pattern: `sed` or manual edits to change `agent->uuid` to `agent->identity.uuid`
- Consider parallel sub-agents for different test file groups

## Notes

This task does NOT migrate callers in src/ - that happens in separate migration tasks. This task:
1. Changes the struct definition
2. Changes the factory functions
3. Updates tests to use new accessors

After this task, production code (src/*.c) will still compile because it uses the old field paths. The migration tasks will update those files incrementally.

**Decision: Embedded vs Pointer Sub-contexts**

We embed sub-contexts (`ik_agent_identity_t identity;`) rather than use pointers (`ik_agent_identity_t *identity;`) because:
- Agent always needs all sub-contexts (no optional sub-contexts)
- Single allocation instead of 5 separate allocations
- Better cache locality
- Simpler lifetime management
