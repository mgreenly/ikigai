# Agent Implementation Gaps

This document records gaps discovered between the rel-06 design specifications and the actual implementation.

## Summary

The multi-agent system's core feature - **agent persistence across sessions** - has been fully implemented. Gap 0 (Message Type Unification), Gap 1 (Startup Agent Restoration Loop), and Gap 5 (Session Restore Migration) are now complete. Multi-agent sessions now persist correctly across restarts with proper parent-child hierarchy restoration.

Remaining gaps include separator navigation context and cross-component consistency improvements.

---

## PREREQUISITE: Type System Cleanup

### Gap 0: Message Type Unification (PREREQUISITE)

**Status:** Incomplete from rel-04 - must be fixed before Gap 1

#### Background

rel-04 intended to create a single canonical message type. The CHANGELOG states:
> "Architecture docs: Canonical message format and extended thinking documentation"

And client.h comment claims:
> "This type is shared across all modules for database storage, in-memory representation, and rendering to scrollback."

#### What Was Intended

1. `ik_msg_t` = single canonical internal format
2. Used everywhere: DB queries, replay context, conversation, serialization
3. OpenAI serialization transforms `ik_msg_t` directly to API JSON

#### What Actually Happened

The unification stopped halfway:

| Type | Fields | Location | Used By |
|------|--------|----------|---------|
| `ik_msg_t` | `kind`, `content`, `data_json` | msg.h | conversation, serialization |
| `ik_message_t` | `id`, `kind`, `content`, `data_json` | db/replay.h | DB queries, replay context |

- `ik_openai_msg_t` → `ik_msg_t` unification completed ✓
- `ik_message_t` → `ik_msg_t` unification **NOT completed** ✗
- `ik_msg_from_db()` conversion function added as workaround

#### Why This Blocks Gap 1

Gap 1 (Startup Restoration) needs to:
1. Call `ik_agent_replay_history()` → returns `ik_replay_context_t`
2. `ik_replay_context_t` contains `ik_message_t**`
3. Populate `agent->conversation` which uses `ik_msg_t**`
4. Currently requires conversion via `ik_msg_from_db()`

If we implement Gap 1 without fixing this:
- Write conversion code throughout restoration logic
- Then unify types later
- Rewrite/remove the conversion code

#### Proposed Fix

**Step 1: Add `id` field to `ik_msg_t`**

```c
// msg.h - canonical internal format (MODIFIED)
typedef struct {
    int64_t id;       // NEW: DB row ID (0 if not from DB)
    char *kind;
    char *content;
    char *data_json;
} ik_msg_t;
```

**Step 2: Update `ik_replay_context_t` to use `ik_msg_t**`**

```c
// db/replay.h (MODIFIED)
typedef struct {
    ik_msg_t **messages;              // CHANGED from ik_message_t**
    size_t count;
    size_t capacity;
    ik_replay_mark_stack_t mark_stack;
} ik_replay_context_t;
```

**Step 3: Update DB query functions to populate `ik_msg_t` directly**

Files to modify:
- `src/db/replay.c` - `ik_db_messages_load()` returns `ik_msg_t**`
- `src/db/agent_replay.c` - `ik_agent_query_range()` returns `ik_msg_t**`

**Step 4: Remove obsolete code**

Files to DELETE:
- None (no files deleted entirely)

Code to REMOVE from files:
- `src/msg.h`: Remove `ik_msg_from_db()` declaration
- `src/msg.c`: Remove `ik_msg_from_db()` implementation (entire function ~45 lines)
- `src/db/replay.h`: Remove `ik_message_t` typedef (lines 15-21)
- `src/repl/session_restore.c`: Remove `ik_msg_from_db_()` calls, use messages directly

**Step 5: Update all `ik_message_t` references**

Search and replace throughout codebase:
```bash
grep -r "ik_message_t" src/
```

Each occurrence changes to `ik_msg_t`.

#### Sentinel Value for `id`

- `id = 0`: Message not from DB (created in-memory, not yet persisted)
- `id > 0`: Valid DB row ID

This matches PostgreSQL SERIAL behavior where IDs start at 1.

#### Files to Modify (Complete List)

