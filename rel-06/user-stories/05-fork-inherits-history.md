# Fork Inherits History

## Description

When forking, the child agent inherits the parent's full conversation history. This uses delta storage - child stores fork point + post-fork messages only.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> What is 2+2?

2+2 equals 4.

> What is 3+3?

3+3 equals 6.

> /fork

───────────────────────── agent [xyz789...] ─────────────────────────
Forked from abc123...

> What was my first question?

Your first question was "What is 2+2?"

> _
```

## Multi-Level Inheritance

Inheritance works across multiple generations - a grandchild can access context from its grandparent:

```text
Agent A (grandparent): Q1, A1, Q2, A2
Agent A runs /fork → Agent B (parent)
Agent B: Q3, A3
Agent B runs /fork → Agent C (grandchild)

In Agent C:
> What was the first question?

"Q1" (from grandparent Agent A)
```

The system walks backwards through the ancestor chain (C → B → A) until it hits a clear event or the origin, then plays forward (A → B → C) to reconstruct the full history.

## Walkthrough

1. Parent agent has conversation: Q1, A1, Q2, A2
2. User runs `/fork`
3. Handler records `fork_message_id` = ID of A2 (last message)
4. Child agent created with reference to fork point
5. Child's effective history = walk full ancestor chain and replay forward
6. User asks child about earlier conversation
7. Child can access full history (inherited from parent)
8. New messages after fork are stored only in child
9. Parent's history unchanged by child's new messages

## Reference

```json
{
  "delta_storage": {
    "fork_message_id": "last message ID before fork",
    "child_stores": "only post-fork messages",
    "replay": "parent history up to fork + child messages"
  },
  "replay_algorithm": {
    "name": "walk backwards, play forwards",
    "steps": [
      "Walk ancestor chain backwards to clear event",
      "Collect: [child, parent, grandparent, ..., origin]",
      "Reverse: [origin, ..., grandparent, parent, child]",
      "Replay forward, appending each ancestor's messages"
    ]
  },
  "clear_events": {
    "role": "context boundaries",
    "behavior": "when walking backwards, stop at clear event",
    "note": "clear event marks where context started"
  },
  "history_structure": {
    "root": "msg1 -> msg2 -> msg3 -> msg4 (parent continues)",
    "child": "         fork_point -> msg4' -> msg5' (child after fork)"
  }
}
```
