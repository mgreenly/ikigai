# wait

## NAME

wait - wait for messages from other agents

## SYNOPSIS

```json
{"name": "wait", "arguments": {"timeout": N}}
{"name": "wait", "arguments": {"timeout": N, "from_agents": ["uuid", ...]}}
```

## DESCRIPTION

`wait` blocks until a message arrives or the timeout expires. It has two modes:

**Next-message mode** (no `from_agents`): Returns the next message from any agent. Useful for simple parent-child workflows where a parent waits for a single child to report back.

**Fan-in mode** (`from_agents` provided): Waits for all named agents to either send a message or terminate. Returns one entry per agent with the message or termination status. Useful for collecting results from multiple parallel workers.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `timeout` | number | yes | Maximum seconds to wait. Use `0` for a non-blocking check. |
| `from_agents` | array of strings | no | UUIDs of specific agents to wait for (fan-in mode). Omit to wait for next message from anyone. |

## RETURNS

**Next-message mode** — message received:

```json
{
  "from": "a1b2c3d4-...",
  "message": "Task complete."
}
```

**Next-message mode** — timeout:

```json
{"status": "timeout"}
```

**Fan-in mode** — all agents responded or terminated:

```json
{
  "results": [
    {"agent_uuid": "a1b2c3d4-...", "agent_name": "worker-1", "status": "message", "message": "Done."},
    {"agent_uuid": "e5f6g7h8-...", "agent_name": "worker-2", "status": "dead"}
  ]
}
```

Fan-in entry fields:

| Field | Description |
|-------|-------------|
| `agent_uuid` | UUID of the agent |
| `agent_name` | Human-readable name set at fork time |
| `status` | `"message"` if the agent sent a message, `"dead"` if it terminated |
| `message` | Present when `status` is `"message"` |

## EXAMPLES

Wait up to 60 seconds for any message:

```json
{"name": "wait", "arguments": {"timeout": 60}}
```

Non-blocking check for pending messages:

```json
{"name": "wait", "arguments": {"timeout": 0}}
```

Fan-in: fork two workers, wait for both:

```json
{"name": "fork", "arguments": {"name": "worker-1", "prompt": "Analyze coverage in repl.c."}}
{"name": "fork", "arguments": {"name": "worker-2", "prompt": "Analyze coverage in parser.c."}}
{"name": "wait", "arguments": {
  "timeout": 120,
  "from_agents": ["<worker-1-uuid>", "<worker-2-uuid>"]
}}
```

## NOTES

Fan-in mode waits until every listed agent has either sent a message or been marked dead. If an agent terminates without sending, its entry has `status: "dead"` and no `message` field. This prevents fan-in from hanging when a worker fails.

`wait` is interruptible. If the user presses Ctrl-C while an agent is blocked in `wait`, the wait is cancelled and control returns to the agent.

In next-message mode, only unread messages are returned. Messages delivered before the current `wait` call are not replayed.

## SEE ALSO

[fork](fork.md), [kill](kill.md), [send](send.md), [Tools](../tools.md), [/wait](../commands/wait.md)
