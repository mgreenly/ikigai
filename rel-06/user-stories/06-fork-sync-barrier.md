# Fork Sync Barrier

## Description

Fork waits for all running tools to complete before executing. This prevents forking mid-operation which could leave inconsistent state.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> Search for files matching *.c

[Tool: file_search running...]

> /fork

[Waiting for tools to complete...]
[Tool: file_search completed]

───────────────────────── agent [xyz789...] ─────────────────────────
Forked from abc123...

> _
```

## Walkthrough

1. Agent is executing a tool (file_search)
2. User types `/fork`
3. Fork handler checks for running tools
4. Handler displays "Waiting for tools to complete..."
5. Handler blocks until all tools finish
6. Tool completes, result added to conversation
7. Fork proceeds with complete conversation state
8. Child inherits conversation including tool result

## Reference

```json
{
  "sync_barrier": {
    "triggers_on": "/fork command",
    "waits_for": "all running tools",
    "message": "Waiting for tools to complete...",
    "ensures": "conversation state is consistent at fork point"
  }
}
```