1. `src/msg.h` - Add `id` field, remove `ik_msg_from_db()` declaration
2. `src/msg.c` - Remove `ik_msg_from_db()` function entirely
3. `src/db/replay.h` - Remove `ik_message_t`, update `ik_replay_context_t`
4. `src/db/replay.c` - Update to populate `ik_msg_t` with `id`
5. `src/db/agent_replay.c` - Update to use `ik_msg_t`
6. `src/db/agent_replay.h` - Update function signatures
7. `src/repl/session_restore.c` - Remove conversion, use `ik_msg_t` directly
8. `src/openai/client_msg.c` - Initialize `id = 0` when creating messages
9. `tests/unit/db/replay_test.c` - Update for new type
10. `tests/unit/db/agent_replay_test.c` - Update for new type
11. `tests/unit/msg_test.c` - Remove `ik_msg_from_db` tests, add `id` field tests

#### Testing

1. All existing replay tests pass with `ik_msg_t`
2. `id` field populated correctly from DB queries
3. `id = 0` for in-memory created messages
4. Session restore works without conversion
5. Serialization ignores `id` field (no wire format change)

#### Impact

After this fix:
- Single message type throughout codebase
- No conversion overhead
- Gap 1 implementation is cleaner
- Gap 5 becomes trivial (no conversion needed)

---

## CRITICAL: Core Persistence Gap

### Gap 1: Startup Agent Restoration Loop (CRITICAL)

**Depends on:** Gap 0 (Message Type Unification)

**Status:** COMPLETE

#### What Was Specified

**rel-06/README.md Phase 5 (lines 90-100):**
> Reconstruct state from database on restart.
> - Query registry for `status = 'running'` agents
> - For each agent, walk ancestor chain backwards to find clear event
> - Resume where left off

**rel-06/user-stories/01-agent-registry-persists.md (lines 35-37):**
> 6. On restart, ikigai queries `agents` table for `status = 'running'`
> 7. For each running agent, reconstruct `ik_agent_ctx_t` from registry + history

**project/agent-process-model.md (lines 139-145):**
> On ikigai startup:
> 1. Query agent registry for all `status = 'running'` agents
> 2. For each agent, replay history from fork point
> 3. Reconstruct parent-child relationships from registry
> 4. Agents resume where they left off

**rel-06/tasks/startup-replay.md (lines 46-50):**
```c
// Pseudocode for startup (after agent-zero-register.md has run)
running_agents = query_agents_by_status("running");
for each agent in running_agents:
    replay_history(agent);
```

#### What Was Implemented

The building blocks exist in `src/db/agent_replay.c`:
- `ik_agent_find_clear()` - finds most recent clear event
- `ik_agent_build_replay_ranges()` - walks ancestor chain, builds ranges
- `ik_agent_query_range()` - queries messages for a range
- `ik_agent_replay_history()` - full replay algorithm

Supporting functions exist:
- `ik_db_agent_list_running()` - queries all running agents from DB
- `ik_agent_create()` - creates agent context
- `ik_repl_add_agent()` - adds agent to repl->agents[] array

#### What Is Missing

The startup loop in `src/repl_init.c` that:
1. Queries all running agents from DB (not just Agent 0)
2. Creates `ik_agent_ctx_t` for each
3. Calls `ik_agent_replay_history()` for each
4. Populates conversation and scrollback from replay context
5. Adds each to `repl->agents[]`
6. Rebuilds parent-child hierarchy

#### Current Behavior (repl_init.c lines 102-120)

```c
// Ensure Agent 0 exists in registry if database is configured
if (shared->db_ctx != NULL) {
    char *agent_zero_uuid = NULL;
    result = ik_db_ensure_agent_zero(shared->db_ctx, &agent_zero_uuid);
    // ... only Agent 0 is loaded
    repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);
}

// Restore session if database is configured
if (shared->db_ctx != NULL) {
    result = ik_repl_restore_session_(repl, shared->db_ctx, cfg);
    // ... uses old session-based replay, not agent-based
}
```

#### Root Cause

Task `startup-replay.md` Files to CREATE/MODIFY section:
```
- src/db/replay.h (MODIFY)
- src/db/agent.h (CREATE)
- src/db/agent.c (CREATE)
- tests/unit/db/agent_replay_test.c (CREATE)
```

