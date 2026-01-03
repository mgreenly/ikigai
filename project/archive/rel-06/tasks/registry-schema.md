# Task: Create Agent Registry Schema

## Target
User Stories: 01-agent-registry-persists.md, 02-agent-status-tracked.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Agent Registry section)
- project/memory.md (talloc ownership)

## Pre-read Source (patterns)
- migrations/001-initial-schema.sql (existing migration patterns)
- src/db/migration.h (migration interface)
- src/db/migration.c (migration implementation)

## Pre-read Tests (patterns)
- tests/unit/db/session_test.c (database test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Database migration system functional
- No `agents` table exists yet

## Task
Create PostgreSQL migration for the `agents` table. This is the foundation for the agent registry - the source of truth for agent existence.

**Schema (from agent-process-model.md):**
```sql
-- Status enum for type safety
CREATE TYPE agent_status AS ENUM ('running', 'dead');

CREATE TABLE agents (
    uuid          TEXT PRIMARY KEY,
    name          TEXT,
    parent_uuid   TEXT REFERENCES agents(uuid) ON DELETE RESTRICT,
    fork_message_id BIGINT,
    status        agent_status NOT NULL DEFAULT 'running',
    created_at    BIGINT NOT NULL,
    ended_at      BIGINT
);

CREATE INDEX idx_agents_parent ON agents(parent_uuid);
CREATE INDEX idx_agents_status ON agents(status);
```

**Design decisions:**
- `uuid` is TEXT (base64url, 22 chars) - primary key
- `name` is optional (NULL allowed)
- `parent_uuid` self-references for hierarchy with ON DELETE RESTRICT (agents are never deleted, only marked dead)
- `fork_message_id` is BIGINT - tracks where child branched from parent history (matches message ID type)
- `status` is PostgreSQL ENUM ('running', 'dead') - NOT NULL, defaults to 'running'
- `created_at` is BIGINT (Unix epoch) - set at creation
- `ended_at` is BIGINT - NULL when running, set to Unix epoch when killed
- Indexes on parent_uuid (for child queries) and status (for running agents)

**Status semantics (Q20):**
- `running` → agent is alive and active
- `dead` → agent was killed; `ended_at` contains death timestamp
- Death tracking: kill events are recorded in message history (not agents table) with event kind `agent_killed` and metadata `{killed_by: "user" | "<agent_uuid>"}`
- Future: ENUM can be extended via `ALTER TYPE agent_status ADD VALUE 'suspended'` if needed

## TDD Cycle

### Red
1. Create `tests/unit/db/agents_schema_test.c`:
   - Test agent_status ENUM type exists with values ('running', 'dead')
   - Test agents table exists after migration
   - Test all columns exist with correct types
   - Test uuid is PRIMARY KEY
   - Test parent_uuid REFERENCES agents(uuid) ON DELETE RESTRICT
   - Test status is agent_status NOT NULL DEFAULT 'running'
   - Test ended_at allows NULL (for running agents)
   - Test indexes exist (idx_agents_parent, idx_agents_status)
   - Test migration is idempotent

2. Run `make check` - expect test failures

### Green
1. Create `migrations/002-agents-table.sql`:
   ```sql
   BEGIN;

   -- Create status enum type
   DO $$ BEGIN
       CREATE TYPE agent_status AS ENUM ('running', 'dead');
   EXCEPTION
       WHEN duplicate_object THEN NULL;
   END $$;

   CREATE TABLE IF NOT EXISTS agents (
       uuid          TEXT PRIMARY KEY,
       name          TEXT,
       parent_uuid   TEXT REFERENCES agents(uuid) ON DELETE RESTRICT,
       fork_message_id BIGINT,
       status        agent_status NOT NULL DEFAULT 'running',
       created_at    BIGINT NOT NULL,
       ended_at      BIGINT
   );

   CREATE INDEX IF NOT EXISTS idx_agents_parent ON agents(parent_uuid);
   CREATE INDEX IF NOT EXISTS idx_agents_status ON agents(status);

   UPDATE schema_metadata SET schema_version = 2 WHERE schema_version = 1;

   COMMIT;
   ```

2. Run `make check` - expect pass

### Refactor
1. Verify migration follows existing patterns (BEGIN/COMMIT, IF NOT EXISTS)
2. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- Migration file exists at migrations/002-agents-table.sql
- agents table created with all columns and constraints
- Indexes created for efficient queries
- Working tree is clean (all changes committed)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase
