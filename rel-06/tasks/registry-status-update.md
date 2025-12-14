# Task: Update Agent Status in Registry

## Target
User Story: 02-agent-status-tracked.md

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

## Pre-read Source (patterns)
- src/db/session.c (UPDATE patterns)

## Source Files to Create
- src/db/agent.h (agent database API - new file)
- src/db/agent.c (agent database implementation - new file)

## Test Files to Create
- tests/unit/db/agent_registry_test.c (new test file for agent registry functions)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- registry-create.md complete (ik_db_agent_insert exists)

## Task
Create function to update agent status (running → dead). Called when killing an agent.

**API:**
```c
// Mark agent as dead
// db_ctx: database context
// uuid: agent UUID to update
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid);
```

**SQL:**
```sql
UPDATE agents
SET status = 'dead', ended_at = $1
WHERE uuid = $2 AND status = 'running'
```

## TDD Cycle

### Red
1. Add declaration to `src/db/agent.h`:
   ```c
   res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid);
   ```

2. Add tests to `tests/unit/db/agent_registry_test.c`:
   - Test mark_dead updates status to 'dead'
   - Test mark_dead sets ended_at timestamp
   - Test mark_dead on already-dead agent is no-op (idempotent)
   - Test mark_dead on non-existent uuid returns error

3. Run `make check` - expect test failures

### Green
1. Add to `src/db/agent.c`:
   ```c
   res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid)
   {
       assert(db_ctx != NULL);
       assert(uuid != NULL);

       const char *query =
           "UPDATE agents SET status = 'dead', ended_at = $1 "
           "WHERE uuid = $2 AND status = 'running'";

       char ended_at_str[32];
       snprintf(ended_at_str, sizeof(ended_at_str), "%" PRId64, time(NULL));

       const char *params[] = { ended_at_str, uuid };
       // Execute...
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Verify idempotent behavior (marking dead twice is safe)
2. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_db_agent_mark_dead()` function exists
- Status transitions to 'dead' with ended_at timestamp
- Operation is idempotent
- Working tree is clean (all changes committed)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase
