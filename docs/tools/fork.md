# fork

## NAME

fork - create a child agent with a specific task

## SYNOPSIS

```json
{"name": "fork", "arguments": {"name": "...", "prompt": "..."}}
```

## DESCRIPTION

`fork` creates a child agent and immediately gives it a task to work on. The child starts with a clean conversation — it does not inherit the parent's conversation history. Both parent and child run concurrently in the agent process tree.

This tool is the LLM-driven counterpart to the [/fork](../commands/fork.md) slash command. Unlike the slash command (which is interactive), the tool is designed for delegation: the parent passes a structured prompt and the child works independently.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | yes | Short human-readable label (e.g., `"analyzer"`, `"worker-1"`). Appears in the wait status layer and in fan-in results. |
| `prompt` | string | yes | The task for the child agent to work on. The child begins with this as its initial user message. |

## RETURNS

On success:

```json
{
  "child_uuid": "a1b2c3d4-...",
  "child_name": "analyzer"
}
```

Use `child_uuid` with [send](send.md) and [wait](wait.md) to coordinate with the child.

On failure, returns an error object with `code` and `message`.

## EXAMPLES

Fork a single child to do a focused task:

```json
{
  "name": "fork",
  "arguments": {
    "name": "coverage-check",
    "prompt": "Analyze test coverage gaps in apps/ikigai/repl.c and report findings."
  }
}
```

Fan-out pattern — fork multiple children then fan-in their results:

```json
{"name": "fork", "arguments": {"name": "schema-reader",   "prompt": "Document all foreign key relationships in the database schema."}}
{"name": "fork", "arguments": {"name": "error-reviewer",  "prompt": "Review error handling in apps/ikigai/ for unchecked return values."}}
{"name": "wait", "arguments": {"timeout": 120, "from_agents": ["<schema-reader-uuid>", "<error-reviewer-uuid>"]}}
```

## NOTES

The child inherits the parent's provider, model, and reasoning level. It does not inherit pinned documents, toolset filters, or conversation history.

The child starts running immediately once `fork` returns. The parent can continue working, call `fork` again to create more children, or call `wait` to collect results.

A child can itself call `fork`, building an unbounded process tree.

## SEE ALSO

[kill](kill.md), [send](send.md), [wait](wait.md), [Tools](../tools.md), [/fork](../commands/fork.md)
