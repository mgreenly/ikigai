# Task: Implement ik_agent_restore() function

## Target
Gap 1: Startup Agent Restoration Loop (PR2)

## Macro Context

This task is part of **Gap 1: Startup Agent Restoration** - the critical feature that enables multi-agent sessions to persist across restarts.

**Why this function is needed:**
- On startup, ikigai must restore agents from the database that were running when it last exited
- The database contains agent records with status='running' that need to be reconstructed as `ik_agent_ctx_t` contexts
- Without this, only Agent 0 survives restarts; forked agents are lost

**How this differs from `ik_agent_create()`:**

| Aspect | `ik_agent_create()` | `ik_agent_restore()` |
|--------|---------------------|----------------------|
| UUID | Generates new UUID | Uses `row->uuid` from DB |
| created_at | Sets to `time(NULL)` | Uses `row->created_at` from DB |
| fork_message_id | Defaults to 0 | Parses from `row->fork_message_id` string |
| name | NULL (unnamed) | Uses `row->name` if present |
| parent_uuid | From parameter | Uses `row->parent_uuid` from DB |
| DB registration | Calls `ik_db_agent_insert()` (or would, if wired) | Does NOT register - agent already exists in DB |
| All other init | Standard init | Same as `ik_agent_create()` |

**Relationship to other Gap 1 tasks:**
- This task creates the restore function
- Later task (`gap1-restore-agents-loop.md`) will call this function in a loop for all running agents
- Later task will also handle conversation replay via `ik_agent_replay_history()`

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- rel-06/gap.md (Gap 1 section, specifically the "Proposed Fix" for `ik_agent_restore()`)

## Pre-read Source
- src/agent.h (READ - `ik_agent_ctx_t` struct definition, existing function declarations)
- src/agent.c (READ - `ik_agent_create()` implementation to understand initialization pattern)
- src/db/agent.h (READ - `ik_db_agent_row_t` struct definition)

## Source Files to MODIFY
- src/agent.h (MODIFY - add `ik_agent_restore()` declaration)
- src/agent.c (MODIFY - add `ik_agent_restore()` implementation)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 0 tasks completed (message type unification complete)

## Task

### Part 1: Add declaration to agent.h

Add the function declaration after `ik_agent_create()`:

```c
/**
 * Restore agent context from database row
 *
 * Creates an agent context populated with data from a DB row.
 * Used during startup to restore agents that were running when
 * the process last exited.
 *
 * Unlike ik_agent_create():
 * - Uses row->uuid instead of generating new UUID
 * - Sets fork_message_id from row
 * - Sets created_at from row
 * - Sets name from row (if present)
 * - Sets parent_uuid from row
 * - Does NOT register agent in database (already exists)
 *
 * @param ctx Talloc parent (repl_ctx)
 * @param shared Shared infrastructure
 * @param row Database row with agent data (must not be NULL)
 * @param out Receives allocated agent context
 * @return OK on success, ERR on failure
 */
res_t ik_agent_restore(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                       const ik_db_agent_row_t *row, ik_agent_ctx_t **out);
```

**Note:** This requires adding a forward declaration for `ik_db_agent_row_t` or including the header. Check if `src/db/agent.h` is already included in `agent.h`. If not, add a forward declaration:
```c
typedef struct ik_db_agent_row_t ik_db_agent_row_t;
```

### Part 2: Implement ik_agent_restore() in agent.c

The implementation follows the same pattern as `ik_agent_create()` but:

1. **Uses row data for identity fields:**
```c
// Instead of: agent->uuid = ik_generate_uuid(agent);
agent->uuid = talloc_strdup(agent, row->uuid);

// Instead of: agent->name = NULL;
agent->name = row->name ? talloc_strdup(agent, row->name) : NULL;

// Instead of: agent->parent_uuid = parent_uuid ? talloc_strdup(agent, parent_uuid) : NULL;
agent->parent_uuid = row->parent_uuid ? talloc_strdup(agent, row->parent_uuid) : NULL;

// Instead of: agent->created_at = time(NULL);
agent->created_at = row->created_at;

// Parse fork_message_id from string to int64_t
agent->fork_message_id = row->fork_message_id ? strtoll(row->fork_message_id, NULL, 10) : 0;
```

