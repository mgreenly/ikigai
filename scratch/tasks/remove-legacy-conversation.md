# Task: Remove Legacy Conversation Field and References

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** migrate-providers.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Remove old `conversation` field from agent struct and delete all calls to legacy `ik_openai_conversation_*` and `ik_openai_msg_*` functions. All code now uses new provider-agnostic API exclusively. This is the disconnection step - after this, legacy code is no longer called from anywhere.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load naming` - Naming conventions
- `/load style` - Code style
- `/load tdd` - Test-driven development

**Source Files to Modify:**
- `src/agent.h` - Remove conversation field (line 96)
- `src/agent.c` - Remove conversation creation/cloning
- `src/repl_actions_llm.c` - Remove old API calls
- `src/repl_event_handlers.c` - Remove old API calls
- `src/repl_tool.c` - Remove old API calls
- `src/commands_fork.c` - Remove old API calls
- `src/commands_basic.c` - Remove old API calls
- `src/repl/agent_restore.c` - Remove old API calls
- `src/repl/agent_restore_replay.c` - Remove old API calls
- `src/wrapper.c` - Remove old mock wrappers
- `src/wrapper_internal.h` - Remove old mock declarations

**Include Removals:**
- All `#include "openai/client.h"` references outside src/openai/
- All `#include "openai/client_msg.h"` references

## Implementation

### 1. Update src/agent.h - Remove conversation Field

**Location:** Line 95-98

**Delete:**
```c
    // Conversation state (per-agent)
    ik_openai_conversation_t *conversation;  // DEPRECATED - will be removed

    // NEW: Provider-agnostic message storage
    ik_message_t **messages;      // Array of message pointers
```

**Keep Only:**
```c
    // Conversation state (per-agent)
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity
```

**Also Delete:**
- Forward declaration: `typedef struct ik_openai_conversation ik_openai_conversation_t;` (line 16)

### 2. Update src/agent.c - Remove Conversation Creation

**In `ik_agent_create()` function:**

**Delete:**
```c
agent->conversation = ik_openai_conversation_create(agent);
```

**Keep:**
```c
agent->messages = NULL;
agent->message_count = 0;
agent->message_capacity = 0;
```

**In fork operation:**

**Delete entire old API section:**
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
```

**Keep only:**
```c
// Clone parent messages
res_t clone_res = ik_agent_clone_messages(child, parent);
if (is_err(&clone_res)) {
    return clone_res;  // Propagate error instead of just logging
}
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 3. Update src/repl_actions_llm.c - Remove Old API

**Delete old API section:**
```c
// Old API (keep for now)
ik_msg_t *msg = ik_openai_msg_create(agent->conversation, IK_ROLE_USER, trimmed);
ik_openai_conversation_add_msg(agent->conversation, msg);

// New API (add in parallel)
```

**Keep only:**
```c
ik_message_t *msg = ik_message_create_text(agent, IK_ROLE_USER, trimmed);
res_t res = ik_agent_add_message(agent, msg);
if (is_err(&res)) {
    return res;  // Propagate error - this is now authoritative
}
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 4. Update src/repl_event_handlers.c - Remove Old API

**Delete old API section:**
```c
// Old API (keep for now)
ik_msg_t *assistant_msg = ik_openai_msg_create(agent->conversation,
                                                IK_ROLE_ASSISTANT,
                                                agent->assistant_response);
ik_openai_conversation_add_msg(agent->conversation, assistant_msg);

// New API (add in parallel)
```

**Keep only:**
```c
ik_message_t *msg = ik_message_create_text(agent, IK_ROLE_ASSISTANT,
                                            agent->assistant_response);
res_t res = ik_agent_add_message(agent, msg);
if (is_err(&res)) {
    ik_log_error("Failed to add assistant message: %s", res.err->msg);
    // Continue - this is logged but not fatal for now
}
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 5. Update src/repl_tool.c - Remove Old API

**For tool calls, delete old API section:**
```c
// Old API (keep for now)
ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(...);
ik_openai_conversation_add_msg(agent->conversation, tool_call_msg);

// New API (add in parallel)
```

**Keep only:**
```c
ik_message_t *call_msg = ik_message_create_tool_call(agent, tool_call_id,
                                                      tool_name, args_json);
res_t res = ik_agent_add_message(agent, call_msg);
if (is_err(&res)) {
    ik_log_error("Failed to add tool call: %s", res.err->msg);
}
```

**For tool results, delete old API section:**
```c
// Old API (keep for now)
ik_msg_t *tool_result_msg = ik_openai_msg_create_tool_result(...);
ik_openai_conversation_add_msg(agent->conversation, tool_result_msg);

// New API (add in parallel)
```

**Keep only:**
```c
ik_message_t *result_msg = ik_message_create_tool_result(agent, tool_call_id,
                                                          output, !success);
res_t res = ik_agent_add_message(agent, result_msg);
if (is_err(&res)) {
    ik_log_error("Failed to add tool result: %s", res.err->msg);
}
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 6. Update src/commands_fork.c - Remove Old API

**Delete old API section:**
```c
// Old API (keep for now)
ik_msg_t *user_msg = ik_openai_msg_create(child->conversation, IK_ROLE_USER, prompt_text);
ik_openai_conversation_add_msg(child->conversation, user_msg);

