# Plan: Fix Duplicate Error Messages

## Problem Summary

Error messages for unknown/empty commands are displayed twice because:

1. `ik_cmd_dispatch()` in `src/commands.c` adds error to scrollback AND returns ERR
2. `handle_slash_cmd_()` in `src/repl_actions_llm.c` sees ERR and adds error again

## Error Flow (Current)

```
User: /bingo
    ↓
handle_slash_cmd_() calls ik_cmd_dispatch()
    ↓
ik_cmd_dispatch() finds unknown command
    ↓
[1] scrollback_append("Error: Unknown command 'bingo'")
    ↓
return ERR("Unknown command 'bingo'")
    ↓
handle_slash_cmd_() sees is_err()
    ↓
[2] scrollback_append("Error: Unknown command 'bingo'")  ← DUPLICATE
```

## Affected Code Locations

| File | Lines | Action |
|------|-------|--------|
| `src/commands.c` | 94-99 | Displays "Error: Empty command" + returns ERR |
| `src/commands.c` | 142-147 | Displays "Error: Unknown command" + returns ERR |
| `src/repl_actions_llm.c` | 72-79 | Displays error from ERR result (duplicate) |

## Recommended Fix: Remove Caller Display

Remove the duplicate error display from `handle_slash_cmd_()` in `src/repl_actions_llm.c` (lines 72-79).

**Rationale:**
- Minimal code change (just remove caller display code)
- Dispatcher already handles error formatting consistently
- Lines 75-78 are marked `LCOV_EXCL_LINE` (never tested = never reached)
- Command handlers don't add their own error display (only dispatcher does)
- No risk of breaking existing error display behavior

## Changes Required

### src/repl_actions_llm.c

**Before (lines 70-82):**
```c
    } else {
        res_t result = ik_cmd_dispatch(repl, repl, command_text);
        if (is_err(&result)) {
            const char *err_msg = error_message(result.err);
            char *display_msg = talloc_asprintf(repl, "Error: %s", err_msg);
            if (display_msg != NULL) {
                ik_scrollback_append_line(repl->current->scrollback,
                                          display_msg, strlen(display_msg));
                talloc_free(display_msg);
            }
            talloc_free(result.err);
        }
    }
```

**After:**
```c
    } else {
        res_t result = ik_cmd_dispatch(repl, repl, command_text);
        if (is_err(&result)) {
            talloc_free(result.err);
        }
    }
```

## Error Flow (After Fix)

```
User: /bingo
    ↓
handle_slash_cmd_() calls ik_cmd_dispatch()
    ↓
ik_cmd_dispatch() finds unknown command
    ↓
[1] scrollback_append("Error: Unknown command 'bingo'")
    ↓
return ERR("Unknown command 'bingo'")
    ↓
handle_slash_cmd_() sees is_err()
    ↓
talloc_free(result.err)  ← No duplicate display
```

## Testing

Existing tests in `tests/unit/commands/dispatch_test.c` call `ik_cmd_dispatch()` directly and verify error behavior. The fix removes dead code from the caller, so no new tests are needed.

Manual verification: Run `/bingo` and confirm single error line.

## Alternative Considered

**Remove display from dispatcher, keep in caller** - Would require changing dispatcher behavior and ensuring all callers handle error display. More invasive, higher risk.

## Task Complexity

- **Single file change**: `src/repl_actions_llm.c`
- **Lines changed**: ~10 lines removed
- **Risk**: Low (removing dead code)
- **Model**: sonnet/thinking
