# Clear Hides Autocomplete

## Description

When user executes `/clear` command, autocomplete suggestions should disappear along with the scrollback being cleared. Currently, autocomplete suggestions remain visible after Enter is pressed.

## Transcript

**Before fix (incorrect behavior):**
```text
You are a helpful assistant for testing.

────────────────────────────────────────────────────────────────────────────────
/cle_
────────────────────────────────────────────────────────────────────────────────
  [clear   Clear scrollback, session messages, and marks]
```

User presses Enter.

```text
You are a helpful coding assistant.

────────────────────────────────────────────────────────────────────────────────
_
────────────────────────────────────────────────────────────────────────────────
  [clear   Clear scrollback, session messages, and marks]
```

Autocomplete suggestion incorrectly persists.

**After fix (correct behavior):**
```text
You are a helpful assistant for testing.

────────────────────────────────────────────────────────────────────────────────
/cle_
────────────────────────────────────────────────────────────────────────────────
  [clear   Clear scrollback, session messages, and marks]
```

User presses Enter.

```text
You are a helpful coding assistant.

────────────────────────────────────────────────────────────────────────────────
_
────────────────────────────────────────────────────────────────────────────────
```

Autocomplete suggestions correctly cleared.

## Walkthrough

1. User types `/clear` in input buffer

2. Autocomplete system detects slash command prefix, displays matching suggestions

3. User presses Enter

4. Input action parser processes Enter key, triggers command submission

5. Command handler executes `/clear` command

6. `/clear` clears scrollback, session messages, marks

7. **CRITICAL:** `/clear` must also clear autocomplete state before returning

8. Input buffer is cleared

9. Render cycle runs

10. Completion layer renders nothing (no active suggestions)

11. User sees clean prompt with new system message

## Reference

The `/clear` command handler must clear autocomplete state:

```c
// In cmd_clear handler, after clearing scrollback/messages/marks:
ik_completion_clear(repl->completion);  // Clear autocomplete state
```

Completion clear function signature:
```c
void ik_completion_clear(ik_completion_t *completion);
```

This ensures the completion layer has nothing to render on the next frame.
