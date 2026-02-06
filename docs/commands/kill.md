# /kill

## NAME

/kill - terminate an agent and its descendants

## SYNOPSIS

```
/kill
/kill UUID
```

## DESCRIPTION

Terminate an agent, marking it and all its descendants as dead. Dead agents remain in the nav rotation with frozen scrollback and no edit input — the user can visit them to review output. Use `/reap` to remove all dead agents.

Without arguments, kills the current agent and switches to its parent.

With a UUID argument, kills that specific agent and all its descendants. Partial UUID matches are accepted — if the prefix uniquely identifies one agent, it is used.

Kill always cascades. There is no non-cascading kill.

The root agent cannot be killed.

## OPTIONS

**UUID**
: The UUID (or unique prefix) of the agent to kill. Without this, the current agent is killed.

## DEAD AGENT STATE

After being killed, agents enter a dead state:
- Scrollback frozen (readable, not writable)
- No edit input line rendered
- Kill event visible in scrollback
- Still in nav rotation (ctrl+up/down reaches them)
- Cannot receive mail or execute tools

Use `/reap` to remove all dead agents from the nav rotation.

## TOOL VARIANT

Agents call `kill` as an internal tool with different behavior than the slash command:

```json
{"name": "kill", "arguments": {"uuid": "a1b2c3d4"}}
```

| | Slash command | Tool |
|---|---|---|
| Caller | Human | Agent |
| UI effect | Dead agents stay in nav, viewable | No UI side effects |
| Cascade | Always | Always |
| Return | None | JSON with list of killed UUIDs |

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
Agent a1b2c3d4 killed (+ 2 descendants).
```

Review a dead agent's output then clean up:

```
> /kill a1b2
Agent a1b2c3d4 killed.
> [ctrl+down to visit dead agent]
> [review scrollback output]
> [ctrl+up back to parent]
> /reap
Reaped 1 dead agent.
```

## NOTES

If the target UUID prefix matches multiple agents, the command fails with an ambiguity error. Provide more characters to disambiguate.

Killing an agent that is the current agent automatically switches the terminal to its parent. Killing a non-current agent has no effect on the terminal view — the dead agent is simply viewable in the nav rotation.

## SEE ALSO

[/reap](reap.md), [/fork](fork.md), [/send](send.md), [/wait](wait.md), [Commands](../commands.md)
