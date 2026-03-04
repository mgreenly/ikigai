# send

## NAME

send - send a message to another agent

## SYNOPSIS

```json
{"name": "send", "arguments": {"to": "...", "message": "..."}}
```

## DESCRIPTION

`send` delivers a message to a running agent. The recipient can retrieve it with the [wait](wait.md) tool. Messages are stored in the database and survive process restarts.

This is the primary mechanism for inter-agent communication. A common pattern is for a child agent to `send` its final results to its parent, then the parent uses `wait` to collect them.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `to` | string | yes | UUID of the recipient agent. The recipient must be a running agent in the same session. |
| `message` | string | yes | Message content to deliver. Any string format (plain text, JSON, markdown). |

## RETURNS

On success:

```json
{
  "status": "sent",
  "to": "a1b2c3d4-..."
}
```

On failure, returns an error object with `code` and `message`.

## EXAMPLES

Child sends results to its parent:

```json
{
  "name": "send",
  "arguments": {
    "to": "<parent-uuid>",
    "message": "Analysis complete. Found 3 unchecked return values in repl.c at lines 42, 87, 103."
  }
}
```

Send structured data as JSON:

```json
{
  "name": "send",
  "arguments": {
    "to": "<parent-uuid>",
    "message": "{\"status\": \"done\", \"files_checked\": 14, \"issues\": []}"
  }
}
```

## NOTES

`send` is non-blocking. It delivers the message to the database and returns immediately. The recipient does not need to be actively waiting when `send` is called — messages queue in the database until the recipient calls `wait`.

An agent can send to any running agent in the session, not just its parent. This enables multi-agent coordination topologies beyond simple parent-child hierarchies.

## SEE ALSO

[fork](fork.md), [kill](kill.md), [wait](wait.md), [Tools](../tools.md), [/send](../commands/send.md)
