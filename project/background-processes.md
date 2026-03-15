# Background Processes — Design

## Overview

Allow an agent to run system processes without blocking its conversation thread. The agent starts a process, receives a handle, and continues working. It can check output, send input, close stdin, and kill the process at any time. ikigai watches the process, collects all output to disk, and notifies the agent when the process exits.

Background processes run in a pseudo-terminal (PTY), so programs behave exactly as they would in a user's terminal — colors, progress bars, interactive prompts, buffering — all identical. The user's mental model is simple: the agent is running my program.

## Motivation

Coding agents frequently need to:
- Start a build and continue editing while it runs
- Launch a dev server and interact with it over time
- Run a long test suite without blocking the conversation
- Start a REPL, send commands, read output, repeat
- Monitor a watcher process (e.g., `inotifywait`, `cargo watch`)

Without background processes, the agent blocks on every command. With them, the agent can orchestrate multiple concurrent activities — the way a human developer uses multiple terminal panes.

## Design Principles

1. **The program runs like the user runs it.** PTY-only — no pipe mode. Programs see a terminal, behave normally.
2. **No output is ever lost.** All output is persisted to disk from the start. The agent can inspect any line, any time, even days later.
3. **TTL is mandatory.** The LLM must always specify a time-to-live. No silent resource leaks from forgotten processes.
4. **User visibility is paramount.** The user can list, inspect, and kill any background process at any time via slash commands.
5. **Crash safety.** Orphaned processes are detected and cleaned up on startup.

## Prior Art

### What existing tools do

| Tool | Background? | Timeout | User inspection | Key weakness |
|------|------------|---------|-----------------|--------------|
| Claude Code | Yes (`run_in_background`) | None | `/bashes` (UI only) | Task handles orphaned on context compaction; no TTL; agent can't list its own processes |
| Gemini CLI | Yes (`is_background`) | Inactivity-based | Ctrl+B panel | Unlimited concurrent processes; no hard TTL |
| Codex CLI | Yes (unified exec) | 10s default | write/terminate/resize API | Head-tail buffer loses middle output |
| LangChain | Persistent shell (pexpect) | 10s expect timeout | None | Single session, no background concept |
| AutoGen | Per code block | 60s default | None | No persistent processes |
| Aider | No | User Ctrl+C | None | Fully synchronous |

### Lessons learned

**Claude Code's pain points are our requirements:**
- Task handles lost on context compaction → we persist process metadata in PostgreSQL
- No programmatic task listing → `ps` tool for the LLM
- No TTL → mandatory TTL parameter
- Crashed tasks report "running" → pidfd-based exit detection, startup recovery scan
- No stdin interaction → full stdin write + close-stdin support

**Codex CLI's head-tail buffer loses data.** We keep everything on disk.

**Gemini CLI uses PTY + xterm headless.** We use PTY too, but persist raw output to disk files rather than an in-memory terminal emulator.

## Architecture

### PTY Model

Every background process runs in its own PTY allocated via `forkpty()`. The master fd is the single bidirectional channel for all I/O.

```
┌─────────────┐         ┌──────────────────┐
│   ikigai    │         │  child process   │
│             │  PTY    │                  │
│  master_fd ◄──────────► stdin/stdout/err │
│             │ master  │    (slave)       │
└──────┬──────┘         └──────────────────┘
       │
       ▼
   disk file
  (raw output)
```

**Why PTY-only:**
- Programs behave identically to running in a user's terminal
- Colors, progress bars, line buffering — all correct by default
- Simpler than supporting both PTY and pipe modes (one code path, one test matrix)
- Merged stdout/stderr is inherent — no design decision needed

**PTY configuration:**
- Window size: 200 columns x 50 rows (generous default, avoids line wrapping in most tools)
- No echo suppression — the PTY line discipline handles it naturally
- `TERM=xterm-256color` in the child environment

### Disk-Backed Output

All process output is written to disk immediately. No ring buffers, no output caps, no data loss.

```
run/bg-processes/<process-id>/
├── output       # Raw PTY output (append-only)
└── metadata     # JSON: command, pid, status, timestamps
```

**Line index:** An in-memory array of byte offsets where each newline occurs. Enables O(1) random access to any line range without scanning the file.

