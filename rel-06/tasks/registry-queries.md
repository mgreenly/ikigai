# Task: Agent Registry Query Functions

## Target
User Stories: 01-agent-registry-persists.md, 19-agents-tree.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Agent Registry, Startup Replay sections)

## Pre-read Source (patterns)
- src/db/message.c (SELECT patterns, query function examples)

## Files to Create/Modify
- src/db/agent.h (add query function declarations)
- src/db/agent.c (implement query functions)
- tests/unit/db/agent_registry_test.c (add query tests)

## Pre-read Tests (patterns)
- tests/unit/db/agent_registry_test.c (existing tests from prior tasks)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- registry-status-update.md complete

## Task
Create query functions for the agent registry: lookup by UUID, list running agents, get children.

**API:**
```c
// Lookup agent by UUID
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                      const char *uuid, ik_db_agent_row_t **out);

// List all running agents
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               ik_db_agent_row_t ***out, size_t *count);

// Get children of an agent
res_t ik_db_agent_get_children(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               const char *parent_uuid,
                               ik_db_agent_row_t ***out, size_t *count);

// Get parent agent (for walking ancestry chain)
res_t ik_db_agent_get_parent(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                              const char *uuid, ik_db_agent_row_t **out);
```

**Row structure:**
```c
typedef struct {
    char *uuid;
    char *name;
    char *parent_uuid;
    char *fork_message_id;
    char *status;
    int64_t created_at;
    int64_t ended_at;  // 0 if still running
} ik_db_agent_row_t;
```

## TDD Cycle

### Red
1. Add declarations to `src/db/agent.h`

2. Add tests to `tests/unit/db/agent_registry_test.c`:
   - Test get returns correct row for existing UUID
   - Test get returns error for non-existent UUID
   - Test list_running returns only status='running' agents
   - Test list_running excludes dead agents
   - Test get_children returns children ordered by created_at
   - Test get_children returns empty for agent with no children
   - Test get_parent returns parent row for child agent
   - Test get_parent returns NULL (via out param) for root agent (no parent)
   - Test get_parent allows iterative chain walking

3. Run `make check` - expect test failures

### Green
1. Implement functions in `src/db/agent.c`

2. Run `make check` - expect pass

### Refactor
1. Verify memory management (rows allocated on provided mem_ctx)
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- Query functions exist for lookup, list, children
- Results allocated on provided talloc context
- Working tree is clean (all changes committed)
