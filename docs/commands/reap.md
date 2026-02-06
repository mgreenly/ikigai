# /reap

## NAME

/reap - remove all dead agents from nav rotation

## SYNOPSIS

```
/reap
```

## DESCRIPTION

Remove all dead agents from the agent navigation rotation and free their memory. Dead agents are created by `/kill` — kill marks agents as dead but keeps them in the nav rotation so the user can review their scrollback output.

`/reap` is the explicit cleanup step. It:

1. Switches the user to the first living agent (typically the parent or root)
2. Removes all dead agents from `repl->agents[]`
3. Frees their memory

Takes no arguments. Removes ALL dead agents at once.

## HUMAN-ONLY

`/reap` is a human-only command. It never appears in the tool list. Agents use `kill` directly — from the agent's perspective, `kill` returns success and the target is gone. Dead agents lingering in the nav rotation is purely a UI concern for the human operator.

## EXAMPLES

After a parent kills its children and reviews their output:

```
> /kill a1b2
Agent a1b2c3d4 killed.

> [ctrl+down to visit dead agent, review scrollback]
> [ctrl+up back to parent]

> /reap
Reaped 2 dead agents.
```

## NOTES

If there are no dead agents, `/reap` is a no-op.

Dead agents have frozen scrollback (readable, not writable) and no edit input line. They remain in the nav rotation until `/reap` is called.

## SEE ALSO

[/kill](kill.md), [/fork](fork.md), [Commands](../commands.md)