Missing from spec:
```
- src/repl_init.c (MODIFY - should have been listed)
```

The sub-agent implemented exactly what the task specified. The task specification was incomplete.

#### Impact

- Multi-agent sessions don't persist across restarts
- Only Agent 0 survives
- Forked agents are lost (remain as orphans in DB with status='running')

#### Proposed Fix

**New function: `ik_agent_restore()`** (distinct from `ik_agent_create()`)

```c
// src/agent.c - restore agent from DB row (does NOT register in DB)
res_t ik_agent_restore(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                       const ik_db_agent_row_t *row,
                       ik_agent_ctx_t **out)
{
    // Same initialization as ik_agent_create() but:
    // - Uses row->uuid instead of generating new UUID
    // - Sets fork_message_id, created_at, name from row
    // - Does NOT call ik_db_agent_register()
}
```

**New function: `ik_repl_restore_agents()`**

```c
// src/repl_init.c or src/repl/agent_restore.c
res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx)
{
    // 1. Query all running agents
    ik_db_agent_row_t **agents;
    size_t count;
    CHECK(ik_db_agent_list_running(db_ctx, tmp, &agents, &count));

    // 2. Sort by created_at (oldest first) - parents before children
    qsort(agents, count, sizeof(ik_db_agent_row_t *), compare_by_created_at);

    // 3. For each agent:
    for (size_t i = 0; i < count; i++) {
        // Skip Agent 0 (already created)
        if (agents[i]->parent_uuid == NULL) continue;

        // Restore agent context from DB row
        ik_agent_ctx_t *agent;
        CHECK(ik_agent_restore(repl, repl->shared, agents[i], &agent));

        // Replay history (returns ik_msg_t** after Gap 0 unification)
        ik_replay_context_t *replay_ctx;
        CHECK(ik_agent_replay_history(db_ctx, agent, agent->uuid, &replay_ctx));

        // Populate conversation directly (no conversion needed after Gap 0)
        for (size_t j = 0; j < replay_ctx->count; j++) {
            ik_msg_t *msg = replay_ctx->messages[j];
            // Skip non-conversation kinds (clear, mark, rewind)
            if (is_conversation_kind(msg->kind)) {
                ik_openai_conversation_add_msg(agent->conversation, msg);
            }
        }

        // Populate scrollback from replay
        for (size_t j = 0; j < replay_ctx->count; j++) {
            ik_msg_t *msg = replay_ctx->messages[j];
            ik_event_render(agent->scrollback, msg->kind, msg->content, msg->data_json);
        }

        // Restore marks from replay context
        // ... copy replay_ctx->mark_stack to agent->marks ...

        // Add to array
        CHECK(ik_repl_add_agent(repl, agent));
    }

    return OK(NULL);
}
```

**Error handling:** If an agent fails to restore, mark it 'dead' in DB, log warning, continue with others.

Call from `ik_repl_init()` after Agent 0 creation, before session restore.

---

## Data Source Consistency Gaps

### Gap 2: /agents and /kill Data Source Mismatch

**Status:** Resolves automatically with Gap 1 fix

#### Symptom

```
> /agents
Agent Hierarchy:

* wlT4G-jnTXaaLmMzn8maZg (running) - root
  +-- Mg23MVijSiWdBRdY6V7Uqw (running)

2 running, 0 dead

> /kill Mg23MVijSiWdBRdY6V7Uqw
Error: Agent not found
```

#### Analysis

**`/agents` command (commands_agent_list.c:51):**
```c
res = ik_db_agent_list_running(repl->shared->db_ctx, tmp_ctx, &all_agents, &all_count);
// Queries DATABASE - includes orphans from previous sessions
```

**`/kill` command (commands_agent.c:350):**
```c
ik_agent_ctx_t *target = ik_repl_find_agent(repl, uuid_arg);
// Searches IN-MEMORY array - only current session agents
```

#### Root Cause

After a restart, only Agent 0 is in memory. But the database still has agents from previous sessions marked as `status='running'` (because they were never killed before exit, and no cleanup happens on shutdown/startup).

#### Resolution

This gap resolves automatically once Gap 1 is fixed - all running agents will be loaded into memory on startup.

---