```c
typedef struct {
    off_t *offsets;       // byte offset of each newline
    int64_t count;        // number of lines indexed
    int64_t capacity;     // allocated slots
} bg_line_index_t;
```

At ~8 bytes per line, indexing is cheap. A process producing 1 million lines of output costs ~8MB of index memory.

**Reading output:** `pread()` to the byte offset for the requested line range. No buffering, no copying — the OS page cache handles it.

**ANSI handling:** Output is stored raw (with escape codes). When serving output to the LLM via tools, ANSI escape sequences are stripped. When serving to the user via `/pread`, raw output is preserved (the terminal interprets it naturally).

### Process Lifecycle

```
         fork fails
STARTING ──────────► FAILED
    │
    │ forkpty succeeds
    ▼
 RUNNING ──┬──────► EXITED        (normal exit)
           │
           ├──────► KILLED        (user/agent sent kill)
           │
           └──────► TIMED_OUT     (TTL expired)
```

All terminal states are permanent. A process reaches exactly one terminal state.

### Per-Process State

```c
typedef struct {
    int32_t          id;              // monotonic, never reused
    pid_t            pid;             // OS pid
    int              master_fd;       // PTY master (read + write)
    int              pidfd;           // pollable, fires on exit (pidfd_open)
    int              output_fd;       // disk file fd (append writes)

    bg_status_t      status;          // STARTING, RUNNING, EXITED, KILLED, TIMED_OUT, FAILED
    int              exit_code;       // valid when EXITED
    int              exit_signal;     // valid when KILLED

    char            *command;         // shell command string
    char            *label;          // human-readable description
    int32_t          agent_id;        // owning agent

    int32_t          ttl_seconds;     // -1 = forever
    struct timespec  started_at;
    struct timespec  exited_at;

    bg_line_index_t  line_index;      // in-memory newline offsets
    int64_t          total_bytes;     // total output written to disk
    int64_t          cursor;          // last byte offset read by agent (for "since last" queries)

    bool             stdin_open;      // false after pclose
} bg_process_t;
```

### Event Loop Integration

The process manager integrates with ikigai's event loop via epoll:

```
┌────────────────────────────────────────────────┐
│                 epoll instance                  │
│                                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ master_fd│  │  pidfd   │  │  timerfd     │ │
│  │ (per     │  │ (per     │  │ (1s tick for │ │
│  │ process) │  │ process) │  │  TTL check)  │ │
│  └──────────┘  └──────────┘  └──────────────┘ │
└────────────────────────────────────────────────┘
```

**master_fd readable:** Read output from PTY, append to disk file, update line index.

**pidfd readable:** Process exited. Call `waitpid(pid, WNOHANG)` to collect status. Transition to EXITED/KILLED. Continue draining master_fd until EOF (output may arrive after exit).

**timerfd fires (1s):** Walk all RUNNING processes, check TTL:
- At 100% of TTL: send `\x04` (EOF) to master_fd, then `kill(pid, SIGTERM)` to process. After 5 seconds, escalate to `SIGKILL`. Mark TIMED_OUT. Send exit message to owning agent.

**Critical:** The reader must drain master_fd continuously, even when nobody is inspecting output. If the PTY buffer fills (~64KB on Linux), the child process blocks on write. The event loop prevents this by reading eagerly and appending to disk.

### Child Process Setup

```c
int master_fd;
struct winsize ws = { .ws_row = 50, .ws_col = 200 };
pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);

if (pid == 0) {
    // Child
    setpgid(0, 0);                      // new process group
    prctl(PR_SET_PDEATHSIG, SIGTERM);    // die if parent dies

    // Verify parent is still alive (race window)
    if (getppid() != expected_parent_pid)
        _exit(1);

    setenv("TERM", "xterm-256color", 1);
    execl("/bin/sh", "sh", "-c", command, NULL);
    _exit(127);
}

// Parent
int pidfd = pidfd_open(pid, 0);
fcntl(master_fd, F_SETFL, O_NONBLOCK);
// Register master_fd and pidfd with epoll
// Open output file for appending
```

**Safety mechanisms:**
- `setpgid(0, 0)` — child gets its own process group for clean subtree kills
- `prctl(PR_SET_PDEATHSIG, SIGTERM)` — child dies if ikigai crashes
- `pidfd_open()` — race-free, pollable exit notification (Linux 5.3+)
- Non-blocking master_fd — prevents reader from blocking

