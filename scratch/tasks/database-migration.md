# Task: Database Schema Migration

**Phase:** 3 - Database
**Depends on:** None (can run in parallel with others)

## Objective

Add provider/model/thinking_level columns to agents table and update message data format.

## Deliverables

1. Create `migrations/005-multi-provider.sql`:
   - Add `provider TEXT` column to agents
   - Add `model TEXT` column to agents
   - Add `thinking_level TEXT` column to agents
   - TRUNCATE tables (clean slate for rel-07)

2. Update `src/db/agent.c`:
   - `ik_db_agent_insert()` - Include new columns
   - `ik_db_agent_update()` - Update provider/model/thinking
   - `ik_db_agent_get()` - Load new columns

3. Update message data JSONB format:
   - Add `provider`, `model`, `thinking_level` fields
   - Add `thinking` field for thinking content
   - Add `thinking_tokens` field
   - Add `provider_data` for opaque metadata

## Reference

- `scratch/plan/database-schema.md` - Full specification

## Verification

- Migration applies successfully
- Agent CRUD includes new columns
- Message data stores provider info