### Gap 3: Mail Commands Also Use In-Memory Lookup

**Status:** Resolves automatically with Gap 1 fix

#### Analysis

**`/send` command (commands_mail.c:99):**
```c
ik_agent_ctx_t *recipient = ik_repl_find_agent(repl, uuid);
if (recipient == NULL) {
    const char *err = "Agent not found";
    // ...
}
```

**`/filter-mail` command (commands_mail.c:392):**
```c
ik_agent_ctx_t *sender = ik_repl_find_agent(repl, uuid_arg);
if (sender == NULL) {
    // ...
}
```

#### Impact

- Can only send mail to agents in memory (current session)
- `/agents` shows agents from DB that mail commands can't target
- Confusing UX: "I see the agent but can't message it"

#### Resolution

Resolves with Gap 1 fix - all running agents loaded at startup.

---

### Gap 4: Navigation Functions Consistent With /kill (Not /agents)

**Status:** Consistent by design, but documented for completeness

#### Analysis

Navigation functions in `src/repl.c`:
- `ik_repl_nav_parent()` - uses `ik_repl_find_agent()` (in-memory)
- `ik_repl_nav_child()` - iterates `repl->agents[]` (in-memory)
- `ik_repl_nav_prev_sibling()` - iterates `repl->agents[]` (in-memory)
- `ik_repl_nav_next_sibling()` - iterates `repl->agents[]` (in-memory)

#### Design Decision

Navigation operates on in-memory agents only, which is correct:
- Can't switch to an agent that isn't loaded
- Dead agents are removed from array when killed
- Consistent with `/kill` behavior

After Gap 1 fix, all running agents will be in memory, making this consistent with `/agents`.

---

## Legacy System Migration Gaps

### Gap 5: Session Restore Uses Legacy Replay System

**Status:** COMPLETE (merged into Gap 1)
**Depends on:** Gap 0 (Message Type Unification)

#### Current Code (session_restore.c:50)

```c
res_t load_res = ik_db_messages_load(tmp, db_ctx, session_id, repl->shared->logger);
```

#### Problem

`ik_db_messages_load()` is session-based (pre-agent system):
- Takes `session_id`, not `agent_uuid`
- Doesn't understand agent-scoped messages
- Doesn't walk ancestor chains
- Doesn't respect fork points

#### Should Use

```c
res_t load_res = ik_agent_replay_history(db_ctx, tmp, agent_uuid, &replay_ctx);
```

This function:
- Takes `agent_uuid`
- Walks ancestor chain (parent -> grandparent -> ... -> root or clear)
- Respects fork_message_id boundaries
- Returns properly scoped history

#### Proposed Fix

After Gap 0 (type unification), this becomes trivial:

```c
// For Agent 0:
ik_replay_context_t *replay_ctx;
res = ik_agent_replay_history(db_ctx, tmp, repl->current->uuid, &replay_ctx);

// Direct assignment - no conversion needed (Gap 0 unified the types)
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *msg = replay_ctx->messages[i];
    if (is_conversation_kind(msg->kind)) {
        ik_openai_conversation_add_msg(repl->current->conversation, msg);
    }
    ik_event_render(repl->current->scrollback, msg->kind, msg->content, msg->data_json);
}
```

**Recommendation:** Merge Gap 5 into Gap 1 - restore ALL agents (including Agent 0) using the same `ik_repl_restore_agents()` function. This eliminates the special case for Agent 0 and removes the legacy `ik_repl_restore_session()` function entirely.

#### Code to Remove After Fix

- `src/repl/session_restore.c` - entire file can be deleted (logic moves to agent restore)
- `src/repl/session_restore.h` - entire file can be deleted
- `src/db/replay.c:ik_db_messages_load()` - no longer needed (agent replay handles all cases)

---

## Unspecified Behavior Gaps

### Gap 6: Current Agent Selection on Restore

**Status:** Needs specification

#### Problem

When multiple agents are restored, which one becomes `repl->current`?

#### Not Specified

Neither the design docs nor task files specify this.

#### Options

1. **Always Agent 0 (root)** - Safest default, user navigates to desired agent
2. **Last active agent** - Requires tracking `last_active_at` timestamp in DB
3. **Most recently created** - Based on `created_at` field

