# Headless Mode

## Motivation

Black box functional testing requires running ikigai end-to-end without a real terminal. Today the app crashes without a TTY because `ik_term_init()` opens `/dev/tty` and calls `tcgetattr`/`tcsetattr`/`ioctl` unconditionally. Headless mode removes the terminal dependency so test harnesses and agents can drive ikigai programmatically.

## Testing Architecture

Three components work together:

1. **`--headless` flag** — ikigai runs normally but never touches a real terminal. Render buffer still filled, event loop still running, database still written to.
2. **`ikigai-ctl`** — test harness sends commands and queries state through the control socket. Control socket is a hard requirement in headless mode (no TTY means no input source without it).
3. **Assertions** — tests check render buffer contents (via `serialize_framebuffer.c`) and/or database state.

No PTY harness, no tmux, no screen scraping. The control socket is the programmatic interface; headless just removes the TTY dependency.

## Default Dimensions

Headless mode uses **100x50** (cols x rows) by default, reflecting realistic terminal usage.

## Current Terminal Architecture

Terminal I/O is already well-centralized:

- **`shared/terminal.c`** — single module for all terminal init/cleanup
- **`shared/wrapper_posix.c`** — all POSIX calls (`tcgetattr`, `tcsetattr`, `ioctl`, `write`, `read`) wrapped and mockable
- **`repl_viewport.c:336`** — primary flush point where framebuffer is written to TTY fd via `posix_write_()`
- **`render.c:119` and `render.c:300`** — legacy flush points (`ik_render_input_buffer` and `ik_render_combined`), used as fallback when layer cake is not initialized
- **`layer_*.c` files** — append escape sequences into `ik_output_buffer`, never touch TTY directly

## Capability Queries

Two kinds of terminal capability queries exist today:

### CSI u Probe (one-time, during init)

- `probe_csi_u_support()` writes `ESC[?u`, waits 100ms for response, parses it
- `enable_csi_u()` sends enable command, reads confirmation
- Called once in `ik_term_init_with_fd()` (lines 223-231)
- **Headless behavior:** skip entirely, set `csi_u_supported = false`

### Terminal Size (init + ongoing)

- `posix_ioctl_(tty_fd, TIOCGWINSZ, &ws)` called during init (line 235)
- Same call in `ik_term_get_size()` (line 285) on SIGWINCH (window resize)
- **Headless behavior:** return stored 100x50 without calling `ioctl`

---

## Implementation Steps

Each step is independently verifiable. Steps 1-2 are zero-risk refactors with no behavior change. Step 3 is the headless feature. Step 4 builds the test infrastructure on top.

### Step 1: Remove Dev Dump

Replace the `#ifdef IKIGAI_DEV` per-frame framebuffer copy with direct access to the current framebuffer. This is a prerequisite — it removes the build-flag gating that would block headless testing.

**Changes:**

- Remove the per-frame `talloc_memdup` into `repl->dev_framebuffer` and the `#ifdef IKIGAI_DEV` guard around it (`repl_viewport.c:325-333`)
- Remove `dev_framebuffer`, `dev_framebuffer_len`, `dev_cursor_row`, `dev_cursor_col` fields from the repl struct
- Point `read_framebuffer` in `control_socket.c` at the actual current framebuffer rather than the dev dump copy
- Remove the `#ifdef IKIGAI_DEV` guard from the `read_framebuffer` handler in `control_socket.c`
- `read_framebuffer` always returns the current framebuffer — no build flags, no special configuration
- Works identically in debug and release builds

**Files:**

| File | Change |
|------|--------|
| `apps/ikigai/repl_viewport.c` | Remove dev dump `talloc_memdup` and `#ifdef IKIGAI_DEV` |
| `apps/ikigai/control_socket.c` | Remove `#ifdef IKIGAI_DEV` from `read_framebuffer`, point at actual framebuffer |
| Repl struct header | Remove `dev_framebuffer`, `dev_framebuffer_len`, `dev_cursor_row`, `dev_cursor_col` |

**Verify:** All existing tests pass. `ikigai-ctl read_framebuffer` still works on a running instance.

---

### Step 2: Add `tty_fd >= 0` Guards

Add guards to all code paths that touch the terminal fd. No behavior change when `tty_fd` is valid — these guards only activate when `tty_fd == -1`.

**Guards:**

- **`ik_term_get_size()`** (`shared/terminal.c`) — if `tty_fd == -1`, return stored values without calling `ioctl`. Resize (SIGWINCH) is a no-op.
- **`ik_term_cleanup()`** (`shared/terminal.c`) — if `tty_fd == -1`, skip all restoration (no termios to restore, no alternate buffer to exit, no fd to close).
- **`repl_viewport.c` flush** — if `tty_fd == -1`, skip the `posix_write_()` call. The framebuffer is still built and available for inspection via `ikigai-ctl`.
- **`render.c` legacy flush points** — guard `posix_write_()` at lines 119 and 300 with `tty_fd >= 0`. These are fallback paths used when layer cake is not initialized.
- **`repl_event_handlers.c:97` FD_SET** — if `tty_fd == -1`, skip `FD_SET(terminal_fd, read_fds)`. `FD_SET` with -1 is undefined behavior per POSIX. In headless mode, `max_fd` comes from control socket and provider fds instead.
- **`repl.c:149` FD_ISSET** — guard terminal input check with `tty_fd >= 0`. Without the guard, `FD_ISSET(-1, ...)` naturally returns false but is semantically incorrect.
- **`panic.c:110,115`** — guard `write()` and `tcsetattr()` calls with `tty_fd >= 0`. These use raw syscalls (not wrappers) and don't check the fd.

