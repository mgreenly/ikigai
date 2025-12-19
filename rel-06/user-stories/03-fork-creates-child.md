# Fork Creates Child

## Description

User runs `/fork` to create a new child agent. The child inherits the parent's conversation history and becomes the active agent.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> Hello, how are you?

I'm doing well, thank you for asking!

> /fork

───────────────────────── agent [xyz789...] ─────────────────────────
Forked from abc123...

> _
```

## Walkthrough

1. User types `/fork` and presses Enter
2. REPL parses slash command, dispatches to fork handler
3. Handler waits for any running tools to complete (sync barrier)
4. Handler generates UUID for child agent
5. Handler inserts child into registry with `parent_uuid = current.uuid`
6. Handler creates `ik_agent_ctx_t` for child
7. Handler copies parent's conversation history to child
8. Handler records `fork_message_id` in registry (where we branched)
9. Handler switches to child agent
10. Handler displays "Forked from {parent_uuid}"
11. User is now on child agent with inherited history

## Reference

```json
{
  "command": "/fork",
  "arguments": "none or prompt string",
  "creates": {
    "uuid": "new base64url UUID",
    "parent_uuid": "current agent UUID",
    "fork_message_id": "last message ID before fork",
    "status": "running"
  },
  "behavior": {
    "sync_barrier": true,
    "history_inheritance": true,
    "auto_switch": true
  }
}
```