### stdin Management

Writing input to a PTY process:

```c
// Send input
write(proc->master_fd, input, len);    // PTY echoes it back in output

// Close stdin (signal EOF)
// On a PTY, write the EOF character rather than closing the fd
// Closing master_fd would terminate the entire PTY session
char eof = '\x04';  // Ctrl-D
write(proc->master_fd, &eof, 1);
proc->stdin_open = false;
```

**PTY stdin considerations:**
- The PTY line discipline echoes input back into the output stream. This is expected — it matches what a user sees in a terminal.
- `\x04` (Ctrl-D) signals EOF to the child's stdin. The master_fd remains open for reading output.
- Writes are atomic up to `PIPE_BUF` (4096 bytes). Larger writes need serialization.
- If the process has already exited, `write()` returns `EIO`. Report "process has already exited."

### Process Cleanup

**Voluntary kill (user or agent):**

```c
void bg_process_kill(bg_process_t *proc) {
    kill(-proc->pid, SIGTERM);           // signal entire process group
    // After 5 seconds if still alive:
    kill(-proc->pid, SIGKILL);
    // waitpid reaps, pidfd fires, status transitions
}
```

**Session end:** When an agent session ends, SIGTERM all its RUNNING processes. Wait 5 seconds, SIGKILL survivors. Mark all as KILLED in the database.

**Startup recovery scan:** On ikigai startup, query `background_processes` for rows with status `running` or `starting`. For each:
1. Check if pid is still alive: `kill(pid, 0)`
2. If alive: kill it (orphan from a crashed session), update status to KILLED
3. If dead: update status to EXITED with unknown exit code

### Concurrency Limit

**20 concurrent processes per agent.**

The limit applies to RUNNING + STARTING processes. Terminal-state processes don't count. When the limit is hit, the start tool returns a clear error: "Cannot start process: 20/20 slots in use. Kill an existing process first."

## Database Schema

```sql
CREATE TYPE bg_process_status AS ENUM (
    'starting', 'running', 'exited', 'killed', 'timed_out', 'failed'
);

CREATE TABLE background_processes (
    id              SERIAL PRIMARY KEY,
    agent_id        INTEGER NOT NULL REFERENCES agents(id),
    pid             INTEGER,
    command         TEXT NOT NULL,
    label           TEXT NOT NULL,
    status          bg_process_status NOT NULL DEFAULT 'starting',
    exit_code       INTEGER,
    exit_signal     INTEGER,
    ttl_seconds     INTEGER NOT NULL,       -- -1 = forever
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    started_at      TIMESTAMPTZ,
    exited_at       TIMESTAMPTZ,
    total_bytes     BIGINT NOT NULL DEFAULT 0,
    output_path     TEXT                    -- path to disk file
);

CREATE INDEX idx_bg_proc_agent ON background_processes(agent_id);
CREATE INDEX idx_bg_proc_active ON background_processes(status)
    WHERE status IN ('starting', 'running');
```

The database stores metadata only. Output lives on disk. The DB enables:
- Persistence across restarts (startup recovery scan)
- Audit trail (what processes ran, when, how they ended)
- Cross-agent visibility (parent sees child agent processes)

## User Slash Commands

All process commands use the `p` prefix (p = process), consistent with Unix convention (`pgrep`, `pkill`, `pstree`).

### `/ps` — List background processes

```
ID  PID    STATUS       AGE      TTL LEFT  OUTPUT     COMMAND
1   48210  running      3m22s    6m38s     14.2KB     make -j8 check-unit
2   48315  running      1m05s    forever   2.1KB      python -m http.server
3   48102  exited(0)    5m10s    —         98.7KB     ./run-integration-tests.sh
4   48400  timed_out    10m00s   —         1.2MB      cargo watch -x test
```

Columns: ID (for referencing), PID (for debugging), status (with exit code), wall-clock age, remaining TTL (`forever` for ttl=-1, `—` for terminal states), total output size, truncated command.

### `/pread <id>` — Read process output

Options:
- `/pread 1` — last 50 lines (default)
- `/pread 1 --tail=100` — last 100 lines
- `/pread 1 --lines=500-550` — specific line range
- `/pread 1 --since-last` — output since last read
- `/pread 1 --full` — everything (warning if large)

Header shows: status, exit code (if finished), total lines, total bytes, age.

