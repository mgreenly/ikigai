# Task: Migrate Display Field Callers

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Display Field Migration

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Source (patterns)

- src/agent.h (ik_agent_ctx_t with embedded ik_agent_display_t)
- src/agent.c (display initialization in factories)
- src/render.c (main rendering code)
- src/layer_wrappers.c (layer creation)
- src/repl_actions.c
- src/repl_viewport.c
- src/repl_actions_viewport.c
- src/repl_actions_history.c
- src/marks.c
- src/completion.c
- src/commands.c
- src/commands_mail.c
- src/commands_kill.c
- src/commands_mark.c
- src/commands_agent_list.c

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (updated accessor patterns)
- tests/unit/agent/agent_display_test.c

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `agent-migrate-identity-callers` is complete
- All identity field migrations done

## Task

Migrate all production code (src/*.c) to use the new display field accessor pattern.

### What

Update all source files that access display fields to use the new path:

| Old Pattern | New Pattern |
|-------------|-------------|
| `agent->scrollback` | `agent->display.scrollback` |
| `agent->layer_cake` | `agent->display.layer_cake` |
| `agent->scrollback_layer` | `agent->display.scrollback_layer` |
| `agent->spinner_layer` | `agent->display.spinner_layer` |
| `agent->separator_layer` | `agent->display.separator_layer` |
| `agent->input_layer` | `agent->display.input_layer` |
| `agent->completion_layer` | `agent->display.completion_layer` |
| `agent->viewport_offset` | `agent->display.viewport_offset` |
| `agent->spinner_state` | `agent->display.spinner_state` |
| `agent->separator_visible` | `agent->display.separator_visible` |
| `agent->input_buffer_visible` | `agent->display.input_buffer_visible` |

### How

1. **Discovery Phase** (use sub-agents):

   Search BOTH access patterns - direct agent and via repl->current:
   ```bash
   # Direct agent access
   grep -rn "agent->scrollback\b" src/
   grep -rn "agent->layer_cake" src/
   grep -rn "agent->.*_layer\b" src/
   grep -rn "agent->viewport_offset" src/
   grep -rn "agent->spinner_state" src/
   grep -rn "agent->separator_visible\|agent->input_buffer_visible" src/

   # Indirect via repl->current (accounts for 80%+ of callsites)
   grep -rn "repl->current->scrollback\b" src/
   grep -rn "repl->current->layer_cake" src/
   grep -rn "repl->current->.*_layer\b" src/
   grep -rn "repl->current->viewport_offset" src/
   grep -rn "repl->current->spinner_state" src/
   grep -rn "repl->current->separator_visible\|repl->current->input_buffer_visible" src/
   ```

2. **Update each file**:
   - For each file with matches, update the accessor pattern
   - Preserve surrounding code exactly
   - Run `make check` after each file

3. **Expected files with many changes**:
   - `src/render.c` - Main rendering logic
   - `src/repl_callbacks.c` - Event handling
   - `src/repl_event_handlers.c` - Input handling
   - `src/commands_*.c` - Various commands
   - `src/agent.c` - State transitions
   - `src/layer_wrappers.c` - Layer setup

### Why

Display fields are the most frequently accessed fields in `ik_agent_ctx_t`. This migration:
- Is the largest migration task
- Touches rendering and UI code
- Enables clear separation of display concerns

## TDD Cycle

### Red

Code should fail to compile if old field paths are removed from struct.

### Green

1. Run grep to find all callers (expect 50-100+ locations)
2. Group files by module (render, repl, commands, etc.)
3. Update one module at a time
4. Run `make check` after each module
5. Continue until all files updated

### Refactor

1. Verify no old patterns remain
2. Run `make lint` - verify clean
3. Verify rendering still works (manual test if possible)

## Post-conditions

- `make check` passes
- `make lint` passes
- No source files use `agent->scrollback` (use `agent->display.scrollback`)
- No source files use `agent->layer_cake` (use `agent->display.layer_cake`)
- No source files use `agent->*_layer` (use `agent->display.*_layer`)
- No source files use `agent->viewport_offset` (use `agent->display.viewport_offset`)
- No source files use `agent->spinner_state` (use `agent->display.spinner_state`)
- No source files use `agent->separator_visible` (use `agent->display.separator_visible`)
- No source files use `agent->input_buffer_visible` (use `agent->display.input_buffer_visible`)
- Working tree is clean (all changes committed)

## Sub-agent Usage

**STRONGLY RECOMMENDED: Use sub-agents for this task**

Display fields have the most usages (estimated 100+ locations). Sub-agents should:
1. Process one module at a time
2. Run `make check` after each module
3. Report progress

Suggested module batches:
1. `src/render.c` (high-density)
2. `src/repl_callbacks.c`, `src/repl_event_handlers.c`
3. `src/commands_*.c` (multiple files)
4. Remaining files

Pattern for sub-agent:
```
For module "render.c":
  - Read src/render.c
  - Replace all agent->scrollback with agent->display.scrollback
  - Replace all agent->layer_cake with agent->display.layer_cake
  - etc. for all display fields
  - Write the file
  - Run make check
  - Report result
```

## Sub-Agent Execution Strategy

This task has 51 callsites across multiple files. Use sub-agents to parallelize:

1. Spawn one sub-agent per file group (e.g., repl files, command files, layer files)
2. Each sub-agent:
   - Grep for all patterns in assigned files
   - Update field access to new pattern (e.g., `agent->scrollback` → `agent->display.scrollback`)
   - Verify with `make check`
   - Report completion

### Suggested Batches:
- **Batch 1:** src/render.c (high-density)
- **Batch 2:** src/repl_callbacks.c, src/repl_event_handlers.c
- **Batch 3:** src/commands_*.c (multiple files)
- **Batch 4:** Remaining files

## Notes

**Pointer vs Embedded Access:**

Because `display` is embedded (not a pointer), the access pattern is:
```c
// Correct: embedded struct access
agent->display.scrollback

// Would be wrong if it were a pointer:
agent->display->scrollback  // Only if display were *display
```

**Special Cases:**

1. `agent->spinner_state` is a struct, not a pointer. Accesses like `agent->spinner_state.visible` become `agent->display.spinner_state.visible`

2. Layer pointers passed to functions - ensure the function receives the correct type

3. Address-of operations: `&agent->scrollback` becomes `&agent->display.scrollback`

This is the largest migration task due to the frequency of display field access.
