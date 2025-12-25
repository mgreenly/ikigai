# Task: Migrate REPL Message Creation to New API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** message-storage-dual-mode.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Update REPL code to use new provider-agnostic message API instead of legacy OpenAI functions. Maintain dual-mode operation: populate both `agent->conversation` (old) AND `agent->messages` (new) arrays in parallel. All existing code continues working while we establish that new API works correctly.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load log` - Logging API (ik_log_error for error logging)
- `/load naming` - Naming conventions
- `/load style` - Code style
- `/load tdd` - Test-driven development

**Source Files to Read:**
- `src/message.h` - New message creation API created in previous task
- `src/agent.h` - Agent struct with dual storage
- `src/openai/client.h` - Old API we're replacing (ik_openai_msg_create*)
- `src/openai/client_msg.c` - Implementation of old message creation

**Files to Modify:**
- `src/repl_actions_llm.c` - User message creation - search for where user input is processed and messages are added to conversation
- `src/repl_event_handlers.c` - Assistant message creation - search for HTTP completion callback where assistant responses are stored
- `src/repl_tool.c` - Tool call/result creation - search for tool execution handlers where tool messages are created

**Test Files:**
- `tests/unit/repl/event_handlers_test.c` - Test pattern for REPL handlers
- `tests/unit/repl/tool_test.c` - Test pattern for tool messages

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

### 1. Update src/repl_actions_llm.c

**Location:** In the function that handles user input submission - search for where user messages are created and added to conversation

**Current Code Pattern:**
```c
ik_msg_t *msg = ik_openai_msg_create(agent->conversation, IK_ROLE_USER, trimmed);
ik_openai_conversation_add_msg(agent->conversation, msg);
```

**New Code Pattern (DUAL MODE):**
```c
// Old API (keep for now)
ik_msg_t *msg = ik_openai_msg_create(agent->conversation, IK_ROLE_USER, trimmed);
ik_openai_conversation_add_msg(agent->conversation, msg);

// New API (add in parallel)
ik_message_t *new_msg = ik_message_create_text(agent, IK_ROLE_USER, trimmed);
res_t res = ik_agent_add_message(agent, new_msg);
if (is_err(&res)) {
    // Log error but continue (old API is authoritative for now)
    ik_log_error("Failed to add message to new storage: %s", res.err->msg);
}
```

**If exact pattern not found:**
Use grep to find all occurrences:
```bash
grep -n "ik_openai_msg_create" src/repl_actions_llm.c
grep -n "ik_openai_conversation_add_msg" src/repl_actions_llm.c
```

Then apply transformation to ALL found instances:
- After each `ik_openai_msg_create()` → add new API call with `ik_message_create_text()`
- After each `ik_openai_conversation_add_msg()` → add new API call with `ik_agent_add_message()`

**Include Updates:**
- Add `#include "message.h"` after existing includes

### 2. Update src/repl_event_handlers.c

**Location:** In the HTTP completion callback where assistant responses are processed and stored - search for where assistant messages are created from `agent->assistant_response`

**Current Code Pattern:**
```c
ik_msg_t *assistant_msg = ik_openai_msg_create(agent->conversation,
                                                IK_ROLE_ASSISTANT,
                                                agent->assistant_response);
ik_openai_conversation_add_msg(agent->conversation, assistant_msg);
```

**New Code Pattern (DUAL MODE):**
```c
// Old API (keep for now)
ik_msg_t *assistant_msg = ik_openai_msg_create(agent->conversation,
                                                IK_ROLE_ASSISTANT,
                                                agent->assistant_response);
ik_openai_conversation_add_msg(agent->conversation, assistant_msg);

// New API (add in parallel)
ik_message_t *new_msg = ik_message_create_text(agent, IK_ROLE_ASSISTANT,
                                                agent->assistant_response);
res_t res = ik_agent_add_message(agent, new_msg);
if (is_err(&res)) {
    ik_log_error("Failed to add assistant message to new storage: %s", res.err->msg);
}
```

**If exact pattern not found:**
Use grep to find all occurrences:
```bash
grep -n "ik_openai_msg_create" src/repl_event_handlers.c
grep -n "ik_openai_conversation_add_msg" src/repl_event_handlers.c
```

Then apply transformation to ALL found instances:
- After each `ik_openai_msg_create()` → add new API call with `ik_message_create_text()`
- After each `ik_openai_conversation_add_msg()` → add new API call with `ik_agent_add_message()`

**Include Updates:**
- Add `#include "message.h"` after existing includes

