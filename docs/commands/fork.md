# /fork

## NAME

/fork - create a child agent

## SYNOPSIS

```
/fork
/fork [PROMPT]
/fork --model MODEL
/fork --model MODEL [PROMPT]
```

## DESCRIPTION

Create a new child agent that inherits the current agent's conversation history, toolset filters, and pinned paths. The current agent becomes the child's parent.

After creation, the terminal switches to the child agent.

If PROMPT is given, the child immediately begins an LLM request with that text. Without a prompt, the child starts idle and waits for user input.

The child is a full agent with its own conversation, tools, and lifecycle. It can be independently killed, can send and receive mail, and can itself fork further children.

## OPTIONS

**--model** MODEL
: Override the parent's model for the child agent. MODEL is any model identifier accepted by `/model` (e.g., `gpt-4o`, `claude-sonnet-4-20250514`).

**PROMPT**
: Text to send to the child's LLM immediately after creation. Everything after `/fork` (or after `--model MODEL`) is treated as the prompt.

## EXAMPLES

Create an idle child and type a task manually:

```
> /fork
Forked. Child: a1b2c3d4
> Analyze the test coverage gaps in src/repl.c
```

Fork with an immediate task:

```
> /fork Research OAuth2 implementation patterns and summarize the tradeoffs
Forked. Child: a1b2c3d4
```

Fork with a different model:

```
> /fork --model claude-sonnet-4-20250514 Review this code for security issues
Forked. Child: a1b2c3d4
```

Fan-out pattern — fork multiple children for parallel work:

```
> /fork Analyze the database schema and document all foreign key relationships
Forked. Child: a1b2c3d4
> /fork Review the error handling in src/provider/ for unchecked returns
Forked. Child: e5f6g7h8
> /mail-check
```

## NOTES

The parent's conversation history is copied in memory at fork time. No database rows are copied — the child references the parent's history via a fork boundary marker. This makes forking cheap regardless of conversation size.

The root agent cannot be forked from a child — `/fork` always operates on the current agent.

## SEE ALSO

[/kill](kill.md), [/mail-send](mail.md#mail-send), [Commands](../commands.md)
