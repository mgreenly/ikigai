# Task: Insert Agent into Registry

## Target
User Story: 01-agent-registry-persists.md

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
- src/db/session.h (CRUD patterns)
- src/db/session.c (INSERT patterns)
- src/agent.h (ik_agent_ctx_t)
- src/agent.c (agent creation)

## Pre-read Tests (patterns)
- tests/unit/db/session_test.c (database operation tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- registry-schema.md complete (agents table exists)
- `ik_agent_ctx_t` has uuid, name, parent_uuid fields (from rel-05)

**Note (from questions.md Q6):** This task adds `created_at` and `fork_message_id` fields to `ik_agent_ctx_t`.

## Task
Create function to insert an agent record into the registry. Called when creating a new agent.

**Key Decisions (from questions.md):**
- `created_at` field in `ik_agent_ctx_t` populated from DB when agent is loaded
- Use `int64_t` Unix timestamp for created_at
- Set `created_at` during insert, store in agent context for use in child selection

**API:**
```c
// Insert agent into registry
// db_ctx: database context
// agent: agent context with uuid, parent_uuid, created_at set
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent);
```

**SQL:**
```sql
INSERT INTO agents (uuid, name, parent_uuid, status, created_at)
VALUES ($1, $2, $3, 'running', $4)
```

## TDD Cycle

### Red
1. Add declaration to `src/db/agent.h`:
   ```c
   res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent);
   ```

2. Create `tests/unit/db/agent_registry_test.c`:
   - Test insert succeeds for root agent (parent_uuid = NULL)
   - Test insert succeeds for child agent
   - Test inserted record has status = 'running'
   - Test inserted record has correct created_at (matches agent->created_at)
   - Test created_at stored in ik_agent_ctx_t for later use
   - Test duplicate uuid fails (PRIMARY KEY violation)

3. Run `make check` - expect test failures

4. Commit: "Red: Add ik_db_agent_insert() tests and declaration"

### Green
1. Add fields to `ik_agent_ctx_t` in `src/agent.h`:
   ```c
   int64_t created_at;       // Unix timestamp when agent was created
   int64_t fork_message_id;  // Message ID at which this agent forked (0 for root)
   ```

2. Create `src/db/agent.c`:
   ```c
   res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent)
   {
       assert(db_ctx != NULL);
       assert(agent != NULL);
       assert(agent->uuid != NULL);

       const char *query =
           "INSERT INTO agents (uuid, name, parent_uuid, status, created_at) "
           "VALUES ($1, $2, $3, 'running', $4)";

       char created_at_str[32];
       snprintf(created_at_str, sizeof(created_at_str), "%" PRId64, time(NULL));

       const char *params[] = {
           agent->uuid,
           agent->name,      // Can be NULL
           agent->parent_uuid, // Can be NULL for root
           created_at_str
       };

       // Execute parameterized query...
   }
   ```

3. Update Makefile to compile agent.c

4. Run `make check` - expect pass

5. Commit: "Green: Implement ik_db_agent_insert() function"

### Refactor
1. Verify error handling for database failures
2. Run `make lint` - verify clean
3. Commit: "Add ik_db_agent_insert() and created_at/fork_message_id fields"

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_agent_ctx_t` has `created_at` and `fork_message_id` fields
- `ik_db_agent_insert()` function exists
- Root and child agents can be inserted
- Status defaults to 'running'
- Working tree is clean (all changes committed)
