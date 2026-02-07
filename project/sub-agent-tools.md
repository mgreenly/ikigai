# Sub-Agent Tools

**Status:** Design finalized. See `.claude/data/scratch.md` for full design decisions (rel-11).

## Philosophy

Provide simple, composable primitives that power users can build complex workflows around. Tools must be straightforward enough that LLMs never get confused about how/when to use them.

Design principles:
- **Minimal decision paths** - LLM makes fewer choices
- **Consistent interface** - everything looks the same
- **Composable** - simple primitives combine into complex patterns
- **Stupid simple** - if the LLM can get confused, simplify further

## Internal Tools

Each tool gets its own registry entry with a precise schema. The LLM sees them in a single alphabetized list alongside external tools — no distinction.

| Tool name (LLM sees) | Backed by command | Purpose |
|---|---|---|
| `fork` | `/fork` | Create a child agent |
| `kill` | `/kill` | Terminate an agent and its descendants |
| `send` | `/send` | Send a message to another agent |
| `wait` | `/wait` | Wait for messages (blocking, with fan-in support) |

## Human-Only Commands

These are NOT internal tools. They never appear in the tool list.

| Command | Purpose |
|---|---|
| `/capture` | Enter capture mode for composing sub-agent tasks |
| `/cancel` | Exit capture mode without forking |
| `/reap` | Remove all dead agents from nav rotation |

## Slash Command vs Tool Behavioral Differences

Slash commands are user-initiated on the main thread. Internal tools are agent-initiated on a worker thread.

### fork

**Slash command (human):**
- `/fork` with active capture — creates child with captured content, switches UI to child
- `/fork` without capture — creates idle child, switches UI to child
- No prompt argument. Human provides the task via `/capture` ... `/fork` or by typing after switching.

**Tool (agent):**
- `fork(name: "file-reader", prompt: "...")` — creates child with full parent context + prompt, no UI switch
- `name` is required. Short human-readable label for the child (used in status layer, wait results).
- `prompt` is required. Agent must tell the child what to do.

### kill

**Slash command (human):**
- Kills target and all descendants (always cascades)
- Dead agents stay in nav rotation with frozen scrollback and no edit input
- Human can visit dead agents to review output before running `/reap`

**Tool (agent):**
- Kills target and all descendants, returns JSON result
- No UI side effects, calling agent keeps running

### send

Behavioral difference is only rendering (scrollback vs JSON return). No control flow divergence.

### wait

**Slash command (human):**
- `/wait TIMEOUT [UUID1 UUID2 ...]` — puts current agent into tool-executing state
- Spinner runs, main loop continues, other agents serviced
- Escape interrupts (same as any tool execution)
- Result renders to scrollback on completion

**Tool (agent):**
- `wait(timeout: N, from_agents: ["uuid1", "uuid2"])` — blocks on worker thread
- Returns structured results as JSON tool result

Both share the same core wait logic — identical mechanism.

## Wait Semantics

`wait` has two modes:

### Mode 1 — Next message (no agent IDs)

`wait(timeout: 30)` returns the first message from anyone. No status display. Useful for ad-hoc communication. `wait(timeout: 0)` is an instant non-blocking check.

### Mode 2 — Fan-in (with agent IDs)

`wait(timeout: 30, from_agents: ["uuid1", "uuid2", "uuid3"])` waits for ALL listed agents to respond.

**Return value:**
```json
{
  "results": [
    {"agent_id": "uuid1", "name": "file-reader",    "status": "received", "message": "..."},
    {"agent_id": "uuid2", "name": "code-analyzer",   "status": "received", "message": "..."},
    {"agent_id": "uuid3", "name": "test-runner",     "status": "running"}
  ]
}
```

Four possible statuses per agent: `received`, `running`, `idle`, `dead`.

**Early termination:**
- All agents responded — return immediately
- All agents either responded, idle, or dead — return immediately (nothing left to wait for)
- Timeout — return whatever state exists (partial results)

The calling agent decides what to do with partial results — wait again, kill stragglers, proceed.

**Messages are consumed on wait.** `wait` pops messages. No accumulation, no cleanup, no delete command.

### Status Layer (human-visible)

When the user views an agent executing a fan-in `wait`, a status layer renders between the scrollback and spinner:

```
  file-reader      ◐ running    22 calls  13k tokens
  code-analyzer    ✓ received   13 calls  11k tokens
  test-runner      ○ idle        9 calls  14k tokens
────────────────────────────────────────────────────────
```

Each line shows: agent name, status indicator + label, tool call count, token usage.

Status indicators:
- `◐ running` — actively streaming or executing a tool
- `✓ received` — sent a message, consumed by wait
- `○ idle` — turn ended without sending a message
- `✗ dead` — killed

Updates live as notifications arrive. The human's dashboard for fan-in operations.

### PG LISTEN/NOTIFY

Wake-up uses PostgreSQL's async notification system. Single channel per agent: `agent_event_<uuid>`. Two event types trigger NOTIFY: mail insertion (from `send`) and agent death (from `kill`). Worker thread uses `select()` on the PG socket fd with remaining timeout.

## Dead Agent Reaping

Kill marks agents as dead but does NOT remove them from the nav rotation. Dead agents remain visitable — frozen scrollback, no edit input, kill event visible. The user can tour dead agents to review their output.

`/reap` removes all dead agents at once:
1. Switch user to first living agent
2. Remove all dead agents from `repl->agents[]`
3. Free memory

`/reap` is human-only. Agents don't need it — from the agent's perspective, `kill` returned success.

## Capture Mode

**Problem**: When a human types a task for a sub-agent, the parent's LLM would execute it.

**Solution**: `/capture` enters capture mode. User input is displayed in scrollback and persisted to DB but never sent to the parent's LLM. `/fork` ends capture and creates the child with the captured content. `/cancel` ends capture without forking.

```
/capture                          <- event rendered in scrollback
Enumerate all the *.md files,     <- rendered in scrollback, not sent to LLM
count their words and build       <- rendered in scrollback, not sent to LLM
a summary table.                  <- rendered in scrollback, not sent to LLM
/fork                             <- child created with captured content
```

- Each input persisted with `kind="capture"` — excluded from LLM conversation
- `/cancel` ends capture without forking; captured text stays in scrollback
- Captured content is immutable once rendered

## Child Lifecycle

```
1. Parent: fork(prompt: "do X")
   |-> Child starts, receives parent UUID

2. Child works autonomously

3. Child completes, sends result to parent:
   send(to: parent_uuid, message: "done: ...")

4. Parent: wait(timeout: 60, from_agents: [child_uuid])
   |-> Receives structured result

5. Parent either:
   - kill(uuid) to terminate
   - send(uuid, "do more") to continue
```

Child doesn't terminate on completion - it idles. This allows reuse without re-forking.

## Related

- [external-tool-architecture.md](external-tool-architecture.md) - External tools architecture (unified registry)
- `.claude/data/scratch.md` - Full rel-11 design decisions (threading, dispatch, DB, registry changes)