#### Recommendation

Option 1 (Agent 0) is simplest and safest. User can use Ctrl+Arrow navigation to reach children.

If tracking is desired, add to agents table:
```sql
ALTER TABLE agents ADD COLUMN last_active_at BIGINT;
```

Update on each agent switch in `ik_repl_switch_agent()`.

---

### Gap 7: No Cleanup of Stale Running Agents

**Status:** Resolves with Gap 1 fix

#### Problem

When the process exits (normal or crash), agents are NOT marked as 'dead' in the database. On next startup, the database still shows them as 'running'.

#### Current Behavior

- **Shutdown:** No cleanup code exists
- **Startup:** No cleanup code exists

#### Options

1. **Mark dead on shutdown** - Clean exit marks all in-memory agents as dead
2. **Mark dead on startup** - Before creating Agent 0, mark all 'running' as 'dead'
3. **Keep running and restore** - Load them back (Gap 1 fix)

#### Recommendation

Option 3 is the intended design per specs. Gap 1 fix addresses this.

For robustness, could add startup cleanup for agents that fail to restore (corrupt data, missing messages, etc).

---

## UI Integration Gaps

### Gap 8: Separator Navigation Context Never Populated (NEW)

**Status:** Implementation complete but not wired up

#### Analysis

**Function exists (layer_separator.c:249):**
```c
void ik_separator_layer_set_nav_context(ik_layer_t *layer,
                                        const char *parent_uuid,
                                        const char *prev_sibling_uuid,
                                        const char *current_uuid,
                                        const char *next_sibling_uuid,
                                        size_t child_count)
```

**But never called in production code:**
```bash
$ grep -r "set_nav_context" src/*.c
src/layer_separator.c:249:void ik_separator_layer_set_nav_context(ik_layer_t *layer,
# Only the definition - no calls!
```

#### Impact

- Separator shows navigation context area (arrows, UUID)
- But navigation context is never populated with actual agent data
- User sees empty/static navigation indicators

#### Task File Issue

`separator-nav-context.md` post-conditions state:
> Indicators update on agent switch (when context pointers updated)

But task doesn't specify WHERE to call `set_nav_context()`.

#### Proposed Fix

Call `ik_separator_layer_set_nav_context()` from:
1. `ik_repl_render_frame()` - update before each render
2. OR `ik_repl_switch_agent()` - update on agent switch

Example integration in render_frame:
```c
// Calculate navigation context for current agent
const char *parent_uuid = repl->current->parent_uuid;
const char *prev_sibling = NULL, *next_sibling = NULL;
size_t child_count = 0;

// Find siblings and children (iterate repl->agents[])
// ... calculation logic ...

// Update separator layers
ik_separator_layer_set_nav_context(repl->current->separator_layer,
    parent_uuid, prev_sibling, repl->current->uuid,
    next_sibling, child_count);
```

---

### Gap 9: Lower Separator Layer Not Per-Agent (NEW)

**Status:** Architectural concern - minor

#### Analysis

**repl_init.c:96-99:**
```c
// Create lower separator layer (not part of agent - stays in repl)
repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &repl->lower_separator_visible);

// Add lower separator to agent's layer cake
result = ik_layer_cake_add_layer(repl->current->layer_cake, repl->lower_separator_layer);
```

#### Problem

1. Lower separator layer is created once, owned by `repl`
2. It's added to `repl->current->layer_cake`
3. When switching agents, each agent has its own `layer_cake`
4. The lower separator isn't moved to the new agent's layer cake

#### Impact

- Minor visual inconsistency when switching agents
- May cause rendering issues if layer_cake iteration expects consistent layers

#### Proposed Fix

Either:
1. Make lower separator per-agent (create in `ik_agent_create`)
2. Or move layer on switch in `ik_repl_switch_agent()`

---

## Tool Execution Gap

### Gap 10: Tool Thread Completion Only Checked for Current Agent (NEW)

**Status:** Edge case - low priority

#### Analysis

**repl.c event loop (lines 117-124):**
```c
// Poll for tool thread completion
pthread_mutex_lock_(&repl->current->tool_thread_mutex);
ik_agent_state_t current_state = repl->current->state;
bool complete = repl->current->tool_thread_complete;
pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

if (current_state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
    handle_tool_completion(repl);
}
```