### 3. Update src/repl_tool.c

**Locations:** In the tool execution handlers - search for where tool call and tool result messages are created

**Tool Call Creation Pattern:**
```c
// Current pattern (appears in multiple places):
ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(agent->conversation,
                                                          tool_call_id,
                                                          tool_name,
                                                          args_json);
ik_openai_conversation_add_msg(agent->conversation, tool_call_msg);
```

**New Code (DUAL MODE):**
```c
// Old API (keep for now)
ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(agent->conversation,
                                                          tool_call_id,
                                                          tool_name,
                                                          args_json);
ik_openai_conversation_add_msg(agent->conversation, tool_call_msg);

// New API (add in parallel)
ik_message_t *new_call = ik_message_create_tool_call(agent, tool_call_id,
                                                      tool_name, args_json);
res_t res = ik_agent_add_message(agent, new_call);
if (is_err(&res)) {
    ik_log_error("Failed to add tool call to new storage: %s", res.err->msg);
}
```

**Tool Result Creation Pattern:**
```c
// Current pattern - search for where tool result messages are created:
ik_msg_t *tool_result_msg = ik_openai_msg_create_tool_result(agent->conversation,
                                                              tool_call_id,
                                                              tool_name,
                                                              output,
                                                              success,
                                                              summary);
ik_openai_conversation_add_msg(agent->conversation, tool_result_msg);
```

**New Code (DUAL MODE):**
```c
// Old API (keep for now)
ik_msg_t *tool_result_msg = ik_openai_msg_create_tool_result(agent->conversation,
                                                              tool_call_id,
                                                              tool_name,
                                                              output,
                                                              success,
                                                              summary);
ik_openai_conversation_add_msg(agent->conversation, tool_result_msg);

// New API (add in parallel)
ik_message_t *new_result = ik_message_create_tool_result(agent, tool_call_id,
                                                          output, !success);
res_t res = ik_agent_add_message(agent, new_result);
if (is_err(&res)) {
    ik_log_error("Failed to add tool result to new storage: %s", res.err->msg);
}
```

**If exact pattern not found:**
Use grep to find all occurrences:
```bash
grep -n "ik_openai_msg_create_tool" src/repl_tool.c
grep -n "ik_openai_conversation_add_msg" src/repl_tool.c
```

Then apply transformation to ALL found instances:
- After each `ik_openai_msg_create_tool_call()` → add new API call with `ik_message_create_tool_call()`
- After each `ik_openai_msg_create_tool_result()` → add new API call with `ik_message_create_tool_result()`
- After each `ik_openai_conversation_add_msg()` → add new API call with `ik_agent_add_message()`

**Include Updates:**
- Add `#include "message.h"` after existing includes

### 4. Update Include Statements

All three files need:
```c
#include "message.h"
```

Added in alphabetical order with other project headers (after own header, before system headers per style guide).

## Test Requirements

### Update Existing Tests

**tests/unit/repl/event_handlers_test.c:**
- Update tests to verify both `conversation` and `messages` are populated
- After HTTP completion, check `agent->conversation->message_count` matches `agent->message_count`
- Verify content matches between old and new storage

**tests/unit/repl/tool_test.c:**
- Update tool call tests to verify dual storage
- Verify tool result messages exist in both arrays
- Check content blocks match legacy message content

### Create New Integration Test

**tests/integration/repl/dual_mode_test.c:**

**Test Cases:**
- `test_user_message_dual_storage` - Submit user message, verify both storages populated
- `test_assistant_message_dual_storage` - HTTP response, verify both storages
- `test_tool_call_dual_storage` - Tool invocation, verify both storages
- `test_tool_result_dual_storage` - Tool result, verify both storages
- `test_message_arrays_match` - After full conversation, verify old and new arrays match

## Postconditions

- [ ] `src/repl_actions_llm.c` uses new API (dual mode)
- [ ] `src/repl_event_handlers.c` uses new API (dual mode)
- [ ] `src/repl_tool.c` uses new API for tool calls and results (dual mode)
- [ ] All three files include `message.h`
- [ ] Existing tests updated to verify dual storage
- [ ] New integration test verifies arrays stay synchronized
- [ ] `make check` passes
- [ ] No compiler warnings

## Success Criteria

After this task:
1. REPL code populates BOTH storage mechanisms
2. Old `conversation` array remains authoritative (used by providers)
3. New `messages` array tracks same content
4. Tests verify synchronization between old and new storage
5. All existing functionality continues working
6. No behavioral changes visible to users
