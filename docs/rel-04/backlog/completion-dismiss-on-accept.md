# Completion Dismiss on Accept

## Problem

After pressing Tab to accept a completion, the completion menu remains visible with the selected item shown in inverse highlighting (solid background, dark foreground).

**Expected:** Completion menu disappears after accepting.

**Actual:** Completion menu stays visible, selected item highlighted with ANSI reverse video.

## Root Cause

`ik_repl_handle_tab_action()` in `src/repl_actions_completion.c` does not dismiss the completion after accepting it.

The function:
1. Updates input buffer with completed text
2. Returns without calling `ik_repl_dismiss_completion()`
3. `repl->completion` remains non-NULL
4. Completion layer sees `completion_is_visible() == true`
5. Renders the menu with inverse highlighting on selected item

Compare with Space key handling which correctly calls `ik_repl_dismiss_completion()` at line 206.

## Why This Matters

- Visual clutter - menu stays on screen when no longer needed
- Confusing UX - inverse highlighting suggests selection is pending
- Inconsistent with standard shell completion behavior

## Proposed Fix

Call `ik_repl_dismiss_completion(repl)` after accepting the completion in `ik_repl_handle_tab_action()`.

The dismiss function already exists and is used by:
- Space key handler (line 206)
- Escape key handler (line 141)

Simply add the same call after updating the input buffer with the accepted completion.

## Scope

- Single change: `src/repl_actions_completion.c` in `ik_repl_handle_tab_action()`
- Add call to `ik_repl_dismiss_completion(repl)` before returning
- Update tests to verify completion is dismissed after Tab accept

## Related

- Completion dismissal: `ik_repl_dismiss_completion()` in same file
- Completion visibility: `src/layer_completion.c` (`completion_is_visible()`)
- Inverse highlighting: ANSI code `\x1b[7;1m` in `completion_render()`