### `/pkill <id>` — Terminate a process

Sends SIGTERM to the process group. Escalates to SIGKILL after 5 seconds. Reports final status.

### `/pwrite <id> <text>` — Write to stdin

Sends text followed by a newline. Options:
- `/pwrite 1 SELECT * FROM users;` — send line
- `/pwrite 1 --raw \x1b[A` — send raw bytes, no newline
- `/pwrite 1 --eof` — send text then close stdin

### `/pclose <id>` — Signal EOF

Writes `\x04` (Ctrl-D) to the PTY. The process sees EOF on stdin. Output continues flowing.

## LLM Tools

Five internal tools registered in the tool registry, using the same `p` prefix as slash commands. The LLM sees them alongside file_read, bash, etc.

### `pstart`

Start a background process.

```json
{
    "name": "pstart",
    "description": "Start a long-running process in the background. The process runs in a terminal (PTY) and all output is preserved. You must specify a TTL.",
    "parameters": {
        "type": "object",
        "properties": {
            "command": {
                "type": "string",
                "description": "Shell command to run"
            },
            "label": {
                "type": "string",
                "description": "Short human-readable description (e.g. 'unit tests', 'dev server')"
            },
            "ttl_seconds": {
                "type": "integer",
                "description": "Time-to-live in seconds. The process is killed when TTL expires. Use -1 for no limit."
            }
        },
        "required": ["command", "label", "ttl_seconds"]
    }
}
```

Returns:
```json
{
    "id": 1,
    "pid": 48210,
    "status": "running"
}
```

### `pread`

Read output and status of a background process.

```json
{
    "name": "pread",
    "description": "Check the status and read output of a background process.",
    "parameters": {
        "type": "object",
        "properties": {
            "id": {
                "type": "integer",
                "description": "Process ID from pstart"
            },
            "mode": {
                "type": "string",
                "enum": ["tail", "since_last", "lines"],
                "description": "Output mode. 'tail' = last N lines, 'since_last' = new output since last check, 'lines' = specific range. Default: tail"
            },
            "tail_lines": {
                "type": "integer",
                "description": "Number of lines for tail mode. Default: 50"
            },
            "start_line": {
                "type": "integer",
                "description": "Start line for lines mode (1-indexed)"
            },
            "end_line": {
                "type": "integer",
                "description": "End line for lines mode (1-indexed, inclusive)"
            }
        },
        "required": ["id"]
    }
}
```

Returns:
```json
{
    "id": 1,
    "status": "running",
    "age_seconds": 202,
    "ttl_remaining_seconds": 398,
    "total_lines": 847,
    "total_bytes": 145920,
    "output": "... ANSI-stripped output lines ..."
}
```

Output is capped at 200 lines or 50KB per tool result (whichever is smaller) to prevent context flooding. If truncated, a header indicates: `[showing lines 798-847 of 847]`.

### `pwrite`

Send input to a background process.

```json
{
    "name": "pwrite",
    "description": "Write to a background process's stdin. Appends a newline by default.",
    "parameters": {
        "type": "object",
        "properties": {
            "id": {
                "type": "integer",
                "description": "Process ID from pstart"
            },
            "input": {
                "type": "string",
                "description": "Text to send to the process"
            },
            "close_stdin": {
                "type": "boolean",
                "description": "If true, close stdin after sending (signal EOF). Default: false"
            }
        },
        "required": ["id", "input"]
    }
}
```

### `pkill`

Kill a background process.

```json
{
    "name": "pkill",
    "description": "Terminate a background process. Sends SIGTERM, escalates to SIGKILL after 5 seconds.",
    "parameters": {
        "type": "object",
        "properties": {
            "id": {
                "type": "integer",
                "description": "Process ID from pstart"
            }
        },
        "required": ["id"]
    }
}
```

### `ps`

List all background processes for the current agent.

```json
{
    "name": "ps",
    "description": "List all background processes owned by the current agent.",
    "parameters": {
        "type": "object",
        "properties": {},
        "required": []
    }
}
```

Returns:
```json
[
    {
        "id": 1,
        "pid": 48210,
        "label": "unit tests",
        "status": "running",
        "age_seconds": 202,
        "ttl_remaining_seconds": 398,
        "total_lines": 847,
        "total_bytes": 145920,
        "command": "make -j8 check-unit"
    }
]
```

