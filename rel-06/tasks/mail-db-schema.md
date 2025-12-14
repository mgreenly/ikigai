# Task: Mail Database Schema

## Target
User Stories: 15-send-mail.md, 16-check-mail.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Mailbox section)

## Pre-read Source (READ - for patterns)
- migrations/001-initial-schema.sql (pattern for migration structure)
- src/db/migration.h (migration constants and functions)

## Files to CREATE
- migrations/004-mail-table.sql (new migration)
- tests/unit/db/mail_schema_test.c (new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- mail-msg-struct.md task complete
- messages-agent-uuid.md task complete (provides migration 003)

## Task
Create PostgreSQL migration for the mail table.

**Schema:**
```sql
CREATE TABLE mail (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    from_uuid TEXT NOT NULL,
    to_uuid TEXT NOT NULL,
    body TEXT NOT NULL,
    timestamp BIGINT NOT NULL,
    read INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_mail_recipient ON mail(session_id, to_uuid, read);
```

**Design decisions:**
- session_id scopes mail to current session
- CASCADE delete removes mail when session deleted
- Index optimizes inbox queries

## TDD Cycle

### Red
1. Create `tests/unit/db/mail_schema_test.c`:
   - Test mail table exists
   - Test all columns exist with correct types
   - Test foreign key to sessions
   - Test index exists
   - Test migration is idempotent

2. Run `make check` - expect test failures

### Green
1. Create `migrations/004-mail-table.sql`:
   ```sql
   BEGIN;

   CREATE TABLE IF NOT EXISTS mail (
       id BIGSERIAL PRIMARY KEY,
       session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
       from_uuid TEXT NOT NULL,
       to_uuid TEXT NOT NULL,
       body TEXT NOT NULL,
       timestamp BIGINT NOT NULL,
       read INTEGER NOT NULL DEFAULT 0
   );

   CREATE INDEX IF NOT EXISTS idx_mail_recipient
   ON mail(session_id, to_uuid, read);

   UPDATE schema_metadata SET schema_version = 4 WHERE schema_version = 3;

   COMMIT;
   ```

2. Run `make check` - expect pass

### Refactor
1. Verify index supports expected query patterns
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- mail table created
- Index for efficient inbox queries
- Working tree is clean (all changes committed)
