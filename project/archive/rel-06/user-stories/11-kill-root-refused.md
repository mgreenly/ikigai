# Kill Root Refused

## Description

The root agent (agent with no parent) cannot be killed. This ensures there's always at least one agent.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /kill

Error: Cannot kill root agent

> /kill abc123...

Error: Cannot kill root agent

> _
```

## Walkthrough

1. User is on root agent (parent_uuid is NULL)
2. User attempts `/kill`
3. Handler checks if current agent is root
4. Handler detects `parent_uuid = NULL`
5. Handler displays error: "Cannot kill root agent"
6. No state changes, user remains on root agent
7. Same error if targeting root by UUID

## Reference

```json
{
  "root_agent": {
    "definition": "agent with parent_uuid = NULL",
    "protected": true,
    "kill_behavior": "refuse with error message"
  },
  "error": "Cannot kill root agent"
}
```
