# Task: Database Schema Migration

**Layer:** 0
**Model:** sonnet/none (simple SQL)
**Depends on:** None (can run in parallel with provider-types.md)

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load database` - PostgreSQL schema and query patterns

**Source:**
- `migrations/004-mail-table.sql` - Migration format reference
- `src/db/agent.h` - `ik_db_agent_row_t` struct definition
- `src/db/agent.c` - Agent CRUD operations

**Plan:**
- `scratch/plan/database-schema.md` - Full specification with JSONB format

## Objective

Add provider/model/thinking_level columns to agents table and update the `ik_db_agent_row_t` struct to include them. This enables agents to persist their provider configuration across sessions. Message table uses JSONB and requires no schema changes - new fields are added by callers.

## Migration File

Create `migrations/005-multi-provider.sql`:

- Add columns: provider (TEXT), model (TEXT), thinking_level (TEXT)
- All columns nullable (agents may not have provider set initially)
- Include TRUNCATE for rel-07 clean slate (developer-only migration)
- Update schema_version to 5
- Include rollback instructions in comments

## Interface

Functions to update:

| Function | Changes |
|----------|---------|
| `res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent)` | Add provider, model, thinking_level to INSERT statement |
| `res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out)` | Add provider, model, thinking_level to SELECT statement |
| `res_t ik_db_agent_list_running(...)` | Add provider, model, thinking_level to SELECT statement |
| `res_t ik_db_agent_get_children(...)` | Add provider, model, thinking_level to SELECT statement |
| `res_t ik_db_agent_get_parent(...)` | Add provider, model, thinking_level to SELECT statement |

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx, const char *uuid, const char *provider, const char *model, const char *thinking_level)` | Update agent provider configuration (for /model command) |

Structs to update:

| Struct | New Members | Purpose |
|--------|-------------|---------|
| `ik_db_agent_row_t` | provider (char*), model (char*), thinking_level (char*) | Store agent's provider configuration |

Files to create:

- `migrations/005-multi-provider.sql` - Schema migration

Files to update:

- `src/db/agent.h` - Add fields to `ik_db_agent_row_t`
- `src/db/agent.c` - Update INSERT, SELECT, add UPDATE function

## Behaviors

### Migration Application
- Add three new TEXT columns to agents table
- All columns nullable (NULL means not configured)
- TRUNCATE all tables for rel-07 clean slate
- Update schema_version from 4 to 5
- Rollback instructions in SQL comments

### INSERT Operations
- Include provider, model, thinking_level in INSERT statement
- Values from `agent->provider`, `agent->model`, `agent->thinking_level`
- Pass NULL if agent fields are NULL (allowed)
- No validation at database layer (application validates)

### SELECT Operations
- Include provider, model, thinking_level in all SELECT statements
- Check `PQgetisnull()` before accessing column values
- Allocate strings on row's talloc context
- Leave row fields NULL if database column is NULL

### UPDATE Operation
- New function: `ik_db_agent_update_provider()`
- Updates all three fields atomically
- Accepts NULL values (clears configuration)
- Returns OK if agent not found (UPDATE affects 0 rows)
- Returns ERR_DB_CONNECT on database error

### Message JSONB Format
- No schema changes to messages table
- JSONB data column is flexible
- Callers populate with provider metadata:
  - `provider`, `model`, `thinking_level`
  - `thinking`, `thinking_tokens`
  - `input_tokens`, `output_tokens`, `total_tokens`
  - `provider_data` (provider-specific metadata)
- Documented format, not enforced by schema

### Error Handling
- NULL db_ctx: assert (precondition)
- NULL uuid in update: assert (precondition)
- Database connection error: return ERR_DB_CONNECT
- Malformed result: return ERR_DB_CONNECT
- Agent not found: UPDATE affects 0 rows (not an error)

### Memory Management
- Row struct allocated on provided context
- All string fields allocated on row's context
- NULL-safe field access
- Caller owns row and must free it

## Migration SQL Structure

Key elements to include:

- `BEGIN;` transaction wrapper
- `ALTER TABLE agents ADD COLUMN IF NOT EXISTS ...` for each column
- `TRUNCATE TABLE agents CASCADE;` (clean slate)
- `TRUNCATE TABLE messages CASCADE;`
- `TRUNCATE TABLE mail CASCADE;`
- `TRUNCATE TABLE sessions CASCADE;`
- `UPDATE schema_metadata SET schema_version = 5 WHERE schema_version = 4;`
- `COMMIT;`
- Rollback instructions in comments

## Test Scenarios

### Migration Tests
- Apply migration: succeeds
- Schema version updated to 5
- Columns exist in agents table
- TRUNCATE cleared all tables
- Can rollback manually via commented SQL

### CRUD Tests
- Insert agent with NULL provider: succeeds
- Insert agent with provider values: succeeds
- Get agent: loads provider fields correctly
- Get agent with NULL provider: row has NULL fields
- Update provider: changes values
- Update provider to NULL: clears values
- List running agents: includes provider fields

### Backward Compatibility
- Old agents (pre-migration): have NULL provider fields
- New agents: have provider fields populated
- No errors on NULL values
- Application handles NULL gracefully

## Postconditions

- [ ] `migrations/005-multi-provider.sql` exists and is valid SQL
- [ ] Migration applies successfully: `make db-migrate` succeeds
- [ ] Schema version is 5: `SELECT schema_version FROM schema_metadata` returns 5
- [ ] Columns exist: `\d agents` shows provider, model, thinking_level
- [ ] `ik_db_agent_row_t` has provider, model, thinking_level fields
- [ ] `ik_db_agent_insert()` includes new columns
- [ ] `ik_db_agent_get()` loads new columns
- [ ] `ik_db_agent_update_provider()` exists and works
- [ ] All existing agent tests pass
- [ ] `make check` passes
