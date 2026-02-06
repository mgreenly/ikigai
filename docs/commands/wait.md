# /wait

## NAME

/wait - wait for messages from other agents

## SYNOPSIS

```
/wait TIMEOUT
/wait TIMEOUT UUID1 [UUID2 ...]
```

## DESCRIPTION

Wait for messages in the current agent's mailbox. Puts the agent into a tool-executing state — the spinner runs, the main loop continues servicing other agents, and escape interrupts the wait.

Messages are consumed (popped) when received. No separate delete or cleanup is needed.

`/wait` has two modes depending on whether agent UUIDs are provided:

### Mode 1 — Next message

```
/wait 30
```

Wait up to 30 seconds for any message from any agent. Returns the first message received. `timeout: 0` performs an instant non-blocking check.

### Mode 2 — Fan-in

```
/wait 60 a1b2 e5f6 c3d4
```

Wait for ALL listed agents to send a message. Returns structured results with per-agent status. A status layer renders between the scrollback and spinner showing progress:

```
Waiting for sub-agents:
  file-reader      [checkmark] received
  code-analyzer    [spinner] running
  test-runner      [spinner] running
```

Updates live as messages arrive. This is the primary workflow for fan-out/fan-in patterns.

## ARGUMENTS

**TIMEOUT**
: Maximum seconds to wait. Required. Use 0 for instant check.

**UUID1, UUID2, ...**
: Optional list of agent UUIDs (or unique prefixes) to wait for. When provided, waits for ALL listed agents. When omitted, returns first message from anyone.

## RESULTS

### Mode 1 (next message)

Returns the message content, sender UUID, and timestamp.

### Mode 2 (fan-in)

Returns a structured result with one entry per agent:

```json
{
  "results": [
    {"agent_id": "uuid1", "name": "file-reader",    "status": "received", "message": "..."},
    {"agent_id": "uuid2", "name": "code-analyzer",   "status": "received", "message": "..."},
    {"agent_id": "uuid3", "name": "test-runner",     "status": "running"}
  ]
}
```

Three possible statuses per agent:
- **received** — message arrived, content included
- **running** — agent is alive but hasn't sent a message yet
- **dead** — agent was killed or died before sending

## EARLY TERMINATION

- All agents responded — return immediately
- All agents either responded or dead — return immediately (nothing left to wait for)
- Timeout — return whatever state exists (partial results)
- Escape — interrupt, return current state

## TOOL VARIANT

Agents call `wait` as an internal tool:

```json
{"name": "wait", "arguments": {"timeout": 60, "from_agents": ["a1b2", "e5f6"]}}
```

| | Slash command | Tool |
|---|---|---|
| Caller | Human | Agent |
| Rendering | Result in scrollback + status layer | JSON result |
| Behavior | Identical | Identical |

Both share the same core wait logic. PG LISTEN/NOTIFY wakes the worker thread when mail arrives or an agent dies.

## EXAMPLES

Wait for any message (ad-hoc):

```
> /wait 30
From: a1b2c3d4 (just now)
Analysis complete. Found 3 issues.
```

Fan-out/fan-in — fork workers then wait for all:

```
> /capture
(capturing) > Analyze database schema foreign keys.
> /fork
Forked. Child: a1b2c3d4

> /capture
(capturing) > Review error handling in src/provider/.
> /fork
Forked. Child: e5f6g7h8

> /wait 120 a1b2 e5f6
Waiting for sub-agents:
  a1b2c3d4    [checkmark] received
  e5f6g7h8    [checkmark] received

Results:
  a1b2c3d4: Found 12 FK relationships, 3 missing indexes...
  e5f6g7h8: 2 unchecked returns in provider.c...
```

Instant check (non-blocking):

```
> /wait 0
No messages.
```

## NOTES

The wake-up mechanism uses PostgreSQL LISTEN/NOTIFY. The `send` command and agent death both fire notifications on the waiting agent's channel. The worker thread uses `select()` on the PG socket fd, avoiding polling.

## SEE ALSO

[/send](send.md), [/fork](fork.md), [/kill](kill.md), [/reap](reap.md), [Commands](../commands.md)
