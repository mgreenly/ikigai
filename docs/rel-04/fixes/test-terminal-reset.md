# Fix: Test Terminal Reset Utility

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `tests/test_utils.h` - Shared test utilities header
- `tests/test_utils.c` - Shared test utilities implementation

### Test patterns (sample files that use test_utils):
- `tests/unit/repl/repl_render_test.c` - Uses mock terminal writes
- `tests/unit/render/render_cursor_visibility_test.c` - Tests cursor escape sequences

## Situation

Tests that exercise rendering code generate escape sequences including `\x1b[?25l` (hide cursor). While these go to mock buffers, several scenarios can leak sequences to the real terminal:

1. Test assertion failures printing raw `mock_write_buffer` contents
2. Interrupted test runs (Ctrl+C)
3. Test crashes after partial output

When this happens, the user's cursor disappears and requires `reset` to fix.

## Task

Add a shared test utility function that emits terminal reset sequences, and call it at the end of each test suite that touches terminal/render code.

## Required Changes

### Step 1: Add utility function to test_utils

In `tests/test_utils.h`:
```c
/**
 * Reset terminal state after tests that may emit escape sequences.
 *
 * Call this in suite teardown for any test file that:
 * - Mocks posix_write_ for terminal output
 * - Tests rendering code
 * - Exercises cursor visibility
 *
 * Safe to call even if terminal is in normal state.
 */
void ik_test_reset_terminal(void);
```

In `tests/test_utils.c`:
```c
#include <unistd.h>

void ik_test_reset_terminal(void)
{
    // Reset sequence:
    // - \x1b[?25h  Show cursor (may be hidden)
    // - \x1b[0m    Reset text attributes (future-proof)
    //
    // Do NOT exit alternate screen - tests don't enter it.
    // Write to stdout which is where test output goes.
    const char reset_seq[] = "\x1b[?25h\x1b[0m";
    (void)write(STDOUT_FILENO, reset_seq, sizeof(reset_seq) - 1);
}
```

### Step 2: Add to test files that mock terminal writes

Add `ik_test_reset_terminal()` call to suite teardown or as final action in these test files:

**Files to update** (grep for `posix_write_` mock or cursor escape tests):
- `tests/unit/terminal/terminal_test.c`
- `tests/unit/render/render_cursor_visibility_test.c`
- `tests/unit/render/input_buffer_test.c`
- `tests/unit/render/render_scrollback_test.c`
- `tests/unit/render/render_separator_terminal_scroll_test.c`
- `tests/unit/repl/repl_render_test.c`
- `tests/unit/repl/repl_render_layers_test.c`
- `tests/unit/repl/repl_combined_render_test.c`
- `tests/unit/repl/repl_initial_state_test.c`
- `tests/unit/repl/repl_viewport_test.c`
- `tests/unit/repl/repl_viewport_defensive_test.c`
- `tests/unit/repl/repl_scrollback_visibility_test.c`
- `tests/unit/repl/repl_document_scrolling_test.c`
- `tests/unit/repl/repl_page_up_scrollback_test.c`
- `tests/unit/repl/repl_separator_last_line_test.c`
- `tests/unit/repl/repl_wrapped_scrollback_test.c`
- `tests/unit/repl/repl_exact_user_scenario_test.c`
- `tests/unit/repl/repl_debug_page_up_test.c`
- `tests/unit/repl/repl_state_machine_test.c`
- `tests/integration/repl_test.c`

**Pattern for adding to test file:**

Option A - In main() after suite runs:
```c
int main(void)
{
    Suite *s = some_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();  // Add this line

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Option B - If test uses checked fixture with teardown, add to teardown.

## Testing Strategy

The utility function itself is simple and doesn't need unit tests. Verification:
1. Run `make check` - should pass
2. Interrupt a render test with Ctrl+C - cursor should remain visible
3. Force a test failure in render test - cursor should remain visible

## Success Criteria

- `make check` passes
- `make lint` passes
- All identified test files call `ik_test_reset_terminal()` before exit
- Manual verification: cursor visible after interrupted test run