## Process Exit Messages

When a background process reaches a terminal state (exited, killed, timed out), it sends a message to its owning agent through the existing agent message-passing system. This reuses the infrastructure that child agents already use to communicate with parents.

### Exit message

```json
{
    "from": "process:3",
    "type": "process_exit",
    "label": "unit tests",
    "status": "exited",
    "exit_code": 1,
    "age_seconds": 45,
    "total_lines": 847,
    "tail": "... last 20 lines ..."
}
```

The message includes a 20-line tail so the LLM often has enough context without a follow-up `pread` call.

### LLM workflow patterns

The LLM chooses how to handle background processes:

- **Block on completion**: `pstart` → `/wait` → receive exit message → act on result
- **Poll when ready**: `pstart` → do other work → `pread` later → discard stale exit message
- **Opportunistic**: `pstart` → do other work → notice exit message between turns

The exit message is informational, not a gate. If the LLM already handled the result via `pread`, the message is stale and can be discarded.

### No TTL warnings

The LLM chose the TTL — it knows the clock is ticking. No warnings are sent at 80% or any other threshold. If the LLM wants to monitor TTL, it calls `pread` or `ps` to check `ttl_remaining_seconds`.

## Process Ownership

- Each process is owned by the agent that started it (`agent_id` in the database)
- A parent agent can see processes started by its child agents (query by agent subtree)
- When an agent session ends, all its RUNNING processes are terminated (SIGTERM → 5s → SIGKILL)
- Process IDs are scoped per-agent — agent A's process #1 is distinct from agent B's process #1

## Safety Summary

| Threat | Mitigation |
|--------|-----------|
| Forgotten process | Mandatory TTL, `/ps` visibility, exit message to agent |
| Resource exhaustion | 20 process concurrency limit per agent |
| Orphaned process (parent crash) | `PR_SET_PDEATHSIG(SIGTERM)` in child; startup recovery scan |
| PID reuse race | `pidfd_open()` for race-free process identity |
| PTY buffer full | Continuous reader drains master_fd to disk |
| Output floods memory | Output goes to disk, only line index in memory |
| Agent session ends | All owned processes terminated with SIGTERM/SIGKILL |
| Zombie processes | `waitpid(WNOHANG)` on pidfd notification |
| Process forks children | Process group kill via `kill(-pgid, sig)` |
| Child daemonizes (escapes group) | Documented limitation; cgroups for v2 |

## Code Structure

### New files

| File | Purpose |
|------|---------|
| `bg_process.c/h` | Process manager: start, kill, read output, send input |
| `bg_reader.c/h` | Event loop integration: epoll, master_fd draining, pidfd handling |
| `bg_line_index.c/h` | Line index: newline offset tracking, range queries |
| `bg_ansi.c/h` | ANSI escape sequence stripping for LLM output |

### Internal tools

Background process tools are internal (C functions called in-process), not external executables:

| Tool | Purpose |
|------|---------|
| `pstart` | Start background process |
| `pread` | Check status and read output |
| `pwrite` | Write to stdin |
| `pkill` | Terminate process |
| `ps` | List processes |

### Integration points

| File | Change |
|------|--------|
| `agent.h` | Add `bg_manager_t *bg_manager` field |
| `agent.c` | Initialize/destroy bg_manager with agent lifecycle |
| `repl.c` | Register bg_manager's epoll fds with main event loop |
| `message.c` | Send exit messages to owning agent via existing message system |
| `slash_commands.c` | Register `/ps`, `/pread`, `/pkill`, `/pwrite`, `/pclose` |
| `schema.sql` | Add `background_processes` table |

### Internal vs external tool decision

Background process tools could be either internal (C functions) or external (separate executables). **Internal** is the right choice because:
- They need direct access to the bg_manager's in-memory state (line index, master_fd, process table)
- They modify shared process state (cursor position, stdin pipe)
- The fork/exec overhead of external tools is acceptable for most tools but wasteful for pread, which may be called frequently
- They follow the same pattern as agent operations (fork, kill, send, wait) which are already internal tools

## Relationship to rel-14

This feature extends rel-14 (Parallel Tool Execution) with a new dimension of concurrency. Parallel tools execute within a single LLM response turn and complete before the next turn. Background processes span multiple turns — they run across the conversation, checked and managed asynchronously.

