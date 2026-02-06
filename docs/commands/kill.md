# /kill

## NAME

/kill - terminate an agent

## SYNOPSIS

```
/kill
/kill UUID
```

## DESCRIPTION

Terminate an agent and mark it as dead in the database. A killed agent's conversation history is preserved but it can no longer execute or receive messages.

Without arguments, kills the current agent and switches to its parent.

With a UUID argument, kills that specific agent. Partial UUID matches are accepted — if the prefix uniquely identifies one agent, it is used.

The root agent cannot be killed.

## OPTIONS

**UUID**
: The UUID (or unique prefix) of the agent to kill. Without this, the current agent is killed.

## TOOL VARIANT

Agents call `kill` as an internal tool with different behavior than the slash command:

```json
{"name": "kill", "arguments": {"uuid": "a1b2c3d4"}}
```

| | Slash command | Tool |
|---|---|---|
| Caller | Human | Agent |
| UI switch | Yes — moves to parent if current agent killed | No — no UI side effects |
| Return | None | JSON success/failure |

The calling agent keeps running after issuing a kill.

## EXAMPLES

Kill the current agent and return to its parent:

```
> /kill
Agent a1b2c3d4 killed. Switched to parent.
```

Kill a specific child by UUID prefix:

```
> /kill a1b2
Agent a1b2c3d4 killed.
```

## NOTES

If the target UUID prefix matches multiple agents, the command fails with an ambiguity error. Provide more characters to disambiguate.

Killing an agent that is the current agent automatically switches the terminal to its parent. Killing a non-current agent has no effect on the terminal view.

## SEE ALSO

[/fork](fork.md), [Commands](../commands.md)
