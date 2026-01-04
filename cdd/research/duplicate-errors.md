# Duplicate Error Messages

## Problem

Error messages are displayed twice. For example, typing an unknown command like `/bingo` produces:

```
Error: Unknown command 'bingo'
Error: Unknown command 'bingo'
```

## Root Cause

The error is added to scrollback in two places:
1. `ik_cmd_dispatch()` adds error to scrollback AND returns ERR
2. `handle_slash_cmd_()` sees the ERR and adds the error to scrollback again

## Code References

### Command Dispatcher (first error output)

`src/commands.c:141-148`:
```c
// Unknown command
char *msg = talloc_asprintf(ctx, "Error: Unknown command '%s'", cmd_name);
if (!msg) {
    PANIC("OOM");
}
ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));  // <-- First output
return ERR(ctx, INVALID_ARG, "Unknown command '%s'", cmd_name);          // <-- Returns error
```

The dispatcher:
1. Appends error message to scrollback
2. Returns an ERR result

### Slash Command Handler (second error output)

`src/repl_actions_llm.c:65-83`:
```c
static void handle_slash_cmd_(ik_repl_ctx_t *repl, char *command_text)
{
    if (strncmp(command_text + 1, "pp", 2) == 0) {
        res_t result = ik_repl_handle_slash_command(repl, command_text + 1);
        if (is_err(&result)) PANIC("allocation failed");
    } else {
        res_t result = ik_cmd_dispatch(repl, repl, command_text);
        if (is_err(&result)) {
            const char *err_msg = error_message(result.err);
            char *display_msg = talloc_asprintf(repl, "Error: %s", err_msg);
            if (display_msg != NULL) {
                ik_scrollback_append_line(repl->current->scrollback,  // <-- Second output
                                          display_msg, strlen(display_msg));
                talloc_free(display_msg);
            }
            talloc_free(result.err);
        }
    }
}
```

The handler:
1. Calls `ik_cmd_dispatch()` (which already adds error to scrollback)
2. If result is error, adds the error to scrollback again

### Error Flow

```
User types: /bingo
    ↓
handle_slash_cmd_() called
    ↓
ik_cmd_dispatch() called
    ↓
Command not found
    ↓
[1] ik_scrollback_append_line("Error: Unknown command 'bingo'")
    ↓
return ERR("Unknown command 'bingo'")
    ↓
Back in handle_slash_cmd_()
    ↓
is_err(&result) == true
    ↓
[2] ik_scrollback_append_line("Error: Unknown command 'bingo'")  // DUPLICATE!
```

## Commands That Display Errors

Other command handlers in the dispatcher also add errors to scrollback before returning ERR:

### Empty Command

`src/commands.c:93-99`:
```c
// Empty command (just "/")
if (*cmd_start == '\0') {
    char *msg = talloc_strdup(ctx, "Error: Empty command");
    ...
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return ERR(ctx, INVALID_ARG, "Empty command");
}
```

## Related Files

- `src/commands.c` - Command registry and dispatcher
- `src/repl_actions_llm.c` - Slash command handling
- `src/scrollback.c` - Scrollback buffer
