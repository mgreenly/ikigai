# Task: Database Schema Migration

**Layer:** 0
**Model:** sonnet/none (simple SQL)
**Depends on:** None (can run in parallel with others)

## Pre-Read

**Source:**
- `migrations/001-*.sql` - Initial schema
- `migrations/002-*.sql` - Existing migrations
- `migrations/003-*.sql` - Existing migrations
- `migrations/004-*.sql` - Existing migrations
- `src/db/agent.c` - Agent CRUD operations
- `src/db/message.c` - Message CRUD operations

**Plan:**
- `scratch/plan/database-schema.md` - Full specification

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

## Postconditions

- [ ] Migration applies successfully
- [ ] Columns exist in agents table (provider, model, thinking_level)
- [ ] Agent CRUD operations work with new columns
- [ ] Message data JSONB stores provider info (provider, model, thinking_level, thinking, thinking_tokens, provider_data)
