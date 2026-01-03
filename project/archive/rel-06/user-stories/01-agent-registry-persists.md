# Agent Registry Persists

## Description

When an agent is created, it is recorded in the database registry. On restart, ikigai can reconstruct agent state from the registry.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /fork

───────────────────────── agent [xyz789...] ─────────────────────────
Agent created

> _

[user restarts ikigai]

───────────────────────── agent [xyz789...] ─────────────────────────
> _
```

## Walkthrough

1. User runs `/fork` command
2. Handler generates new UUID for child agent
3. Handler inserts row into `agents` table:
   - `uuid`: new UUID (base64url, 22 chars)
   - `parent_uuid`: current agent's UUID
   - `status`: 'running'
   - `created_at`: current timestamp
4. Handler creates `ik_agent_ctx_t` in memory
5. Handler switches to new agent
6. On restart, ikigai queries `agents` table for `status = 'running'`
7. For each running agent, reconstruct `ik_agent_ctx_t` from registry + history

## Reference

```json
{
  "table": "agents",
  "columns": {
    "uuid": "TEXT PRIMARY KEY",
    "name": "TEXT",
    "parent_uuid": "TEXT REFERENCES agents(uuid)",
    "fork_message_id": "TEXT",
    "status": "TEXT NOT NULL",
    "created_at": "BIGINT NOT NULL",
    "ended_at": "BIGINT"
  }
}
```
