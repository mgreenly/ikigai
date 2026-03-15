# Parallel Tool Execution — Design

## Overview

Parallelize tool call execution within a single LLM response turn. Tool calls are scheduled based on resource conflict detection, maximizing concurrency while preserving correctness. Tool calls begin executing as they arrive from the streaming response — the scheduler builds incrementally and advances on every state change.

## Queue Model

Tool calls enter an ordered queue as each `content_block_stop` event fires during response streaming. Arrival order is the total order. Dependencies only point backward (no cycles, no deadlock possible).

### States

- **queued** — waiting, has unresolved blockers
- **running** — executing in a pthread worker
- **completed** — finished successfully, has result
- **errored** — failed, has error message
- **skipped** — cascading failure from a blocker that errored or was skipped

### State Transitions

```
queued → running → completed
                 → errored → cascades skipped to dependents
skipped (terminal, also cascades to dependents)
```

All of completed, errored, and skipped are terminal states.

## Access Classification

Each tool call has an access descriptor derived from the tool name and its input arguments:

```c
typedef struct {
    ik_access_mode_t read_mode;   // NONE, PATHS, WILDCARD
    ik_access_mode_t write_mode;  // NONE, PATHS, WILDCARD
    const char **read_paths;      // NULL when NONE or WILDCARD
    int32_t read_path_count;
    const char **write_paths;     // NULL when NONE or WILDCARD
    int32_t write_path_count;
} ik_access_t;
```

### Tool Classifications

| Tool | reads | writes | Notes |
|------|-------|--------|-------|
| file_read | [file_path arg] | [] | Path from `file_path` arg |
| file_edit | [file_path arg] | [file_path arg] | Reads then modifies |
| file_write | [] | [file_path arg] | Full overwrite, no read needed |
| glob | * | [] | Scans filesystem |
| grep | * | [] | Scans filesystem |
| list | * | [] | Lists directories |
| bash | * | * | Unknown effects, full barrier |
| web_fetch | [] | [] | Remote only, no local resources |
| web_search | [] | [] | Remote only, no local resources |
| mem (get/list/revisions/revision_get) | [mem:path arg] | [] | Remote API read ops |
| mem (create/update/delete) | [] | [mem:path arg] | Remote API write ops |
| internal tools (fork/kill/send/wait/skill ops) | Not parallelized | | Inherently sequential — outputs feed next call's inputs |

### Classification is argument-aware

Most tools have static classification based on tool name alone. Exceptions:
- **mem** — inspects the `action` argument to distinguish read vs write operations
- Path is extracted from tool-specific input fields (`file_path`, `path`, etc.)

### Resource namespaces

File paths and mem paths are separate namespaces. A mem write to "notes" does not conflict with a file_read of "/foo/notes". Mem paths are prefixed with `mem:` internally for conflict detection.

## Conflict Rules

For each incoming tool call, scan backward through all pending (non-terminal) items. A conflict exists when:

| Incoming \ Pending | read(path) | read(*) | write(path) | write(*) |
|---|---|---|---|---|
| **read(path)** | no conflict | no conflict | conflict if same path | conflict |
| **read(*)** | no conflict | no conflict | conflict | conflict |
| **write(path)** | conflict if same path | conflict | conflict if same path | conflict |
| **write(*)** | conflict | conflict | conflict | conflict |

Items with `reads: [], writes: []` (web_fetch, web_search, mem reads) never conflict with anything.

## Execution

- No concurrency cap — the conflict graph is the natural throttle
- Two triggers promote queued items to running:
  1. **New tool call arrives** from the stream — start immediately if no blockers
  2. **Running tool completes** (or errors) — promote anything newly unblocked
- Both triggers run the same promotion logic: walk the queue, find queued entries whose blockers are all terminal, spawn worker threads
- Bash acts as a full barrier: blocked by everything pending before it, blocks everything after it

## Error Cascading

When a tool call enters errored or skipped state:
1. All items blocked by it become **skipped**
2. Skipped items cascade identically — anything blocked by a skipped item is also skipped
3. The cascade walks forward through the entire dependency chain

### Synthesized error message for skipped tools

Skipped tools still require a tool_result (API constraint). The result contains:

```
Skipped: a prerequisite tool call failed. Tool call [id] for [tool_name] returned: [error message]
```

This gives the model the failed tool's identity and error so it can decide whether to retry.

## Result Delivery

All tool results are held until every tool call in the batch reaches a terminal state (completed, errored, or skipped) AND the stream is complete (`message_stop` received). Results are then sent back in a single user message per API requirements.

Results are matched by ID, not position — they can be sent in any order.