// New API (add in parallel)
```

**Keep only:**
```c
ik_message_t *msg = ik_message_create_text(child, IK_ROLE_USER, prompt_text);
res_t res = ik_agent_add_message(child, msg);
if (is_err(&res)) {
    return res;  // Propagate error
}
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 7. Update src/commands_basic.c - Remove Old API

**Delete old API section:**
```c
// Old API (keep for now)
ik_openai_conversation_clear(agent->conversation);

// New API (add in parallel)
```

**Keep only:**
```c
ik_agent_clear_messages(agent);
```

**Remove include (if present):**
```c
#include "openai/client.h"  // DELETE THIS LINE IF PRESENT
```

### 8. Update src/repl/agent_restore.c - Remove Old API

**Delete old API section:**
```c
// Old API (keep for now)
ik_msg_t *system_msg = ik_openai_msg_create(agent->conversation, IK_ROLE_SYSTEM, system_prompt);
ik_openai_conversation_add_msg(agent->conversation, system_msg);

// New API (add in parallel)
```

**Keep only:**
```c
// System prompts are NOT added to message array in new API
// They are handled via agent->system_prompt field (string)
// The request builder will use ik_request_set_system() to add them
// No action needed in restore code - system prompt already stored in agent struct
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 9. Update src/repl/agent_restore_replay.c - Remove Old API

**Delete old API section in replay loop:**
```c
// Old API (keep for now)
ik_openai_conversation_add_msg(agent->conversation, db_msg);

// New API (add in parallel)
```

**Keep only:**
```c
ik_message_t *provider_msg = ik_message_from_db_msg(agent, db_msg);
if (provider_msg != NULL) {
    res_t res = ik_agent_add_message(agent, provider_msg);
    if (is_err(&res)) {
        ik_log_error("Failed to add replayed message: %s", res.err->msg);
    }
} else {
    ik_log_error("Failed to convert database message kind=%s", db_msg->kind);
}
```

**Remove include:**
```c
#include "openai/client.h"  // DELETE THIS LINE
```

### 10. Update src/wrapper.c - Remove Old Mock

**Delete:**
```c
MOCKABLE_IMPL(res_t, ik_openai_conversation_add_msg_,
              (ik_openai_conversation_t *conv, ik_msg_t *msg))
{
    return ik_openai_conversation_add_msg(conv, msg);
}
```

**Keep:**
```c
MOCKABLE_IMPL(res_t, ik_agent_add_message_,
              (ik_agent_ctx_t *agent, ik_message_t *msg))
{
    return ik_agent_add_message(agent, msg);
}
```

### 11. Update src/wrapper_internal.h - Remove Old Declaration

**Delete:**
```c
MOCKABLE_DECL(res_t, ik_openai_conversation_add_msg_,
              (ik_openai_conversation_t *conv, ik_msg_t *msg));
```

**Keep:**
```c
MOCKABLE_DECL(res_t, ik_agent_add_message_,
              (ik_agent_ctx_t *agent, ik_message_t *msg));
```

## Test Requirements

### Update All Existing Tests

Every test that referenced `agent->conversation` must be updated to use `agent->messages`:

**Pattern to find:**
```bash
grep -r "agent->conversation" tests/
```

**Update each occurrence:**
- `agent->conversation->message_count` → `agent->message_count`
- `agent->conversation->messages[i]` → `agent->messages[i]`
- Remove dual-mode verification tests (no longer needed)

**Tests to update:**
- `tests/unit/agent/fork_test.c`
- `tests/unit/agent/message_management_test.c`
- `tests/unit/commands/clear_test.c`
- `tests/unit/commands/fork_test.c`
- `tests/unit/repl/agent_restore_test.c`
- `tests/integration/agent/restore_test.c`
- `tests/integration/repl/dual_mode_test.c` - DELETE THIS FILE (no longer dual mode)

### Verification Tests

**tests/unit/agent/no_legacy_references_test.c:**

**Test Cases:**
- Verify agent struct compiles without `ik_openai_conversation_t` type
- Verify can create agent without including `openai/client.h`
- Verify can add messages, fork, clear without legacy API

## Postconditions

- [ ] `agent->conversation` field deleted from struct
- [ ] All `ik_openai_conversation_*` calls removed from non-openai code
- [ ] All `ik_openai_msg_*` calls removed from non-openai code
- [ ] All `#include "openai/client.h"` removed from non-openai code
- [ ] Old mock wrappers removed
- [ ] All tests updated to use new API exclusively
- [ ] `make check` passes
- [ ] No compiler warnings
- [ ] Grep shows no legacy references outside src/openai/

## Verification Commands

After completion, these should return EMPTY:

```bash
grep -r "ik_openai_conversation_t" src/ --include="*.c" --include="*.h" | grep -v "src/openai/"
grep -r "ik_openai_msg_create" src/ --include="*.c" --include="*.h" | grep -v "src/openai/"
grep -r "ik_openai_conversation_add_msg" src/ --include="*.c" --include="*.h" | grep -v "src/openai/"
grep -r "#include.*openai/client" src/ --include="*.c" --include="*.h" | grep -v "src/openai/"
```

## Success Criteria

After this task:
1. Agent struct has ONLY new message storage (no conversation field)
2. No code outside src/openai/ calls legacy functions
3. No code outside src/openai/ includes legacy headers
4. All tests pass using only new API
5. Legacy code completely disconnected from rest of codebase
6. Ready to delete src/openai/ directory in next task
