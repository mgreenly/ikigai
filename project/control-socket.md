# Control Socket

Unix domain socket interface for external programs to interact with ikigai programmatically.

## Purpose

Allow external programs (coding agents, ikigai itself, test harnesses) to drive ikigai like a user. The primary use case is tighter iterative loops: an agent makes code changes, runs the application, and inspects what's on screen — all programmatically.

## Protocol

- **Transport:** Unix domain socket (AF_UNIX, SOCK_STREAM)
- **Framing:** Newline-delimited JSON (one JSON object per line)
- **Pattern:** Stateless request/response (one request in, one response out)
- **Concurrency:** Single client. One connection at a time. The listen fd and client fd are both in the main `select()` set — no threading.

## Socket Path

`$IKIGAI_RUNTIME_DIR/ikigai-<pid>.sock`

### IKIGAI_RUNTIME_DIR

A new environment variable following the existing `IKIGAI_*_DIR` pattern.

| Context | Value |
|---------|-------|
| Development (.envrc) | `$PWD/run` |
| Production (wrapper script) | `${XDG_RUNTIME_DIR}/ikigai` |

`XDG_RUNTIME_DIR` is `/run/user/$UID` on systemd-based Debian systems. It is tmpfs, mode 0700, cleaned on logout. This is the correct location for ephemeral runtime files like sockets per XDG, FHS, and Debian policy.

Historical note: `/var/run` is now a compatibility symlink to `/run`.

### Wrapper Script Addition

All wrapper scripts gain one new line:

```bash
IKIGAI_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/ikigai
```

### Development .envrc Addition

```bash
export IKIGAI_RUNTIME_DIR=$PWD/run
```

### paths.c Addition

Add `runtime_dir` field to `ik_paths_t`, read from `IKIGAI_RUNTIME_DIR`.

## Socket Lifecycle

### Creation

Before `ik_repl_run()` enters the main loop:

1. `mkdir -p $IKIGAI_RUNTIME_DIR` (ensure directory exists)
2. `socket(AF_UNIX, SOCK_STREAM, 0)`
3. `bind()` to `$IKIGAI_RUNTIME_DIR/ikigai-<pid>.sock`
4. `listen()` with backlog of 1
5. Store listen fd on repl context

### Integration with Event Loop

The socket fds are added to the existing `select()` loop:

- **`ik_repl_setup_fd_sets()`** — add listen fd to read set; if a client is connected, add client fd to read set
- **Main loop dispatch** — after `select()` returns:
  - If listen fd is ready: `accept()` new connection (close existing client if any)
  - If client fd is ready: read request line, dispatch, write response line

### Destruction

After the main loop exits (when `repl->quit` is set):

1. `close()` client fd (if connected)
2. `close()` listen fd
3. `unlink()` socket file

## Messages

Message names mirror internal data structure names. The `read_` prefix indicates a query.

### read_framebuffer

Captures the final render buffer — exactly what is on screen at the moment the request is processed. If the spinner is animating, you see the spinner. If text is half-typed, you see the half-typed text.

**Request:**
```json
{"type": "read_framebuffer"}
```

**Response:**
```json
{
  "type": "framebuffer",
  "rows": 50,
  "cols": 120,
  "cursor": {"row": 45, "col": 12, "visible": true},
  "lines": [
    {
      "row": 0,
      "spans": [
        {"text": "Welcome to ", "style": {}},
        {"text": "ikigai", "style": {"fg": 153}}
      ]
    },
    {
      "row": 1,
      "spans": [
        {"text": "────────────", "style": {"dim": true}}
      ]
    },
    {
      "row": 2,
      "spans": [
        {"text": ""}
      ]
    }
  ]
}
```

**Span format:**
- `text` — plain text content (ANSI stripped)
- `style` — object with only present attributes (empty `{}` means unstyled):
  - `fg` — 256-color palette index (0-255)
  - `bg` — 256-color palette index (future)
  - `bold` — boolean
  - `dim` — boolean
  - `reverse` — boolean

**Line format:**
- Every screen row is represented (empty rows have a single span with empty text)
- `row` — 0-indexed screen row number
- `spans` — array of styled text runs, left to right

### send_keys (future)

```json
{"type": "send_keys", "keys": "hello world\n"}
```

```json
{"type": "ok"}
```

### Future Messages

Additional buffer reads will follow the same `read_` pattern:

```json
{"type": "read_scrollback"}
{"type": "read_input_buffer"}
{"type": "read_completion"}
```

## Implementation

### Data Source

The framebuffer is already captured in `ik_repl_render_frame()` under `IKIGAI_DEV`:

```c
repl->dev_framebuffer = talloc_memdup(repl, framebuffer, offset);
repl->dev_framebuffer_len = offset;
repl->dev_cursor_row = final_cursor_row;
repl->dev_cursor_col = final_cursor_col;
```

This save needs to happen unconditionally when the control socket is active (not gated on `IKIGAI_DEV`).

### Framebuffer Structure

The raw framebuffer byte array contains:

1. `\x1b[?25l` — hide cursor (6 bytes)
2. `\x1b[H` — home cursor (3 bytes)
3. **Layer content** — rendered text with embedded ANSI sequences
4. `\x1b[J` — clear to end of screen (3 bytes)
5. `\x1b[?25h/l` — cursor visibility (6 bytes)
6. `\x1b[row;colH` — cursor position (variable)

The serializer parses item 3 (layer content). Cursor position and visibility come from `dev_cursor_row`, `dev_cursor_col`, and `input_buffer_visible`.

### ANSI Vocabulary

The parser handles a closed set of escape sequences (all generated by ikigai's own code):

- `\x1b[38;5;Nm` — 256-color foreground
- `\x1b[0m` — reset
- `\x1b[2m` — dim
- `\x1b[1m` — bold
- `\x1b[7m` — reverse video
- `\r\n` — line breaks

### Serialization Approach

Parse on demand (option 1). No shadow screen buffer. The ANSI vocabulary is small and controlled — we own all the code that generates it. Parsing cost is negligible for a cold path (agent asks for screengrabs occasionally, not 60fps).

### File Structure

```
apps/ikigai/
├── control_socket.h              # Opaque type, init/destroy, fd set, dispatch
├── control_socket.c              # Socket lifecycle: bind/listen/accept/read/write/close
├── serialize.h                   # Common serialization types and helpers
├── serialize_framebuffer.c       # framebuffer -> JSON (ANSI parser + JSON builder)
├── serialize_scrollback.c        # (future)
├── serialize_input_buffer.c      # (future)
├── serialize_completion.c        # (future)
```

The serialization layer is separate from the socket transport. Each `serialize_*.c` knows how to take one internal data structure and produce JSON. The control socket dispatches by message type and calls the right serializer.

Serializers are independently unit-testable: feed a known byte array, assert on JSON output.

### Touch Points in Existing Code

- `paths.h` / `paths.c` — add `runtime_dir` field, read `IKIGAI_RUNTIME_DIR`
- `repl.h` — add control socket field to `ik_repl_ctx_t`
- `repl_event_handlers.c` (`ik_repl_setup_fd_sets()`) — add socket fds
- `repl.c` (`ik_repl_run()`) — init before loop, dispatch after select, cleanup after loop
- `repl_viewport.c` — remove `#ifdef IKIGAI_DEV` guard on framebuffer save (or gate on socket active)
- `.envrc` — add `IKIGAI_RUNTIME_DIR`
- `install-directories.md` — update wrapper script examples