#### Problem

Only checks `repl->current` for tool completion. If:
1. Agent A starts a tool
2. User switches to Agent B
3. Tool on Agent A completes

The completion isn't detected until user switches back to Agent A.

#### Impact

- Low - tools typically complete quickly
- Tool result will be processed when user switches back
- No data loss, just delayed processing

#### Proposed Fix (optional)

Iterate all agents for tool completion:
```c
for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *agent = repl->agents[i];
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    if (agent->state == IK_AGENT_STATE_EXECUTING_TOOL &&
        agent->tool_thread_complete) {
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        // Handle completion for this agent
        // May need to switch context temporarily
    } else {
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
    }
}
```

---

## Design Clarification

### Gap 11: Scrollback Empty at Fork Time (Design Question)

**Status:** Intentional behavior - document for clarity

#### Observation

When forking:
- **Conversation**: Full parent history copied (for LLM context)
- **Scrollback**: Starts empty (shows only post-fork content)

#### Code (commands_agent.c)

```c
// Copy parent's conversation to child (history inheritance)
res = ik_agent_copy_conversation(child, parent);
// Note: No scrollback copy - intentional
```

#### Is This Correct?

**Yes, per design:**
- Conversation copy is for LLM context continuity
- Scrollback is ephemeral display
- On restart, scrollback IS reconstructed via replay

**But UX concern:**
- User sees empty child scrollback
- But LLM has full conversation context
- Could be confusing ("why doesn't the agent remember?")

#### Recommendation

Document this behavior in user-facing docs. Consider:
1. Status bar indication: "Inherited: 15 messages"
2. `/context` command to show inherited message count
3. Scrollback header showing inheritance

---

## Task Specification Issues

### startup-replay.md

The task correctly described the algorithm and created the implementation functions. However, the "Files to CREATE/MODIFY" section omitted:

- `src/repl_init.c` - the integration point
- `src/repl/session_restore.c` - migration to agent-based replay

### agents-tree-cmd.md

Did not specify which data source to use (DB vs memory). The implementation chose DB, which is correct for showing "all agents" but inconsistent with `/kill` which uses memory.

### separator-nav-context.md

Did not specify WHERE to call `ik_separator_layer_set_nav_context()`. The function was implemented but never integrated into render or switch logic.

---

## Related Files

### Replay Infrastructure (implemented, working)
- `src/db/agent_replay.c` - replay functions
- `src/db/agent_replay.h` - declarations
- `tests/unit/db/agent_replay_test.c` - tests pass

### Agent Registry (implemented, working)
- `src/db/agent.c` - registry CRUD
- `src/db/agent.h` - declarations
- `tests/unit/db/agent_registry_test.c` - tests pass

### Startup (needs modification)
- `src/repl_init.c` - needs restoration loop
- `src/repl/session_restore.c` - needs agent-based replay

### Separator (needs wiring)
- `src/layer_separator.c` - rendering works, `set_nav_context` never called
- `src/repl.c` or `src/render.c` - needs to call `set_nav_context`

### Commands (consistent after Gap 1)
- `src/commands_agent_list.c` - `/agents` (queries DB - correct)
- `src/commands_agent.c` - `/kill`, `/fork` (uses memory - correct)
- `src/commands_mail.c` - `/send`, `/filter-mail` (uses memory - correct)

---

## Testing Considerations

After fixes, need tests for:

### Gap 0 (Message Type Unification)
1. `ik_msg_t` has `id` field
2. DB queries populate `id` correctly
3. In-memory created messages have `id = 0`
4. `ik_replay_context_t` uses `ik_msg_t**`
5. OpenAI serialization ignores `id` field (no wire format change)
6. All existing tests pass after type change
7. No `ik_message_t` references remain in codebase
8. `ik_msg_from_db()` is removed (grep confirms no references)

