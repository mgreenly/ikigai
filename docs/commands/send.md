# /send

## NAME

/send - send a message to another agent

## SYNOPSIS

```
/send UUID "MESSAGE"
```

## DESCRIPTION

Send a message to another agent's mailbox. The recipient must exist and not be dead. The message body must be non-empty and enclosed in quotes.

Messages are persisted in the database and survive restarts. Recipients receive messages by calling `/wait` or the `wait` tool.

## ARGUMENTS

**UUID**
: The recipient agent's UUID or unique prefix.

**MESSAGE**
: The message body, enclosed in quotes.

## TOOL VARIANT

Agents call `send` as an internal tool:

```json
{"name": "send", "arguments": {"to": "a1b2c3d4", "message": "Analysis complete."}}
```

| | Slash command | Tool |
|---|---|---|
| Caller | Human | Agent |
| Rendering | Confirmation in scrollback | JSON result |
| Behavior | Identical | Identical |

Both paths insert a mail row and fire PG NOTIFY to wake any waiting recipient.

## EXAMPLES

Send a result to the parent:

```
> /send e5f6 "Analysis complete. Found 3 unchecked error paths in provider.c"
Message sent to e5f6g7h8.
```

Send follow-up instructions to a child:

```
> /send a1b2 "Good findings. Now fix the unchecked returns you identified."
Message sent to a1b2c3d4.
```

## SEE ALSO

[/wait](wait.md), [/fork](fork.md), [/kill](kill.md), [Commands](../commands.md)
