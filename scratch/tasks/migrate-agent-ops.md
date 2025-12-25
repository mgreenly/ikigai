# Task: Migrate Agent Operations to New API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** migrate-repl-messages.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Update agent lifecycle operations (create, fork, clear) to use new provider-agnostic message API. Maintain dual-mode: populate both storage mechanisms in parallel. Focus on agent.c (create/fork), commands_fork.c (fork message), and commands_basic.c (clear command).

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load naming` - Naming conventions
- `/load style` - Code style
- `/load tdd` - Test-driven development

**Source Files to Read:**
- `src/message.h` - New message API
- `src/agent.h` - Agent struct and functions
- `src/agent.c` - Agent creation and fork logic - search for ik_agent_create() and fork functions
- `src/commands_fork.c` - Fork command implementation
- `src/commands_basic.c` - Clear command implementation
- `src/openai/client.h` - Old conversation API

**Test Files:**
- `tests/unit/agent/fork_test.c` - Fork operation tests
- `tests/unit/commands/clear_test.c` - Clear command tests

## Error Handling Policy

**Memory Allocation Failures:**
- All talloc allocations: PANIC with LCOV_EXCL_BR_LINE
- Rationale: OOM is unrecoverable, panic is appropriate

**Validation Failures:**
- Return ERR allocated on parent context (not on object being freed)
- Example: `return ERR(parent_ctx, INVALID_ARG, "message")`

**During Dual-Mode (Tasks 1-4):**
- Old API calls succeed: continue normally
- New API calls fail: log error, continue (old API is authoritative)
- Pattern: `if (is_err(&res)) { ik_log_error("Failed: %s", res.err->msg); }`

**After Migration (Tasks 5-8):**
- New API calls fail: propagate error immediately
- Pattern: `if (is_err(&res)) { return res; }`

**Assertions:**
- NULL pointer checks: `assert(ptr != NULL)` with LCOV_EXCL_BR_LINE
- Only for programmer errors, never for runtime conditions

## Implementation

### 1. Update src/agent.c - Agent Creation

**Location:** In `ik_agent_create()` function - search for where conversation is initialized

**Current Initialization:**
```c
agent->conversation = ik_openai_conversation_create(agent);
```

**Keep As-Is:** The old conversation is created and remains. New fields are already initialized to NULL/0 by previous task.

**No changes needed** - dual mode initialization complete from previous task.

### 2. Update src/agent.c - Fork Operation

**Location:** In agent fork function - search for where parent conversation is cloned to child

**Current Code Pattern:**
```c
// Clone parent conversation
child->conversation = ik_openai_conversation_create(child);
for (size_t i = 0; i < parent->conversation->message_count; i++) {
    ik_msg_t *orig = parent->conversation->messages[i];
    ik_msg_t *clone = ik_openai_msg_create(child->conversation, orig->role, orig->content);
    // Copy data_json if present
    if (orig->data_json) {
        clone->data_json = talloc_strdup(clone, orig->data_json);
    }
    ik_openai_conversation_add_msg(child->conversation, clone);
}
```

**New Code (DUAL MODE):**
```c
// Clone parent conversation (old API - keep for now)
child->conversation = ik_openai_conversation_create(child);
for (size_t i = 0; i < parent->conversation->message_count; i++) {
    ik_msg_t *orig = parent->conversation->messages[i];
    ik_msg_t *clone = ik_openai_msg_create(child->conversation, orig->role, orig->content);
    if (orig->data_json) {
        clone->data_json = talloc_strdup(clone, orig->data_json);
    }
    ik_openai_conversation_add_msg(child->conversation, clone);
}

// Clone parent messages (new API - add in parallel)
res_t clone_res = ik_agent_clone_messages(child, parent);
if (is_err(&clone_res)) {
    ik_log_error("Failed to clone messages to new storage: %s", clone_res.err->msg);
}
```

**Include Updates:**
- Verify `#include "message.h"` is present (should be from agent.h)

### 3. Update src/commands_fork.c - Fork Message Addition

**Location:** In fork command handler - search for where the fork prompt message is added to child agent's conversation

**Current Code Pattern:**
```c
ik_msg_t *user_msg = ik_openai_msg_create(child->conversation, IK_ROLE_USER, prompt_text);
ik_openai_conversation_add_msg(child->conversation, user_msg);
```

**New Code (DUAL MODE):**
```c
// Old API (keep for now)
ik_msg_t *user_msg = ik_openai_msg_create(child->conversation, IK_ROLE_USER, prompt_text);
ik_openai_conversation_add_msg(child->conversation, user_msg);

// New API (add in parallel)
ik_message_t *new_msg = ik_message_create_text(child, IK_ROLE_USER, prompt_text);
res_t res = ik_agent_add_message(child, new_msg);
if (is_err(&res)) {
    ik_log_error("Failed to add fork message to new storage: %s", res.err->msg);
}
```

**Include Updates:**
- Add `#include "message.h"` after existing includes

### 4. Update src/commands_basic.c - Clear Command

**Location:** In clear command handler function - search for where conversation is cleared

**Current Code Pattern:**
```c
ik_openai_conversation_clear(agent->conversation);
```

**New Code (DUAL MODE):**
```c
// Old API (keep for now)
ik_openai_conversation_clear(agent->conversation);

// New API (add in parallel)
ik_agent_clear_messages(agent);
```

**Include Updates:**
- Add `#include "message.h"` after existing includes (likely already has agent.h which includes it)

## Test Requirements

### Update Existing Tests

**tests/unit/agent/fork_test.c:**
- After fork, verify `child->message_count == parent->message_count`
- Verify message content matches between old and new storage
- After adding fork prompt, verify both storages incremented
- Add test: `test_fork_dual_storage_clone` - verifies deep copy in both storages

**tests/unit/commands/fork_test.c:**
- Update fork command tests to check dual storage
- Verify fork message added to both arrays

**tests/unit/commands/clear_test.c:**
- After clear, verify `agent->conversation->message_count == 0`
- After clear, verify `agent->message_count == 0`
- Add test: `test_clear_dual_storage` - verifies both storages cleared

### Create New Tests

**tests/unit/agent/clone_messages_test.c:**

**Test Cases:**
- `test_clone_empty` - Clone from agent with no messages
- `test_clone_text_messages` - Clone text messages only
- `test_clone_tool_messages` - Clone messages with tool calls/results
- `test_clone_deep_copy` - Verify deep copy (modify original, check clone unchanged)
- `test_clone_mixed_content` - Clone conversation with mixed message types

## Postconditions

- [ ] `src/agent.c` fork operation clones both storages
- [ ] `src/commands_fork.c` adds message to both storages
- [ ] `src/commands_basic.c` clears both storages
- [ ] Include statements updated in all modified files
- [ ] Existing tests updated to verify dual storage
- [ ] New clone tests created and passing
- [ ] `make check` passes
- [ ] No compiler warnings

## Success Criteria

After this task:
1. Agent fork operation maintains both storage mechanisms
2. Clear command empties both storages
3. Fork prompt message added to both storages
4. Tests verify synchronization across all operations
5. All existing functionality continues working
6. Message clone implementation thoroughly tested
