# Task: Add Ctrl+Arrow Input Actions for Agent Navigation

## Target
User Stories: 02-switch-next-agent.md, 03-switch-prev-agent.md, 04-switch-wrap-forward.md, 05-switch-wrap-backward.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/patterns/state-machine.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Switching Agents section)
- docs/rel-05/user-stories/02-switch-next-agent.md
- docs/rel-05/user-stories/03-switch-prev-agent.md

## Pre-read Source (patterns)
- src/input.h (input action types, ik_input_action_t)
- src/input.c (input parser, escape sequence handling)
- src/ansi.h (ANSI escape sequence definitions)
- src/repl_actions.c (action dispatch)

## Pre-read Tests (patterns)
- tests/unit/input/input_test.c (input parsing tests)
- tests/unit/input/sgr_test.c (escape sequence tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Input parser handles arrow keys (Up, Down, Left, Right)
- Input parser handles Ctrl modifiers for some keys
- `ik_input_action_t` enum exists

## Task
Add input actions for Ctrl+Left and Ctrl+Right arrow keys to enable agent navigation. These keys will trigger agent switching in the REPL.

**New actions:**
```c
typedef enum ik_input_action {
    // ... existing actions ...
    IK_INPUT_AGENT_PREV,    // Ctrl+Left Arrow - previous agent
    IK_INPUT_AGENT_NEXT,    // Ctrl+Right Arrow - next agent
} ik_input_action_t;
```

**Terminal escape sequences:**
Ctrl+Arrow keys typically emit modified CSI sequences:
- `Ctrl+Left`:  `\x1b[1;5D` (CSI 1;5 D)
- `Ctrl+Right`: `\x1b[1;5C` (CSI 1;5 C)

The `5` modifier indicates Ctrl. Some terminals may emit:
- `\x1b[5D` or `\x1bOD` variants

**Parser logic:**
```c
// In CSI sequence handler
if (final_byte == 'D') {  // Left arrow
    if (modifier == 5) {  // Ctrl modifier
        return IK_INPUT_AGENT_PREV;
    }
    return IK_INPUT_CURSOR_LEFT;
}
if (final_byte == 'C') {  // Right arrow
    if (modifier == 5) {  // Ctrl modifier
        return IK_INPUT_AGENT_NEXT;
    }
    return IK_INPUT_CURSOR_RIGHT;
}
```

## TDD Cycle

### Red
1. Add new action constants to `src/input.h`:
   ```c
   IK_INPUT_AGENT_PREV,
   IK_INPUT_AGENT_NEXT,
   ```

2. Create/update tests in `tests/unit/input/input_ctrl_arrow_test.c`:
   - Test `\x1b[1;5D` parses to `IK_INPUT_AGENT_PREV`
   - Test `\x1b[1;5C` parses to `IK_INPUT_AGENT_NEXT`
   - Test plain `\x1b[D` still parses to `IK_INPUT_CURSOR_LEFT`
   - Test plain `\x1b[C` still parses to `IK_INPUT_CURSOR_RIGHT`
   - Test Ctrl+Up (`\x1b[1;5A`) is ignored or handled separately
   - Test Ctrl+Down (`\x1b[1;5B`) is ignored or handled separately

3. Run `make check` - expect test failures

### Green
1. Update CSI parser in `src/input.c` to detect modifier parameter:
   ```c
   // Parse CSI parameters: CSI Ps ; Pm final
   // Pm=5 means Ctrl modifier
   int32_t modifier = 0;
   if (param_count >= 2) {
       modifier = params[1];
   }
   ```

2. Update arrow key handling to check modifier:
   ```c
   case 'C':  // Right arrow
       if (modifier == 5) {
           action->type = IK_INPUT_AGENT_NEXT;
       } else {
           action->type = IK_INPUT_CURSOR_RIGHT;
       }
       break;

   case 'D':  // Left arrow
       if (modifier == 5) {
           action->type = IK_INPUT_AGENT_PREV;
       } else {
           action->type = IK_INPUT_CURSOR_LEFT;
       }
       break;
   ```

3. Run `make check` - expect pass

### Refactor
1. Verify other Ctrl+Arrow combinations don't break
2. Consider: should Ctrl+Up/Down do anything? (Future: scroll viewport)
3. Add action name strings for debugging:
   ```c
   case IK_INPUT_AGENT_PREV: return "AGENT_PREV";
   case IK_INPUT_AGENT_NEXT: return "AGENT_NEXT";
   ```
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `IK_INPUT_AGENT_PREV` action exists
- `IK_INPUT_AGENT_NEXT` action exists
- Ctrl+Left parses to AGENT_PREV
- Ctrl+Right parses to AGENT_NEXT
- Plain Left/Right arrows unchanged
- No action handler yet (just parsing) - that comes in agent-switch.md
- Working tree is clean (all changes committed)