2. **Does NOT call ik_db_agent_insert()** - the agent already exists in the DB

3. **All other initialization is identical to ik_agent_create():**
   - Display state (scrollback, layer_cake, layers)
   - Input state (input_buffer, visibility flags)
   - Conversation state (conversation, marks)
   - LLM interaction state (multi, curl_still_running, state, etc.)
   - Tool execution state (pending_tool_call, thread state, mutex)

### Part 3: Handle required includes

In agent.c, add include if needed:
```c
#include "db/agent.h"  // for ik_db_agent_row_t
```

### Implementation Notes

1. **Consider extracting common initialization:**
   - Both `ik_agent_create()` and `ik_agent_restore()` share most initialization
   - Option A: Duplicate the code (simpler, explicit)
   - Option B: Extract helper `init_agent_common()` (DRY, but adds indirection)
   - **Recommendation:** Start with Option A (duplication). Refactor to Option B only if there are issues.

2. **Error handling pattern:**
   - Follow the same pattern as `ik_agent_create()`
   - On mutex init failure, free agent and return ERR
   - Use PANIC for out-of-memory (consistent with codebase)

3. **Required assertions:**
```c
assert(ctx != NULL);
assert(shared != NULL);
assert(row != NULL);
assert(row->uuid != NULL);  // UUID is required
assert(out != NULL);
```

## TDD Cycle

### Red
1. Run `make` to verify clean starting state
2. Add declaration to agent.h
3. Run `make` - should fail (function declared but not implemented)

### Green
1. Implement `ik_agent_restore()` in agent.c following the pattern from `ik_agent_create()`
2. Run `make` - should compile successfully
3. Run `make check` - all existing tests should pass (new function not yet tested)
4. Run `make lint` - should pass

### Refactor
1. Review for code duplication between `ik_agent_create()` and `ik_agent_restore()`
2. If significant duplication, consider extracting common initialization
3. Ensure consistent error handling and assertions

## Unit Tests

Create basic tests for `ik_agent_restore()`:

### Test file location
- Add tests to existing `tests/unit/agent_test.c` if it exists
- Or create new `tests/unit/agent_test.c`

### Test cases
1. **test_agent_restore_basic** - restore with valid row data, verify all fields populated correctly
2. **test_agent_restore_null_name** - restore with row->name == NULL, verify agent->name is NULL
3. **test_agent_restore_null_parent** - restore root agent (parent_uuid == NULL)
4. **test_agent_restore_fork_message_id** - verify fork_message_id parsed correctly from string

### Test structure
```c
static void test_agent_restore_basic(void **state)
{
    TALLOC_CTX *tmp = talloc_new(NULL);

    // Create mock shared context
    ik_shared_ctx_t *shared = create_test_shared_ctx(tmp);

    // Create mock DB row
    ik_db_agent_row_t *row = talloc_zero(tmp, ik_db_agent_row_t);
    row->uuid = "test-uuid-123";
    row->name = "test-agent";
    row->parent_uuid = "parent-uuid-456";
    row->fork_message_id = "42";
    row->created_at = 1234567890;
    row->status = "running";

    // Call restore
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(tmp, shared, row, &agent);

    // Verify
    assert_true(is_ok(&res));
    assert_non_null(agent);
    assert_string_equal(agent->uuid, "test-uuid-123");
    assert_string_equal(agent->name, "test-agent");
    assert_string_equal(agent->parent_uuid, "parent-uuid-456");
    assert_int_equal(agent->fork_message_id, 42);
    assert_int_equal(agent->created_at, 1234567890);

    talloc_free(tmp);
}
```

## Sub-agent Usage
- Use haiku sub-agents to run `make` and `make check` for quick feedback
- If you need to search for patterns across multiple files (e.g., finding all places that call `ik_agent_create()`), use a sub-agent

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- `ik_agent_restore()` declared in src/agent.h
- `ik_agent_restore()` implemented in src/agent.c
- Function populates agent from DB row data (uuid, name, parent_uuid, fork_message_id, created_at)
- Function does NOT call database registration functions
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different include strategy, additional error cases, test structure changes), document the deviation and reasoning in your final report.
