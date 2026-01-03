# Task: Use shared message filter in agent restore

## Target
Bug 5: Application crashes with assertion failure during /fork when agent_killed events exist in database

## Macro Context

**What:** When a user types `/fork`, the application crashes with:
```
ikigai: src/openai/client_msg.c:22: ik_openai_msg_create: Assertion `content != NULL' failed.
Aborted
```

**Why this happens:**
1. User performs operations that create metadata events (e.g., `/kill`)
2. These events are stored in database with `kind="agent_killed"` and `content=NULL`
3. On restart, `ik_repl_restore_agents()` loads ALL messages from database
4. The local `is_conversation_kind()` filter in agent_restore.c doesn't exclude "agent_killed"
5. Metadata events with NULL content end up in the conversation array
6. When user types `/fork`, `ik_agent_copy_conversation()` tries to copy these messages
7. `ik_openai_msg_create()` crashes on `assert(content != NULL)`

**Root cause:**
Code duplication - there are TWO different functions for identifying conversation messages:
- `ik_msg_is_conversation_kind()` in src/msg.c (correct, from bug4)
- `is_conversation_kind()` in src/repl/agent_restore.c (buggy, hardcoded exclusions)

**Why bug4 didn't catch this:**
Bug4 fixed metadata filtering at OpenAI API serialization, but agent restore uses its own duplicate filter logic that wasn't updated.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/database.md
- .agents/skills/errors.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/quality.md

## Pre-read Docs
- src/msg.h (lines 30-45 - ik_msg_is_conversation_kind documentation)

## Pre-read Source
- src/msg.h (READ - canonical message helper functions)
- src/msg.c (READ - ik_msg_is_conversation_kind implementation from bug4)
- src/repl/agent_restore.c (MODIFY - lines 20-34, replace local filter with shared helper)
- src/agent.c (READ - lines 258-290, ik_agent_copy_conversation that triggers the crash)
- src/openai/client_msg.c (READ - lines 20-22, assertion that fails)

## Source Files to MODIFY
- src/repl/agent_restore.c (remove local is_conversation_kind, use ik_msg_is_conversation_kind)
- tests/integration/agent_restore_test.c (add test for metadata filtering during restore)

## Investigation Context

**The duplicate filter logic:**

In `src/repl/agent_restore.c:23-34`:
```c
// Local static function - DUPLICATE LOGIC, MISSING agent_killed
static bool is_conversation_kind(const char *kind)
{
    if (kind == NULL) return false;

    // Control events that don't go in conversation
    if (strcmp(kind, "clear") == 0) return false;
    if (strcmp(kind, "mark") == 0) return false;
    if (strcmp(kind, "rewind") == 0) return false;

    // Everything else is conversation content
    return true;  // BUG: Returns true for "agent_killed"!
}
```

In `src/msg.c:12-29` (from bug4):
```c
// Shared helper - CORRECT, complete list
bool ik_msg_is_conversation_kind(const char *kind) {
    if (kind == NULL) {
        return false;
    }

    /* Conversation kinds that should be sent to LLM */
    if (strcmp(kind, "system") == 0 ||
        strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "tool_call") == 0 ||
        strcmp(kind, "tool_result") == 0 ||
        strcmp(kind, "tool") == 0) {
        return true;
    }

    /* All other kinds (including metadata events) should not be sent to LLM */
    return false;
}
```

**Call sites in agent_restore.c:**

Line 101: `if (is_conversation_kind(msg->kind)) {`
Line 276: `if (is_conversation_kind(msg->kind)) {`

Both need to use `ik_msg_is_conversation_kind()` instead.

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Bug4 is complete (`src/msg.c` and `ik_msg_is_conversation_kind()` exist)

## Task

### Part 1: Replace duplicate filter with shared helper
1. In `src/repl/agent_restore.c`:
   - Add `#include "msg.h"` to includes section (alphabetically after logger.h)
   - Delete the local `is_conversation_kind()` function (lines 23-34)
   - Replace calls to `is_conversation_kind(msg->kind)` with `ik_msg_is_conversation_kind(msg->kind)` (lines 101, 276)

### Part 2: Add test coverage for metadata filtering during restore
1. Create integration test that reproduces the crash scenario:
   - Start with clean database
   - Create agent, send message, kill agent (creates agent_killed event)
   - Restore agents from database
   - Verify agent_killed event is NOT in conversation array
   - Verify agent_killed event IS in scrollback
   - Attempt fork (should not crash)

## TDD Cycle

### Red
1. Create test in `tests/integration/agent_restore_metadata_test.c`:
   - Test agent restore with agent_killed event in database
   - Verify metadata events are excluded from conversation array
   - Verify fork doesn't crash when metadata events exist

2. Run test - should FAIL because local filter allows agent_killed into conversation

### Green
1. Replace local `is_conversation_kind()` with `ik_msg_is_conversation_kind()`:
   - Add `#include "msg.h"`
   - Remove static function
   - Update two call sites (lines 101, 276)

2. Run `make check` - tests should pass
3. Run `make lint` - should pass

### Refactor
1. Search codebase for other potential duplicate filter logic
2. Ensure all message filtering uses the shared helper
3. Add comment explaining why metadata events are excluded

## Sub-agent Usage
- Use Task tool with Explore agent to search for other places that might filter messages by kind
- Verify no other code has duplicate filter logic

## Overcoming Obstacles
- If test is complex, refer to existing agent restore tests for patterns
- If unsure about test database setup, refer to .agents/skills/database.md
- Persist until tests pass and fork no longer crashes

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- Local `is_conversation_kind()` function removed from agent_restore.c
- All calls use `ik_msg_is_conversation_kind()` from msg.h
- Test exists verifying metadata events are filtered during restore
- Fork command doesn't crash when agent_killed events exist in database
- Working tree is clean (all changes committed)

## Success Criteria

**Can reproduce and fix:**
```bash
# Setup
$ bin/ikigai
> what is 1 + 1
[LLM responds: 2]
> /fork     # Should NOT crash (used to crash on agent_killed event)
Forked from <uuid>
```

**Verification:**
- No duplicate filter logic (DRY principle)
- Metadata events excluded from conversation array during restore
- Fork command works even after kill/mark/rewind operations
- All message filtering uses the centralized helper

## Deviations
Document any deviation from this plan with reasoning.
