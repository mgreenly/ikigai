# Task: Fork History Inheritance

## Target
User Story: rel-06/user-stories/05-fork-inherits-history.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (History Storage section)
- rel-06/tasks/startup-replay.md (for replay algorithm details)

## Pre-read Source (READ - patterns to follow)
- src/agent.c (ik_agent_create function)
- src/agent.h (ik_agent_ctx_t structure, conversation field)
- src/db/message.h (message persistence API)

## Pre-read Tests (READ - patterns to follow)
- tests/unit/agent/agent_test.c

## Files to CREATE/MODIFY
- MODIFY: src/agent.h (add fork_message_id field to ik_agent_ctx_t)
- MODIFY: src/agent.c (update ik_agent_create to handle fork_message_id)
- MODIFY: src/commands.c or equivalent (update cmd_fork to pass fork point)
- MODIFY: src/db/agent.c or registry code (update registry insert with fork_message_id)
- MODIFY: tests/unit/agent/agent_test.c (add fork history tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Task fork-cmd-prompt.md is complete
- Task messages-agent-uuid.md is complete (messages.agent_uuid column exists)

## Task
Implement history inheritance at fork. Child starts with parent's conversation history. Uses delta storage - child records fork point and only stores post-fork messages.

**Approach: "Walk Backwards, Play Forwards"**
When loading a child agent's conversation, walk the full ancestor chain backwards to find all ancestors, then replay their histories forward in chronological order. This ensures multi-generational context inheritance.

**Changes to ik_agent_ctx_t:**
```c
typedef struct ik_agent_ctx {
    // ... existing ...
    char *fork_message_id;  // Last message ID before fork (NULL for root)
} ik_agent_ctx_t;
```

**On fork:**
1. Record current last message ID as fork_message_id
2. Copy parent's conversation to child's memory
3. Store fork_message_id in registry

**On conversation load (walk backwards, play forwards):**

See startup-replay.md for the detailed algorithm. Summary:

1. Build replay ranges using `ik_agent_build_replay_ranges()`:
   - Walk from leaf agent to root (or clear event)
   - Each range captures: `{agent_uuid, start_id, end_id}`
   - `start_id` = after clear (or 0 for beginning)
   - `end_id` = fork point (or 0 for leaf = no limit)

2. Reverse ranges to chronological order

3. For each range: query messages, append to replay context

**Range structure:**
```c
typedef struct {
    char *agent_uuid;   // Which agent's messages to query
    int64_t start_id;   // Start AFTER this message ID (0 = from beginning)
    int64_t end_id;     // End AT this message ID (0 = no limit)
} ik_replay_range_t;
```

Result: full inherited context + child's own messages in correct order

## TDD Cycle

### Red
1. Update `src/agent.h` with fork_message_id field

2. Add tests:
   - Test fork records fork_message_id
   - Test child can access parent's pre-fork messages
   - Test child's post-fork messages are separate
   - Test registry updated with fork_message_id
   - Test conversation replay includes inherited history
   - Test multi-level ancestry (grandchild accessing grandparent's context)
   - Test clear event boundary (stops ancestry traversal)
   - Test ancestor chain collection and reversal
   - Test forward replay produces correct chronological order

3. Run `make check` - expect test failures

### Green
1. Update `ik_agent_create()` to accept fork_message_id
2. Update `cmd_fork()` to pass current last message ID
3. Implement conversation copying on fork
4. Update registry insert to include fork_message_id
5. Run `make check` - expect pass

### Refactor
1. Consider copy-on-write optimization (later)
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- Fork records fork_message_id
- Child inherits parent's history
- Post-fork messages stored separately
- Registry tracks fork point
- Working tree is clean (all changes committed)
