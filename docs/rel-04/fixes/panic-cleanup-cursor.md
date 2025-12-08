# Fix: Panic Cleanup Cursor Restore

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/panic.c` - `ik_panic_impl()` function
- `src/panic.h` - Panic macros

## Situation

The panic handler restores terminal state but does NOT restore cursor visibility. If a PANIC occurs while cursor is hidden (scrollback mode), the user's terminal is left with a hidden cursor.

### Current panic cleanup:
```c
const char exit_alt[] = "\x1b[?1049l";
write_ignore(g_term_ctx_for_panic->tty_fd, exit_alt, 8);
```

### Required cleanup (future-proof):
```c
const char reset_seq[] = "\x1b[?25h\x1b[0m\x1b[?1049l";
write_ignore(g_term_ctx_for_panic->tty_fd, reset_seq, 18);
```

## Task

Add future-proof terminal reset sequence to `ik_panic_impl()`:
1. Show cursor (`\x1b[?25h`)
2. Reset text attributes (`\x1b[0m`) - for future color support
3. Exit alternate screen (`\x1b[?1049l`) - already present

## Required Changes

In `src/panic.c`, update `ik_panic_impl()`:

```c
// Restore terminal state if available
if (g_term_ctx_for_panic != NULL) {
    // Full terminal reset sequence:
    // - Show cursor (may be hidden in scrollback mode)
    // - Reset text attributes (future-proof for colors)
    // - Exit alternate screen buffer
    const char reset_seq[] = "\x1b[?25h\x1b[0m\x1b[?1049l";
    write_ignore(g_term_ctx_for_panic->tty_fd, reset_seq, 18);

    // Restore original termios
    tcsetattr(g_term_ctx_for_panic->tty_fd, TCSANOW,
              &g_term_ctx_for_panic->orig_termios);
}
```

## Testing Strategy

Panic handlers cannot be unit tested (they abort). This is already in LCOV_EXCL_START block. Manual verification:
1. Trigger a PANIC while in scrollback mode
2. Cursor should be visible after abort

## Success Criteria

- `make check` passes
- `make lint` passes
- Code review confirms reset sequence is correct
