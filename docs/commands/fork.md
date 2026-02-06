# /fork

## NAME

/fork - create a child agent

## SYNOPSIS

```
/fork
```

## DESCRIPTION

Create a new child agent that inherits the current agent's full conversation history, toolset filters, and pinned paths. The current agent becomes the child's parent. The terminal switches to the child agent.

`/fork` has two modes depending on whether capture mode is active:

**With capture** (`/capture` ... `/fork`): Collects all text entered during capture mode and creates a child with that content as the starting prompt. The child begins working immediately. The parent's LLM never sees the captured text.

**Without capture**: Creates an idle child. The terminal switches to the child and the human types the task directly.

`/fork` takes no arguments. The human provides the child's task either through capture mode or by typing after the switch.

## CAPTURE MODE

The typical workflow for giving a child a task:

```
> /capture
(capturing) > Enumerate all the *.md files,
(capturing) > count their words and build
(capturing) > a summary table.
> /fork
Forked. Child: a1b2c3d4
```

Each line entered during capture mode is rendered in the parent's scrollback and persisted to the database, but is never sent to the parent's LLM. `/fork` ends capture mode, collects the captured text, and creates the child with it.

See [/capture](capture.md) for full details.

## TOOL VARIANT

Agents call `fork` as an internal tool with different behavior than the slash command:

```json
{"name": "fork", "arguments": {"prompt": "Analyze test coverage gaps in src/repl.c"}}
```

| | Slash command | Tool |
|---|---|---|
| Caller | Human | Agent |
| Prompt source | Capture mode or typed after switch | Required `prompt` parameter |
| UI switch | Yes — terminal moves to child | No — parent keeps focus |
| Child start | Immediate if captured, idle otherwise | Immediate with prompt |

The tool returns the child's UUID. Both parent and child run concurrently.

## EXAMPLES

Fork an idle child and type a task:

```
> /fork
Forked. Child: a1b2c3d4
> Analyze the test coverage gaps in src/repl.c
```

Fork with a captured task:

```
> /capture
(capturing) > Research OAuth2 implementation patterns.
(capturing) > Compare token-based vs session-based approaches.
(capturing) > Report findings via /send when complete.
> /fork
Forked. Child: a1b2c3d4
```

Fan-out pattern — fork multiple children for parallel work:

```
> /capture
(capturing) > Analyze the database schema and document all foreign key relationships.
> /fork
Forked. Child: a1b2c3d4

> /capture
(capturing) > Review error handling in src/provider/ for unchecked returns.
> /fork
Forked. Child: e5f6g7h8

> /wait 120 a1b2 e5f6
```

## NOTES

The parent's conversation history is copied in memory at fork time. No database rows are copied — the child references the parent's history via a fork boundary marker. Forking is cheap regardless of conversation size (3 DB rows written, constant).

The child is a full agent with its own conversation, tools, and lifecycle. It can independently send and receive mail, and can itself fork further children.

## SEE ALSO

[/capture](capture.md), [/kill](kill.md), [/send](send.md), [/wait](wait.md), [Commands](../commands.md)