The parallel tool scheduler and background process manager are independent systems. They share the agent's event loop but don't interact directly. A pstart tool call within a parallel batch starts the process and returns immediately (the tool result is just the process ID and status), allowing other parallel tools to proceed.

## Industry Context

The design addresses known weaknesses across existing implementations:

- **Claude Code** loses task handles on context compaction and has no TTL. We persist metadata in PostgreSQL and require TTL.
- **Codex CLI** uses a head-tail buffer that loses middle output. We persist everything to disk.
- **Gemini CLI** uses PTY (validated our approach) but has no hard TTL or concurrency limits.
- **No existing tool** provides random-access line-range queries into process output. Our line index enables this.
- **No existing tool** persists background process metadata to a database for crash recovery.

The combination of PTY execution, full disk-backed output with line indexing, mandatory TTL, database-backed crash recovery, and rich user inspection commands is unique in the space.

## Implementation Decisions

Decisions made during codebase analysis, recorded for implementors.

### 1. select() over epoll

The design doc references epoll, but the codebase uses `select()` exclusively — the main event loop (`repl.c`), curl_multi integration, control socket, and terminal input all use `fd_set`. Worst case with 20 background processes is ~45 fds, well within `FD_SETSIZE` (1024).

**Decision:** Stay with `select()`. No timerfd. Migration to epoll is a standalone future goal with no feature coupling.

### 2. No timerfd — piggyback on existing timeout infrastructure

TTL checks need ~1s resolution. The existing `ik_repl_calculate_select_timeout_ms()` already combines multiple timeout sources (spinner 80ms, curl, tool poll 50ms, scroll detector) and picks the minimum.

**Decision:** Add a bg process timeout source — cap select timeout at 1000ms when RUNNING processes exist. After select() returns, walk RUNNING processes and check `started_at + ttl_seconds` against `CLOCK_MONOTONIC`. Same pattern as spinner and tool polling — no timerfd needed.

### 3. Agent UUID for ownership, global SERIAL for process IDs

The codebase uses UUID (TEXT) as the primary key for agents everywhere. The design doc's `agent_id INTEGER` is incorrect for this codebase.

Process IDs use `SERIAL PRIMARY KEY` — globally unique, monotonic, never reused. This is the handle the LLM and user reference (`pread 3`, `/pkill 3`). The design doc's "process IDs scoped per-agent" adds complexity for no benefit — a global SERIAL already guarantees uniqueness.

**Decision:** `agent_uuid TEXT NOT NULL REFERENCES agents(uuid) ON DELETE CASCADE` in the schema. `char *agent_uuid` in the C struct. Global SERIAL for process IDs.

### 4. Hybrid ownership — per-agent manager, flat fd list on repl

Two concerns pull in opposite directions: agent encapsulation (talloc ownership, scoped operations, concurrency limits) wants per-agent managers, while the single select() loop wants a flat fd list.

**Decision:** Both. Each agent owns a `bg_manager_t` — talloc destructor handles cleanup, concurrency limit is `manager->count`, `ps` iterates its own array. The repl maintains a flat fd list for the select() hot path. When a process starts or exits (always on the main thread via on_complete hooks or select() handlers), both structures are updated atomically. No data race risk because all mutations happen on the single main thread — same reason agent arrays, curl handles, and control socket state need no synchronization.

### 5. New MOCKABLE wrappers as needed

Testability requires MOCKABLE wrappers for system calls used by background processes: `forkpty()`, `pidfd_open()`, `waitpid()`, `kill()`, and others as they arise during implementation.

**Decision:** Add to `shared/wrapper_posix.h` following the existing pattern (`posix_open_()`, `posix_read_()`, etc.).

### 6. File structure respects 16KB limit

The design doc's four file pairs (`bg_process`, `bg_reader`, `bg_line_index`, `bg_ansi`) are the right decomposition. Internal tool handlers go in `internal_tools_bg.c` (all five in one file unless individual handlers grow large, then split to `internal_tool_pstart.c` etc. following the `internal_tool_fork.c` pattern).

### 7. Process exit messages via mail system

The design doc says exit messages reuse "the infrastructure that child agents already use to communicate with parents." That infrastructure is the mail system (`mail` table, `commands_send.c`). Process exit messages are sent as mail from `"process:<id>"` to the owning agent's UUID.
