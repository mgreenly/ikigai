# /capture

## NAME

/capture - enter capture mode for composing a child agent's task

/cancel - exit capture mode without forking

## SYNOPSIS

```
/capture
...text...
/fork

/capture
...text...
/cancel
```

## DESCRIPTION

`/capture` enters capture mode. While in capture mode, user input is displayed in the scrollback and persisted to the database, but is never sent to the current agent's LLM. This allows the human to compose a task for a child agent without the parent acting on it.

Capture mode ends in one of two ways:

**`/fork`**: Collects all captured text and creates a child agent with that content as the starting prompt. The child begins working immediately. See [/fork](fork.md).

**`/cancel`**: Ends capture mode without creating a child. The captured text remains visible in the scrollback but has no effect — the LLM never saw it.

A visual indicator (e.g., `(capturing)` prompt label) shows when capture mode is active.

## BEHAVIOR

- Each input line is rendered to the scrollback immediately (immutable, always visible)
- Each input is persisted to the database with `kind="capture"`
- Captured messages are excluded from the LLM's conversation history
- On session replay/restore, captured messages render to scrollback but remain excluded from LLM history
- `/capture` and `/fork` act as bookends — `/capture` starts accumulation, `/fork` ends it

## EXAMPLES

Compose a multi-line task for a child:

```
> /capture
(capturing) > Enumerate all the *.md files in the project.
(capturing) > Count the words in each file.
(capturing) > Build a summary table sorted by word count.
> /fork
Forked. Child: a1b2c3d4
```

Cancel capture without forking:

```
> /capture
(capturing) > Actually, I'll do this myself.
> /cancel
Capture cancelled.
```

The captured text ("Actually, I'll do this myself.") stays in the scrollback but the parent's LLM never sees it.

## NOTES

`/capture` and `/cancel` are human-only commands. They never appear in the tool list. Agents do not need capture mode — they call `fork(prompt: "...")` directly as an internal tool.

## SEE ALSO

[/fork](fork.md), [Commands](../commands.md)