### Gap 1 (Startup Restoration)
1. Multiple agents survive restart
2. Parent-child hierarchy preserved
3. Each agent's conversation intact
4. Each agent's scrollback rendered correctly
5. Fork points respected (child doesn't see parent's post-fork messages)
6. Clear events respected (don't walk past clear)
7. Deep ancestry (grandchild accessing grandparent context)
8. Dependency ordering (parents created before children)

### Gap 8 (Separator Navigation)
1. Navigation context updates on agent switch
2. Parent/sibling/child indicators show correct UUIDs
3. Grayed indicators for unavailable directions
4. Navigation context updates on fork (new child appears)
5. Navigation context updates on kill (removed agent disappears)

---

## Priority Order

### Phase 1: Foundation (PREREQUISITE)

1. **Gap 0: Message Type Unification** - COMPLETE
   - Mechanical refactor: add `id` to `ik_msg_t`, eliminate `ik_message_t`
   - Removes conversion overhead, simplifies all subsequent work

### Phase 2: Core Persistence (CRITICAL)

2. **Gap 1: Startup Agent Restoration Loop** - COMPLETE
   - Core feature: multi-agent persistence across restarts
   - Includes `ik_agent_restore()` function (distinct from `ik_agent_create()`)

3. **Gap 5: Session Restore Migration** - COMPLETE (merged into Gap 1)
   - Merged into Gap 1 as recommended
   - Deleted `session_restore.c`, `session_restore.h`
   - Note: `ik_db_messages_load()` still exists but may not be needed

### Phase 3: Polish (MEDIUM/LOW)

4. **Gap 8: Separator Navigation Wiring** - MEDIUM (UI feature incomplete)
5. **Gap 6: Current Agent Selection** - LOW (needs spec, simple default works)
6. **Gap 9: Lower Separator Per-Agent** - LOW (minor visual issue)
7. **Gap 10: Multi-Agent Tool Polling** - LOW (edge case)

### Automatic Resolutions

Gaps 2, 3, 4, 7 resolve automatically with Gap 1 fix.

### Code Removal Summary

After all gaps are fixed, the following should be removed:

**From Gap 0:**
- `src/msg.c:ik_msg_from_db()` - entire function
- `src/msg.h:ik_msg_from_db()` - declaration
- `src/db/replay.h:ik_message_t` - typedef
- `tests/unit/msg_test.c` - tests for `ik_msg_from_db()`

**From Gap 1+5 (merged):**
- `src/repl/session_restore.c` - entire file
- `src/repl/session_restore.h` - entire file
- `src/db/replay.c:ik_db_messages_load()` - entire function (if no other callers)

---

## Future Work (Deferred)

The following gaps are acknowledged but deferred for future releases:

### Gap 9: Lower Separator Per-Agent

**Status:** LOW priority - minor visual issue

The lower separator layer is created once and owned by `repl`, but added to `repl->current->layer_cake`. When switching agents, each has its own layer_cake, so the lower separator isn't moved.

**Impact:** Minor visual inconsistency when switching agents.

**Fix options:**
1. Make lower separator per-agent (create in `ik_agent_create`)
2. Or move layer on switch in `ik_repl_switch_agent()`

### Gap 10: Multi-Agent Tool Polling

**Status:** LOW priority - edge case

The event loop only checks `repl->current` for tool thread completion. If:
1. Agent A starts a tool
2. User switches to Agent B
3. Tool on Agent A completes

The completion isn't detected until user switches back to Agent A.

**Impact:** Low - tools typically complete quickly. Tool result will be processed when user switches back. No data loss, just delayed processing.

**Fix:** Iterate all agents for tool completion in event loop.

### Gap 11: Scrollback Empty at Fork Time (UX)

**Status:** Design clarification - intentional behavior

When forking:
- Conversation: Full parent history copied (for LLM context)
- Scrollback: Starts empty (shows only post-fork content)

This is intentional - scrollback IS reconstructed via replay on restart. But UX concern exists:
- User sees empty child scrollback
- But LLM has full conversation context
- Could be confusing

**UX improvements to consider:**
1. Status bar indication: "Inherited: 15 messages"
2. `/context` command to show inherited message count
3. Scrollback header showing inheritance

### Known Data Loss: Mark Timestamps

Mark timestamps (`ik_mark_t.timestamp`) are lost after restart. The replay context doesn't include timestamps - only position and label are preserved. To fix:
- Add `timestamp` to mark event's `data` JSON in database
- Parse timestamp during replay

This is acceptable for now - marks retain position and label.
