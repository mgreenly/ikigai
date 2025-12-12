# Task: Add CSI u Protocol Support to Terminal Module

## Target

Input Handling: Probe for CSI u (Kitty keyboard protocol) support on terminal init and enable flag 9 if supported.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/scm.md

## Pre-read Docs
- terminal-emulators/README.md (CSI u protocol details)
- terminal-emulators/IMPLEMENTATION_GUIDE.md (implementation reference)
- docs/memory.md (talloc patterns)

## Pre-read Source
- src/terminal.h (ik_term_ctx_t struct)
- src/terminal.c (ik_term_init, ik_term_cleanup)
- src/wrapper.h (POSIX wrapper patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

The CSI u (Kitty keyboard protocol) enables distinguishing modified keys like Shift+Enter, Ctrl+Enter, and Alt+Enter. This is supported by modern terminals (Kitty, foot, Ghostty, Alacritty 0.15+, WezTerm with config).

Protocol details:
- Query support: `ESC[?u` - terminal responds with `ESC[?flags u` if supported
- Enable flag 9: `ESC[>9u` - flag 9 = disambiguate (1) + report all keys (8)
- Disable: `ESC[<u` - restore previous mode

Flag 9 is required for Alacritty compatibility (needs flag 8+).

## Data Structures

Add to `ik_term_ctx_t` in `src/terminal.h`:
```c
typedef struct ik_term_ctx {
    int tty_fd;
    struct termios orig_termios;
    int screen_rows;
    int screen_cols;
    bool csi_u_supported;  // True if CSI u protocol is available
} ik_term_ctx_t;
```

## API

No new public functions needed. The probing and enable/disable happens internally in init/cleanup.

## TDD Cycle

### Red

Create tests in `tests/unit/terminal/terminal_test.c` (or new file if needed):

```c
// Test: csi_u_supported field exists and is initialized
START_TEST(test_term_init_sets_csi_u_supported)
{
    // This test verifies the field exists - actual value depends on terminal
    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init(ctx, &term);

    // In test environment, may not have real terminal
    if (is_ok(&res)) {
        // Field should be either true or false (not uninitialized)
        ck_assert(term->csi_u_supported == true || term->csi_u_supported == false);
        ik_term_cleanup(term);
    }
}
END_TEST
```

Note: Full integration testing requires a real terminal. Unit tests verify the field exists and is initialized.

### Green

1. Add `csi_u_supported` field to `ik_term_ctx_t` in `src/terminal.h`

2. Update `ik_term_init()` in `src/terminal.c`:
   - After entering alternate screen, probe for CSI u support
   - Use `select()` with ~100ms timeout to wait for response
   - Parse response to detect support
   - If supported, write `ESC[>9u` to enable flag 9
   - Set `ctx->csi_u_supported` accordingly

3. Update `ik_term_cleanup()` in `src/terminal.c`:
   - If `ctx->csi_u_supported`, write `ESC[<u` before exiting alternate screen

Implementation sketch for probing:
```c
// Probe for CSI u support
static bool probe_csi_u_support(int tty_fd)
{
    // Send query
    const char *query = "\x1b[?u";
    if (posix_write_(tty_fd, query, 4) < 0) {
        return false;
    }

    // Wait for response with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tty_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms

    int ready = select(tty_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready <= 0) {
        return false;  // Timeout or error - no CSI u support
    }

    // Read response - format: ESC[?flags u
    char buf[32];
    ssize_t n = posix_read_(tty_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        return false;
    }
    buf[n] = '\0';

    // Check for ESC[? prefix and u suffix
    if (n >= 4 && buf[0] == '\x1b' && buf[1] == '[' && buf[2] == '?') {
        // Look for 'u' terminator
        for (ssize_t i = 3; i < n; i++) {
            if (buf[i] == 'u') {
                return true;
            }
        }
    }

    return false;
}
```

### Verify

1. `make check` - all tests pass
2. `make lint` - no issues
3. Manual test in supported terminal (Kitty/foot/Ghostty): verify init doesn't hang

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `ik_term_ctx_t` has `csi_u_supported` field
- Terminal init probes for CSI u and enables if supported
- Terminal cleanup disables CSI u if it was enabled

## Notes

- The probe uses a 100ms timeout which adds slight startup latency
- Terminals without CSI u support simply won't respond to the query
- WezTerm users need to add `config.enable_csi_u_key_encoding = true` to their config
- The enabled flag 9 works on Kitty, foot, Ghostty, and Alacritty 0.15+
