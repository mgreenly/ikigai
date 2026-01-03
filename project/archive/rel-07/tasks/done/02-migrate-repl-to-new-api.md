# Task: Migrate REPL to New Message API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** 01-create-message-api.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Replace ALL calls to `ik_openai_conversation_*` and `ik_openai_msg_*` functions in REPL code with new provider-agnostic message API. Remove all references to `agent->conversation`. Fix all compilation errors and test failures. After this task, REPL code uses ONLY the new API.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load log` - Logging API
- `/load naming` - Naming conventions
- `/load style` - Code style

**Source Files to Modify:**
- `src/repl_actions_llm.c` - User message creation
- `src/repl_event_handlers.c` - Assistant message creation
- `src/repl_tool.c` - Tool call/result creation
- `src/commands_fork.c` - Fork prompt message
- `src/commands_basic.c` - Clear command
- `src/agent.c` - Fork operation (if has conversation cloning)

## Implementation

### 1. Find and Replace Pattern

Use grep to find all occurrences:
```bash
grep -rn "ik_openai_msg_create" src/ --include="*.c"
grep -rn "ik_openai_conversation_" src/ --include="*.c"
grep -rn "agent->conversation" src/ --include="*.c"
```

For each file, apply transformations below.

### 2. Update src/repl_actions_llm.c

**Find:** `ik_openai_msg_create(agent->conversation, IK_ROLE_USER, ...)`
**Replace with:**
```c
ik_message_t *msg = ik_message_create_text(agent, IK_ROLE_USER, trimmed);
res_t res = ik_agent_add_message(agent, msg);
if (is_err(&res)) {
    ik_log_error("Failed to add user message: %s", res.err->msg);
    return res;
}
```

**Remove includes:**
```c
#include "openai/client.h"  // DELETE
```

**Add includes:**
```c
#include "message.h"
```

### 3. Update src/repl_event_handlers.c

**Find:** `ik_openai_msg_create(agent->conversation, IK_ROLE_ASSISTANT, ...)`
**Replace with:**
```c
ik_message_t *msg = ik_message_create_text(agent, IK_ROLE_ASSISTANT, agent->assistant_response);
res_t res = ik_agent_add_message(agent, msg);
if (is_err(&res)) {
    ik_log_error("Failed to add assistant message: %s", res.err->msg);
    // Continue - not fatal
}
```

**Remove includes:**
```c
#include "openai/client.h"  // DELETE
```

**Add includes:**
```c
#include "message.h"
```

### 4. Update src/repl_tool.c

**Find tool call creation:**
```c
ik_openai_msg_create_tool_call(agent->conversation, tool_call_id, tool_name, args_json)
```

**Replace with:**
```c
ik_message_t *call_msg = ik_message_create_tool_call(agent, tool_call_id, tool_name, args_json);
res_t res = ik_agent_add_message(agent, call_msg);
if (is_err(&res)) {
    ik_log_error("Failed to add tool call: %s", res.err->msg);
}
```

**Find tool result creation:**
```c
ik_openai_msg_create_tool_result(agent->conversation, tool_call_id, tool_name, output, success, summary)
```

**Replace with:**
```c
ik_message_t *result_msg = ik_message_create_tool_result(agent, tool_call_id, output, !success);
res_t res = ik_agent_add_message(agent, result_msg);
if (is_err(&res)) {
    ik_log_error("Failed to add tool result: %s", res.err->msg);
}
```

**Note:** `success` boolean is inverted to `is_error` for new API.

**Remove includes:**
```c
#include "openai/client.h"  // DELETE
```

**Add includes:**
```c
#include "message.h"
```

### 5. Update src/commands_fork.c

**Find:** `ik_openai_msg_create(child->conversation, IK_ROLE_USER, prompt_text)`
**Replace with:**
```c
ik_message_t *msg = ik_message_create_text(child, IK_ROLE_USER, prompt_text);
res_t res = ik_agent_add_message(child, msg);
if (is_err(&res)) {
    return res;
}
```

**Remove includes:**
```c
#include "openai/client.h"  // DELETE
```

**Add includes:**
```c
#include "message.h"
```

### 6. Update src/commands_basic.c

**Find:** `ik_openai_conversation_clear(agent->conversation)`
**Replace with:**
```c
ik_agent_clear_messages(agent);
```

**Remove includes (if present):**
```c
#include "openai/client.h"  // DELETE
```

### 7. Update src/agent.c fork operation

If there's conversation cloning code like:
```c
child->conversation = ik_openai_conversation_create(child);
for (size_t i = 0; i < parent->conversation->message_count; i++) {
    // ... clone messages ...
}
```

**Replace entire section with:**
```c
res_t res = ik_agent_clone_messages(child, parent);
if (is_err(&res)) {
    talloc_free(child);
    return res;
}
```

**Remove includes:**
```c
#include "openai/client.h"  // DELETE (if present)
```

### 8. Update ALL test files

Find all test files referencing `agent->conversation`:
```bash
grep -rn "agent->conversation" tests/ --include="*.c"
```

For each occurrence:
- `agent->conversation->message_count` → `agent->message_count`
- `agent->conversation->messages[i]` → `agent->messages[i]`
- Remove any dual-mode checks

Update test includes:
- Remove `#include "openai/client.h"`
- Add `#include "message.h"` if needed

## Build Verification

```bash
make clean
make all
```

**Expected:**
- Build SUCCEEDS
- No undefined references to `ik_openai_*` functions
- No references to `agent->conversation`

```bash
make check
```

**Expected:**
- ALL tests pass
- If any tests fail, fix them by updating assertions to use new API

## Debugging Failed Tests

If tests fail:
1. Read test output carefully
2. Identify which test failed
3. Update test to use new API (`agent->messages` instead of `agent->conversation`)
4. Rerun until all pass

## Postconditions

- [ ] All `ik_openai_msg_create*` calls replaced with `ik_message_create_*`
- [ ] All `ik_openai_conversation_*` calls replaced with `ik_agent_*`
- [ ] All `agent->conversation` references replaced with `agent->messages`
- [ ] All REPL files include `message.h` not `openai/client.h`
- [ ] Build succeeds
- [ ] `make check` passes (ALL tests)

## Success Criteria

After this task:
1. REPL code uses ONLY new message API
2. No references to legacy conversation API outside src/openai/
3. Build succeeds with no errors
4. All tests pass
5. Ready to migrate provider code
