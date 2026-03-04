# kill

## NAME

kill - terminate an agent and all its descendants

## SYNOPSIS

```json
{"name": "kill", "arguments": {"uuid": "..."}}
```

## DESCRIPTION

`kill` marks an agent as dead and cascades to all its descendants. The target agent stops receiving messages and is excluded from future `wait` fan-in results.

An agent cannot kill itself, the root agent, or its own parent.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `uuid` | string | yes | UUID of the agent to terminate. Cascades to all descendant agents. |

## RETURNS

On success:

```json
{
  "killed": ["a1b2c3d4-..."]
}
```

The `killed` array lists the UUIDs of all agents that were terminated (including descendants).

On failure, returns an error object with `code` and `message`. Error codes:

| Code | Meaning |
|------|---------|
| `AGENT_NOT_FOUND` | No agent with the given UUID exists |
| `ALREADY_DEAD` | The target agent is not running |
| `CANNOT_KILL_ROOT` | Cannot kill the root agent |
| `CANNOT_KILL_PARENT` | Cannot kill the caller's own parent |

## EXAMPLES

Terminate a child after it finishes sending results:

```json
{"name": "kill", "arguments": {"uuid": "a1b2c3d4-..."}}
```

Fork a worker, collect its result, then kill it:

```json
{"name": "fork", "arguments": {"name": "worker", "prompt": "Summarize the error log."}}
{"name": "wait", "arguments": {"timeout": 60, "from_agents": ["<worker-uuid>"]}}
{"name": "kill", "arguments": {"uuid": "<worker-uuid>"}}
```

## NOTES

Killing an agent is a soft termination: the agent's database record is marked dead and the in-memory flag is set. Any inflight LLM response the agent is generating will be discarded when it next checks its state.

Dead agents remain in the database and their conversation history is preserved. Only their active processing stops.

## SEE ALSO

[fork](fork.md), [send](send.md), [wait](wait.md), [Tools](../tools.md), [/kill](../commands/kill.md)
