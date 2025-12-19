# Task: Startup History Replay

## Target
User Stories: 01-agent-registry-persists.md, 05-fork-inherits-history.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Startup Replay section)
- rel-06/README.md (Phase 5 - Replay Algorithm Detail)

## Pre-read Source (patterns)
- src/db/replay.c (READ - existing replay logic for session-based events)
- src/db/replay.h (READ - existing structures: ik_replay_context_t, ik_message_t)
- src/repl/session_restore.c (READ - session restoration patterns)

## Source Files to CREATE/MODIFY
- src/db/replay.h (MODIFY - add ik_replay_range_t typedef)
- src/db/agent.h (CREATE - new header for agent registry operations)
- src/db/agent.c (CREATE - implement replay range building and queries)
- tests/unit/db/agent_replay_test.c (CREATE - new test file for agent replay)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior task dependencies completed:
  - agent-zero-register.md (Agent 0 registration exists)
  - fork-history-copy.md (fork copies message history)
  - registry-queries.md (agent registry query functions exist)
  - messages-agent-uuid.md (messages.agent_uuid column exists)

## Task
Implement startup replay using the "walk backwards, play forwards" algorithm to reconstruct agent history from the database on application startup.

**Note (from questions.md Q7):** This task does NOT handle Agent 0 creation. That is handled by `agent-zero-register.md` which runs first and ensures Agent 0 exists before replay. This task only replays history for existing running agents.

```c
// Pseudocode for startup (after agent-zero-register.md has run)
running_agents = query_agents_by_status("running");
for each agent in running_agents:
    replay_history(agent);
```

### Core Data Structure

Each range entry contains enough information to query the exact subset of messages:

```c
typedef struct {
    char *agent_uuid;   // Which agent's messages to query
    int64_t start_id;   // Start AFTER this message ID (0 = from beginning)
    int64_t end_id;     // End AT this message ID (0 = no limit, i.e., leaf)
} ik_replay_range_t;
```

**Semantics:**
- `start_id` is **exclusive** (query messages AFTER this ID)
- `end_id` is **inclusive** (query messages up to and including this ID)
- `end_id = 0` means "no upper limit" (used for leaf agent)

### Walk Algorithm (child to root)

```
build_replay_ranges(leaf_agent):
    ranges = []
    current = leaf_agent
    end_id = 0  // leaf has no upper bound

    while current != NULL:
        // Find most recent clear for this agent (within range)
        clear_id = find_most_recent_clear(current.uuid, max_id=end_id)

        if clear_id found:
            // Clear found - start after clear, this terminates the walk
            ranges.append({current.uuid, clear_id, end_id})
            break
        else:
            // No clear - include all messages from beginning
            ranges.append({current.uuid, 0, end_id})

            if current.parent_uuid == NULL:
                break  // Root agent reached, done

            // Move to parent - parent's range ends at this agent's fork point
            end_id = current.fork_message_id
            current = lookup(current.parent_uuid)

    return reverse(ranges)  // Chronological order (root first)
```

**Exit conditions:**
1. **Clear found** - Context boundary reached, stop walking
2. **Root reached** - `parent_uuid == NULL`, no more ancestors

### Query per Range

```sql
SELECT * FROM messages
WHERE agent_uuid = $1
  AND id > $2                    -- after start_id
  AND ($3 = 0 OR id <= $3)       -- up to end_id (0 = no limit)
ORDER BY created_at
```

### Replay Execution

```
replay(ranges):
    context = empty_context()
    for range in ranges:  // Already in chronological order
        messages = query_messages(range.agent_uuid, range.start_id, range.end_id)
        for msg in messages:
            process_event(context, msg)
    return context
```

### API

```c
// Build replay ranges by walking ancestor chain
// Returns array in chronological order (oldest first)
res_t ik_agent_build_replay_ranges(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                                    const char *agent_uuid,
                                    ik_replay_range_t **ranges_out,
                                    size_t *count_out);

// Find most recent clear event for an agent (within message ID limit)
// Returns 0 if no clear found, otherwise the message ID of the clear
res_t ik_agent_find_clear(ik_db_ctx_t *db_ctx, const char *agent_uuid,
                           int64_t max_id, int64_t *clear_id_out);

// Replay history for an agent using range-based algorithm
res_t ik_agent_replay_history(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               const char *agent_uuid,
                               ik_replay_context_t **ctx_out);

// Query messages for a single range
res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                            const ik_replay_range_t *range,
                            ik_message_t ***messages_out,
                            size_t *count_out);
```

## TDD Cycle

### Red
1. Add `ik_replay_range_t` typedef to `src/db/replay.h`

2. Add declarations to `src/db/agent.h`

3. Add tests to `tests/unit/db/agent_replay_test.c`:
   - Test range building for root agent (single range with end_id=0)
   - Test range building for child (two ranges: parent with end_id=fork, child with end_id=0)
   - Test range building for grandchild (three ranges in correct order)
   - Test range building stops at clear event
   - Test find_clear returns correct message ID
   - Test find_clear returns 0 when no clear exists
   - Test find_clear respects max_id limit
   - Test query_range returns correct message subset
   - Test query_range with start_id=0 returns from beginning
   - Test query_range with end_id=0 returns to end
   - Test full replay produces correct chronological order
   - Test replay handles agent with no history
   - Test replay handles deep ancestry (4+ levels)

4. Run `make check` - expect test failures

### Green
1. Implement `ik_agent_find_clear()`:
   - Query messages for kind='clear' with agent_uuid
   - Filter by id <= max_id if max_id > 0
   - Return highest ID found, or 0 if none

2. Implement `ik_agent_build_replay_ranges()`:
   - Start with leaf agent, end_id=0
   - Loop: find_clear, append range, move to parent or break
   - Reverse array before returning

3. Implement `ik_agent_query_range()`:
   - Build parameterized query with agent_uuid, start_id, end_id
   - Handle end_id=0 case (no upper limit)
   - Return message array

4. Implement `ik_agent_replay_history()`:
   - Call build_replay_ranges
   - For each range: query_range, process events
   - Return replay context

5. Run `make check` - expect pass

### Refactor
1. Consider caching replay ranges for frequently accessed agents
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (Red, Green, Refactor)

## Post-conditions
- `make check` passes
- `make lint` passes
- Range building walks full chain to clear event or root
- Query correctly handles start_id/end_id boundaries
- Replay produces complete history in correct order
- Grandchild agents can access grandparent's context
- Working tree is clean (all changes committed)
