# Task: Display errors from slash commands

## Target
Bug fix: Slash command errors are silently discarded, leaving users with no feedback when commands fail.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/log.md

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Pre-read Source (patterns)
- READ: src/repl_actions_llm.c lines 62-71 (handle_slash_cmd_ - see `(void)result` discarding errors)
- READ: src/commands.c lines 95-156 (ik_cmd_dispatch - returns res_t, already handles unknown/empty commands)
- READ: src/commands_agent.c lines 448-510 (cmd_fork - example of handler returning errors without display)
- READ: src/scrollback.h (ik_scrollback_append_line for displaying messages)
- READ: src/error.h (res_t, is_err, error_message helper)

## Pre-read Tests (patterns)
- READ: tests/unit/commands/dispatch_test.c (test_dispatch_unknown_command - shows error display pattern)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task
Modify `handle_slash_cmd_()` in `src/repl_actions_llm.c` to display error messages to the user instead of discarding them.

**Root cause:**
At lines 68-69 in `src/repl_actions_llm.c`:
```c
res_t result = ik_cmd_dispatch(repl, repl, command_text);
(void)result;  // ERROR IS SILENTLY DISCARDED
```

When a command handler fails (e.g., `/fork` with database error), the user sees nothing.

**Important context:**
- `ik_cmd_dispatch()` already displays errors for invalid/unknown commands (empty command, unknown command)
- But when a valid command's HANDLER returns an error (e.g., DB failure), that error bubbles up and is discarded
- Looking at `cmd_fork` in `commands_agent.c`, DB errors at lines 461-502 just `return res;` without displaying to scrollback
- The fix ensures these handler errors are displayed to the user

**Fix approach:**
1. Check if result is an error
2. If error, append error message to scrollback so user sees it
3. Optionally log to logger as well

**Example fix:**
```c
res_t result = ik_cmd_dispatch(repl, repl, command_text);
if (is_err(&result)) {
    // Display error to user using error_message() helper from error.h
    // Note: err->msg is a char[256] array, use error_message() for safe access
    const char *err_msg = error_message(result.err);
    char *display_msg = talloc_asprintf(repl, "Error: %s", err_msg);
    if (display_msg != NULL) {
        ik_scrollback_append_line(repl->current->scrollback,
                                  display_msg, strlen(display_msg));
        talloc_free(display_msg);
    }
}
```

**Relevant API from error.h:**
- `is_err(const res_t *result)` - Check if result is an error
- `error_message(const err_t *err)` - Get error message (returns `err->msg` if non-empty, else `error_code_str(err->code)`)
- `result.err->msg` - char[256] buffer with error message (can access directly but `error_message()` is safer)

## TDD Cycle

### Red
1. **Note on testing**: `handle_slash_cmd_()` is a static function in `repl_actions_llm.c`, so it cannot be tested directly. However, `ik_cmd_dispatch()` already displays errors for unknown commands AND returns error results. The fix ensures errors from command handlers (like database failures in `/fork`) are also displayed.

2. **Testing approach**: The existing `tests/unit/commands/dispatch_test.c` tests `ik_cmd_dispatch()` directly. For this fix, you can verify the behavior indirectly:
   - The `dispatch_test.c` tests like `test_dispatch_unknown_command` show that errors are displayed to scrollback
   - Command handlers that fail (like DB errors in `/fork`) return errors that will now be displayed
   - Manual verification: Run the app, trigger a command error, observe the scrollback

3. **Optional test**: If a test is desired, create `tests/unit/repl/repl_actions_llm_test.c`:
   - Mock a command that returns an error
   - Call the action handler that invokes `handle_slash_cmd_()`
   - Verify error appears in scrollback

4. Run `make check` - establish baseline

### Green
1. Modify `src/repl_actions_llm.c`:
   - In `handle_slash_cmd_()`, replace `(void)result;` with error handling
   - Display error message to scrollback
   - Consider also logging via `ik_log_error()` if logger available

2. Run `make check` - expect pass

### Refactor
1. Ensure error messages are user-friendly
2. Ensure memory is properly managed (no leaks)
3. Run `make check` - verify still passing
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- Working tree is clean
- When slash commands fail, user sees error message in scrollback
- Errors are not silently discarded
