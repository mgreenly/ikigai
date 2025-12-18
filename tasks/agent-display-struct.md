# Task: Create ik_agent_display_t Struct and Factory

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Display Sub-context

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

- src/agent.h (lines 68-81, 105-109 - display-related fields in ik_agent_ctx_t)
- src/agent.c (lines 49-117 - display state initialization)
- src/scrollback.h (scrollback interface)
- src/layer.h (layer interfaces)
- src/layer_wrappers.h (layer creation wrappers)
- src/layer_spinner.c
- src/layer_completion.c
- src/layer_input.c
- src/layer_separator.c
- src/layer_scrollback.c

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (display field initialization tests)
- tests/unit/layer/scrollback_layer_test.c (layer test patterns)
- tests/unit/agent/agent_identity_test.c (pattern from previous task)
- tests/unit/agent/meson.build (test registration pattern)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `agent-identity-struct` is complete
- `ik_agent_identity_t` struct exists

## Task

Extract display/UI fields from `ik_agent_ctx_t` into a new `ik_agent_display_t` struct with its own factory function.

### What

Create a new struct `ik_agent_display_t` containing:

**Scrollback and Layer Cake (2 fields):**
- `ik_scrollback_t *scrollback` - Conversation history buffer
- `ik_layer_cake_t *layer_cake` - Layer composition manager

**Individual Layers (5 fields):**
- `ik_layer_t *scrollback_layer` - Scrollback rendering layer
- `ik_layer_t *spinner_layer` - Loading spinner layer
- `ik_layer_t *separator_layer` - Separator line layer
- `ik_layer_t *input_layer` - User input layer
- `ik_layer_t *completion_layer` - Tab completion layer

**Viewport and Spinner State (2 fields):**
- `size_t viewport_offset` - Current scroll position
- `ik_spinner_state_t spinner_state` - Spinner animation state (embedded)

**Visibility Flags (2 fields):**
- `bool separator_visible` - Whether separator is shown
- `bool input_buffer_visible` - Whether input is shown

**Total: 11 fields**

### How

1. In `src/agent.h`:
   - Add `ik_agent_display_t` struct definition AFTER `ik_agent_identity_t`, BEFORE `ik_agent_ctx_t`
   - Add factory declaration: `res_t ik_agent_display_create(TALLOC_CTX *ctx, int32_t term_cols, int32_t term_rows, ik_agent_display_t **out);`

2. In `src/agent.c` (or new `src/agent_display.c`):
   - Implement `ik_agent_display_create()`:
     - Allocate `ik_agent_display_t` under ctx
     - Create scrollback with term_cols
     - Create layer_cake with term_rows
     - Create and add all 5 layers to cake
     - Initialize viewport_offset to 0
     - Initialize spinner_state (frame_index=0, visible=false)
     - Initialize visibility flags (both true)
     - Return error if any layer creation fails

### Why

The display fields (11 total) represent a distinct concern: how this agent is rendered. They are:
- All UI/rendering related
- Require terminal dimensions for initialization
- Change together during render operations
- Independent of identity, conversation, and LLM state

Extracting them enables:
- Testable display initialization without full agent
- Clear separation of rendering concerns
- Easier future changes to display architecture

## TDD Cycle

### Red

Create `tests/unit/agent/agent_display_test.c`:

1. Test `ik_agent_display_create()` succeeds
2. Test display->scrollback is non-NULL
3. Test display->layer_cake is non-NULL
4. Test all 5 layer pointers are non-NULL
5. Test display->viewport_offset is 0
6. Test display->spinner_state.frame_index is 0
7. Test display->spinner_state.visible is false
8. Test display->separator_visible is true
9. Test display->input_buffer_visible is true
10. Test display is allocated under provided parent context
11. Test layers are added to layer_cake in correct order

Run `make check` - expect compilation failure (struct doesn't exist yet)

### Green

1. Add struct definition to `src/agent.h`
2. Add factory declaration to `src/agent.h`
3. Implement factory in `src/agent.c`
4. Add test file to `tests/unit/agent/meson.build`
5. Run `make check` - expect pass

### Refactor

1. Verify layer creation order matches existing code in `ik_agent_create()`
2. Verify error handling for layer creation failures
3. Run `make lint` - verify clean

## Post-conditions

- `make check` passes
- `make lint` passes
- `ik_agent_display_t` struct exists in `src/agent.h`
- `ik_agent_display_create()` implemented
- Unit tests cover all display fields
- Working tree is clean (all changes committed)

## Sub-agent Usage

- Use sub-agents to search for display field usages: `grep -r "agent->scrollback\|agent->layer_cake\|agent->.*_layer" src/`
- Pattern analysis helps identify which callers need migration

## Notes

The display factory takes terminal dimensions as parameters (term_cols, term_rows) because layer initialization depends on them. These come from `shared->term` in the full agent, but the sub-context doesn't need to know about shared context.

The `input_text` and `input_text_len` fields are deliberately NOT included here - they are layer reference fields that couple to input_buffer and belong with input state.
