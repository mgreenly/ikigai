# Task: Fix /clear Autocomplete Persistence Bug

## Target

User Story: docs/rel-05/user-stories/47-clear-hides-autocomplete.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/rel-05/user-stories/47-clear-hides-autocomplete.md

## Pre-read Source (patterns)
- src/commands.c (slash command handlers, find cmd_clear)
- src/completion.h (ik_completion_clear function)
- src/completion.c (clear implementation)
- src/layer_completion.c (completion layer rendering)

## Pre-conditions
- `make check` passes
- Autocomplete system works correctly during typing
- `/clear` command clears scrollback, messages, and marks
- Bug present: autocomplete suggestions persist after `/clear` executes

## Task

Fix the bug where autocomplete suggestions remain visible after executing `/clear` command.

**Root cause:** The `/clear` command handler clears scrollback, messages, and marks, but does not clear the autocomplete state. The completion layer continues to render stale suggestions.

**Solution:** Call `ik_completion_clear()` in the `/clear` command handler to reset autocomplete state.

## TDD Cycle

### Red

1. Create test in `tests/unit/repl/completion_clear_command_test.c` (or add to existing completion test file):
   - Setup REPL with autocomplete enabled
   - Type `/cle` to trigger autocomplete (suggestions should appear)
   - Verify `repl->completion->match_count > 0`
   - Execute `/clear` command
   - Verify `repl->completion->match_count == 0` after clear
   - Verify completion layer renders nothing (no suggestions visible)

2. Run `make check` - expect test to FAIL (suggestions persist)

### Green

1. Locate the `/clear` command handler in `src/commands.c` (look for `cmd_clear` function)

2. After the existing clear operations (scrollback, messages, marks), add:
   ```c
   // Clear autocomplete state so suggestions don't persist
   ik_completion_clear(repl->completion);
   ```

3. Verify `ik_completion_clear()` exists in `src/completion.h` and `src/completion.c`
   - If it doesn't exist, implement it:
   ```c
   void ik_completion_clear(ik_completion_t *completion)
   {
       assert(completion != NULL);  // LCOV_EXCL_BR_LINE

       completion->match_count = 0;
       completion->selected_idx = 0;
       completion->trigger_offset = 0;
       // Clear any other state fields that track active suggestions
   }
   ```

4. Run `make check` - expect PASS

### Verify

1. Run `make check` - all tests pass

2. Run `make lint` - complexity checks pass

3. Manual test:
   - Run `bin/ikigai`
   - Type `/cle` and verify autocomplete appears
   - Press Enter
   - Verify autocomplete suggestions are gone
   - Only clean separator and cursor visible

## Post-conditions

- `make check` passes
- `/clear` command clears autocomplete state
- Autocomplete suggestions do not persist after `/clear` executes
- Test exists verifying autocomplete is cleared
- Manual testing confirms fix
