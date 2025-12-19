# Task: Agent 0 Registration

## Target
User Stories: rel-06/user-stories/01-agent-registry-persists.md

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

## Pre-read Source (patterns - READ for context)
- src/repl_init.c (initialization flow)
- src/agent.h (ik_agent_ctx_t)

## Files to CREATE/MODIFY
- CREATE: src/db/agent.h (add ik_db_ensure_agent_zero declaration)
- CREATE: src/db/agent.c (implement ik_db_ensure_agent_zero)
- MODIFY: src/repl_init.c (call ik_db_ensure_agent_zero during init)
- CREATE: tests/unit/db/agent_zero_test.c (new test file)

## Missing Dependencies
NOTE: The following files are referenced but do NOT currently exist in the codebase:
- src/db/agent.h - will be CREATED by this task
- src/db/agent.c - will be CREATED by this task
- src/uuid.h - should exist from uuid-refactor.md pre-condition (CHECK this)
- tests/unit/db/agent_registry_test.c - should exist from registry-queries.md pre-condition

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior task registry-queries.md complete (agent registry CRUD exists)
- Prior task uuid-refactor.md complete (ik_generate_uuid exists)
- Database schema includes agents table with columns: uuid, parent_uuid, status, created_at
- Database schema includes messages table with agent_uuid column

## Task
Register Agent 0 on startup before the event loop. This ensures the root agent exists in the registry before any fork operations.

**Key Decisions (from questions.md):**
- Registration happens during `ik_repl_init()` (before event loop)
- Generate random UUID on first run, persist it
- **Migration** handles upgrade: if `messages` table has rows but registry empty -> create Agent 0 -> `UPDATE messages SET agent_uuid = $1 WHERE agent_uuid IS NULL`
- **Init** handles fresh install: if registry empty -> create Agent 0
- **Agent 0 is unkillable** - `/kill` on Agent 0 returns error (enforced in kill-cmd-self.md via parent_uuid == NULL check)

**API:**
```c
// Ensure Agent 0 exists in registry
// Creates if missing, retrieves UUID if present
// Called once during ik_repl_init()
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);
```

**Flow - Fresh Install:**
1. Query registry: `SELECT uuid FROM agents WHERE parent_uuid IS NULL`
2. If no root agent found:
   - Generate new UUID (base64url, 22 chars)
   - Insert into registry with `parent_uuid = NULL, status = 'running'`
3. Store UUID in `repl->current->uuid`

**Flow - Upgrade (messages exist, no agents):**
1. Query registry: `SELECT uuid FROM agents WHERE parent_uuid IS NULL`
2. If no root agent AND messages table has rows:
   - Generate new UUID
   - Insert Agent 0 into registry
   - `UPDATE messages SET agent_uuid = $1 WHERE agent_uuid IS NULL`
   - This retroactively claims all orphan messages for Agent 0
3. Store UUID in `repl->current->uuid`

## TDD Cycle

### Red
1. Add declaration to `src/db/agent.h`:
   ```c
   res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);
   ```

2. Create tests in `tests/unit/db/agent_zero_test.c`:
   - Test creates Agent 0 on empty registry
   - Test returns existing Agent 0 UUID if present
   - Test Agent 0 has parent_uuid = NULL
   - Test Agent 0 has status = 'running'
   - Test upgrade: adopts orphan messages
   - Test idempotent (multiple calls return same UUID)

3. Run `make check` - expect test failures

### Green
1. Implement in `src/db/agent.c`:
   ```c
   res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid)
   {
       assert(db != NULL);
       assert(out_uuid != NULL);

       // Check for existing root agent
       const char *query = "SELECT uuid FROM agents WHERE parent_uuid IS NULL";
       // ... execute query ...

       if (/* root found */) {
           *out_uuid = talloc_strdup(db, existing_uuid);
           return OK(*out_uuid);
       }

       // Generate new UUID
       char *uuid = ik_generate_uuid(db);

       // Check for orphan messages (upgrade scenario)
       bool has_orphans = /* SELECT 1 FROM messages WHERE agent_uuid IS NULL LIMIT 1 */;

       // Begin transaction
       CHECK(ik_db_begin(db));

       // Insert Agent 0
       CHECK(ik_db_agent_insert_raw(db, uuid, NULL, NULL, "running", time(NULL)));

       // Adopt orphans if any
       if (has_orphans) {
           const char *adopt = "UPDATE messages SET agent_uuid = $1 WHERE agent_uuid IS NULL";
           CHECK(ik_db_exec(db, adopt, uuid));
       }

       CHECK(ik_db_commit(db));

       *out_uuid = uuid;
       return OK(*out_uuid);
   }
   ```

2. Call from `ik_repl_init()`:
   ```c
   char *agent_zero_uuid = NULL;
   CHECK(ik_db_ensure_agent_zero(repl->shared->db, &agent_zero_uuid));
   repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);
   ```

3. Run `make check` - expect pass

### Refactor
1. Verify UUID generation is cryptographically random
2. Verify transaction rollback on any failure
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes (verify with haiku sub-agent)
- `make lint` passes (verify with haiku sub-agent)
- Agent 0 is registered on startup
- Existing installations get Agent 0 with orphan message adoption
- Fresh installations get Agent 0 in registry
- `repl->current->uuid` is populated before event loop
- Working tree is clean (all changes committed via haiku sub-agent)

## Sub-agent Usage
Use haiku sub-agents for the following tasks to save tokens:
- Running `make check` verification after each TDD phase (Red, Green, Refactor)
- Running `make lint` verification during Refactor phase
- Creating git commits after completing each TDD phase
- The haiku model is sufficient for these mechanical verification and commit tasks