**Files:**

| File | Change |
|------|--------|
| `shared/terminal.c` | Guard `ik_term_get_size()` and `ik_term_cleanup()` |
| `apps/ikigai/repl_viewport.c` | Guard framebuffer flush on `tty_fd >= 0` |
| `apps/ikigai/render.c` | Guard legacy flush points at lines 119 and 300 |
| `apps/ikigai/repl_event_handlers.c` | Guard `FD_SET` on `tty_fd >= 0` |
| `apps/ikigai/repl.c` | Guard `FD_ISSET` on `tty_fd >= 0` |
| `shared/panic.c` | Guard terminal restore on `tty_fd >= 0` |

**Verify:** All existing tests pass. No behavior change — guards only activate for `tty_fd == -1` which no production path produces yet.

---

### Step 3: Headless Mode

The actual feature: `--headless` flag and `ik_term_init_headless()`.

**New function: `ik_term_init_headless()`**

Add to `shared/terminal.c`. Infallible — no system calls, no I/O. Only `talloc_zero` which PANICs on OOM (consistent with codebase pattern).

**Signature:** `ik_term_ctx_t *ik_term_init_headless(TALLOC_CTX *ctx)`

Creates an `ik_term_ctx_t` with canned values:

- `tty_fd = -1` (sentinel: no real fd)
- `screen_rows = 50`
- `screen_cols = 100`
- `csi_u_supported = false`
- No system calls, no `/dev/tty`, no raw mode, no alternate buffer

The `tty_fd == -1` sentinel is already proven in tests (`repl_combined_render_test.c:62,116` sets `tty_fd = -1`).

**Flag parsing in `main.c`**

Parse `--headless` and call `ik_term_init_headless()` instead of `ik_term_init()`. The app currently does not parse argv — this is the first CLI flag.

**Files:**

| File | Change |
|------|--------|
| `shared/terminal.h` | Declare `ik_term_init_headless()` |
| `shared/terminal.c` | Implement `ik_term_init_headless()` |
| `apps/ikigai/main.c` | Parse `--headless`, route to headless init |

**Verify:** `ikigai --headless` starts without crashing. Control socket appears. `ikigai-ctl read_framebuffer` returns a frame. `ikigai-ctl send_keys` injects input.

---

### Step 4: Functional Test Infrastructure

Build the test harness and helpers that let C test binaries drive headless ikigai.

**New test helpers:**

- **Headless instance lifecycle** (`tests/helpers/headless_instance_helper.c`) — spawn ikigai `--headless`, wait for control socket to appear, cleanup on teardown
- **Control socket C client** (`apps/ikigai/control_socket_client.c`) — C library for `send_keys` and `read_framebuffer` (the bash `ikigai-ctl` wrapper depends on `socat`/`jq`, not suitable for C test binaries)
- **Framebuffer inspection** (`tests/helpers/framebuffer_inspect_helper.c`) — search/extract text from serialized framebuffer JSON

**New harness:**

- **`check-functional` Makefile target** (`.make/check-functional.mk`) — follows existing pattern from `check-unit.mk` and `check-integration.mk`
- **Harness script** (`.claude/harness/functional/run`) — Ruby, follows existing pattern

**Test directory:**

`tests/functional/apps/ikigai/` — functional test binaries using libcheck, same conventions as unit/integration tests.

**Existing infrastructure leveraged:**

- libcheck test framework
- Database isolation per test file (`test_utils_db_helper.c`)
- VCR HTTP mocking for AI providers (`tests/helpers/vcr_helper.c`)
- POSIX wrapper weak symbols for fault injection
- Makefile parallel execution with XML parsing and emoji status

**Verify:** A basic functional test starts headless ikigai, sends keys via control socket, reads framebuffer, and asserts on content.

---

## Reference

### What Still Runs in Headless

- Event loop
- AI provider communication
- PostgreSQL persistence
- Render buffer construction (all `layer_*.c` rendering)
- Framebuffer serialization (`serialize_framebuffer.c`)
- Control socket (`ikigai-ctl` interface) — **required** in headless mode
- Input processing (commands arrive via `ikigai-ctl send_keys` instead of keyboard)
- Key injection buffer (event loop drains it before checking TTY, already works)

### What Is Skipped in Headless

- Opening `/dev/tty`
- Raw mode (`tcgetattr`/`tcsetattr`)
- Alternate screen buffer enter/exit
- CSI u probe and enable
- Terminal size queries via `ioctl`
- Framebuffer flush to TTY (all three flush points: `repl_viewport.c:336`, `render.c:119`, `render.c:300`)
- SIGWINCH handling (no terminal to resize)