## Display

Sequential status lines appended to the scrollback as state transitions occur. No fancy multi-line live display — just explicit text:

```
Queued: file_read(/src/main.c)
Running: file_read(/src/main.c)
Running: file_read(/src/util.c)
Blocked: file_edit(/src/parser.c) — waiting on file_read(/src/main.c)
Completed: file_read(/src/util.c)
Completed: file_read(/src/main.c)
Running: file_edit(/src/parser.c)
Completed: file_edit(/src/parser.c)
Errored: bash(make test) — exit code 1
Skipped: file_read(/src/output.log) — prerequisite failed: bash(make test)
```

Every state transition is visible. The user sees what's running, what's waiting and why, and what finished.

## API Constraints (all providers)

- Every tool_use block must have a corresponding tool_result in the next user message
- No messages can be inserted between the assistant's tool_use message and the user's tool_result message
- The count must be exactly 1:1

## Code Structure

### New file: `tool_scheduler.c/h`

Core data structures:

```c
typedef struct {
    ik_tool_call_t *tool_call;
    ik_schedule_status_t status;
    ik_access_t access;
    int32_t *blocked_by;
    int32_t blocked_by_count;
    pthread_t thread;
    pthread_mutex_t mutex;
    pid_t child_pid;
    char *result;
    char *error;
} ik_schedule_entry_t;

typedef struct {
    ik_schedule_entry_t *entries;
    int32_t count;
    int32_t capacity;
    bool stream_complete;
    pthread_mutex_t mutex;
} ik_tool_scheduler_t;
```

Key functions:
- `ik_tool_scheduler_create()` / `_destroy()`
- `ik_tool_scheduler_add(scheduler, tool_call)` — classify, detect conflicts, start if unblocked
- `ik_tool_scheduler_on_complete(scheduler, index, result)` — mark completed, promote
- `ik_tool_scheduler_on_error(scheduler, index, error)` — mark errored, cascade skips, promote
- `ik_tool_scheduler_promote(scheduler)` — find and start unblocked entries
- `ik_tool_scheduler_poll(scheduler)` — check running entries for thread completion
- `ik_tool_scheduler_all_terminal(scheduler)` — batch complete check
- `ik_tool_scheduler_classify(tool_name, args_json)` — returns ik_access_t
- `ik_tool_scheduler_conflicts(access_a, access_b)` — conflict matrix check

### Integration points

**Streaming callback** (`repl_callbacks.c`):
- Hook `IK_STREAM_TOOL_CALL_DONE` to feed tool calls into scheduler as they arrive
- Currently these events are logged but ignored

**Completion callback** (`repl_callbacks.c`):
- `ik_repl_completion_callback()` sets `scheduler->stream_complete = true`
- No longer extracts tool calls here (scheduler already has them)

**Main loop polling** (`repl.c`):
- Replace single-tool poll with `ik_tool_scheduler_poll()`
- Check `ik_tool_scheduler_all_terminal()` to trigger message assembly

**Message assembly** (new):
- Build one assistant message with all tool_call content blocks
- Build one user message with all tool_result content blocks
- Persist to DB and add to `agent->messages[]`

**Agent struct** (`agent.h`):
- Replace single `pending_tool_call`, `tool_thread`, `tool_child_pid`, etc. with `ik_tool_scheduler_t *scheduler`

### Files to modify

| File | Change |
|------|--------|
| `agent.h` | Replace single tool fields with scheduler pointer |
| `repl_callbacks.c` | Hook IK_STREAM_TOOL_CALL_DONE, update completion callback |
| `repl_response_helpers.c` | Remove single-tool extraction (scheduler handles it) |
| `repl_tool.c` | Multi-tool dispatch via scheduler |
| `repl_tool_completion.c` | Poll scheduler, assemble multi-tool messages |
| `message.c` | Build multi-tool-call/result messages |
| `repl_tool_json.c` | Handle multiple tool results |
| `layer_spinner.c` | Replace generic spinner with status line output |

### Files unchanged

| File | Why |
|------|-----|
| `tool_external.c` | fork/exec per tool works as-is |
| `tool_executor.c` | Dispatch logic unchanged |
| `repl.c` | Event loop structure stays, just different poll target |
| Provider streaming parsers | Already emit per-block events |

## Industry Context

The design draws from patterns observed across Claude Code, OpenCode, Gemini CLI, LangChain/LangGraph, and the OpenAI ecosystem. Our approach is more granular than any surveyed implementation — resource-level conflict detection rather than binary all-parallel or all-serial per batch. This allows a write to file A to run concurrently with a read of file B, which none of the surveyed implementations support.
