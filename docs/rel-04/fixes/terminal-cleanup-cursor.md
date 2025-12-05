# Fix: Terminal Cleanup Cursor Restore

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/terminal.c` - `ik_term_cleanup()` function
- `src/terminal.h` - Terminal context structure

## Situation

When the REPL is in scrollback mode, the cursor is hidden with `\x1b[?25l`. On exit, `ik_term_cleanup()` exits alternate screen but does NOT restore cursor visibility. This leaves the user's terminal with a hidden cursor.

### Current cleanup sequence:
```c
const char *exit_alt = "\x1b[?1049l";
(void)posix_write_(ctx->tty_fd, exit_alt, 8);
```

### Required cleanup sequence (future-proof):
```c
const char *reset_seq = "\x1b[?25h\x1b[0m\x1b[?1049l";
//                       ^show    ^reset  ^exit alt
//                       cursor   attrs   screen
```

## Task

Add future-proof terminal reset sequence to `ik_term_cleanup()`:
1. Show cursor (`\x1b[?25h`)
2. Reset text attributes (`\x1b[0m`) - for future color support
3. Exit alternate screen (`\x1b[?1049l`) - already present

## Required Changes

In `src/terminal.c`, update `ik_term_cleanup()`:

```c
// Exit alternate screen buffer with full terminal reset
// - Show cursor (may be hidden in scrollback mode)
// - Reset text attributes (future-proof for colors)
// - Exit alternate screen
const char *reset_seq = "\x1b[?25h\x1b[0m\x1b[?1049l";
(void)posix_write_(ctx->tty_fd, reset_seq, 18);
```

## Testing Strategy

This is difficult to unit test (escape sequences go to mock). The existing terminal tests verify cleanup is called. Manual verification:
1. Run ikigai, scroll up so cursor is hidden
2. Exit with Ctrl+D
3. Cursor should be visible in shell

## Success Criteria

- `make check` passes
- `make lint` passes
- Manual verification: cursor visible after exit from scrollback mode
