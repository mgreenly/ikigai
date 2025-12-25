# Task: Migrate Restore to New Message API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** 03-migrate-providers-to-new-storage.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Update conversation restore code to load messages from database into new `agent->messages` storage using `ik_message_from_db_msg()`. System prompts are NOT added to message array (handled via agent->system_prompt string field).

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load database` - Database schema and replay API
- `/load naming` - Naming conventions
- `/load style` - Code style

**Source Files to Modify:**
- `src/repl/agent_restore.c` - System prompt setup
- `src/repl/agent_restore_replay.c` - Message replay loop

## Implementation

### 1. Update src/repl/agent_restore.c

**Find:** System message creation during restore

**Current code:**
```c
ik_msg_t *system_msg = ik_openai_msg_create(agent->conversation, IK_ROLE_SYSTEM, system_prompt);
ik_openai_conversation_add_msg(agent->conversation, system_msg);
```

**New code:**
```c
/* System prompts are NOT added to message array in new API */
/* They are handled via agent->system_prompt string field */
/* The request builder uses ik_request_set_system() to add them */
/* No action needed here - system prompt already stored in agent struct */
```

**Delete the old code entirely.**

**Remove includes:**
```c
#include "openai/client.h"  // DELETE
```

**Add includes:**
```c
#include "message.h"
```

### 2. Update src/repl/agent_restore_replay.c

**Find:** Message replay loop

**Current code:**
```c
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *msg = replay_ctx->messages[i];

    if (ik_msg_is_conversation_kind(msg->kind)) {
        ik_openai_conversation_add_msg(agent->conversation, msg);
    }
}
```

**New code:**
```c
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *db_msg = replay_ctx->messages[i];

    if (ik_msg_is_conversation_kind(db_msg->kind)) {
        /* Convert database format to provider format */
        ik_message_t *provider_msg = NULL;
        res_t res = ik_message_from_db_msg(agent, db_msg, &provider_msg);

        if (is_err(&res)) {
            /* JSON parse error or other conversion failure */
            ik_log_error("Failed to convert database message kind=%s: %s",
                         db_msg->kind, res.err->msg);
            /* Continue with next message - don't fail entire restore */
            continue;
        }

        if (provider_msg == NULL) {
            /* NULL means system message - skip (handled via agent->system_prompt) */
            continue;
        }

        /* Add to agent's message array */
        res = ik_agent_add_message(agent, provider_msg);
        if (is_err(&res)) {
            ik_log_error("Failed to add replayed message: %s", res.err->msg);
            /* Continue with next message - don't fail entire restore */
        }
    }
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

## Test Requirements

### Update Existing Tests

**tests/unit/repl/agent_restore_test.c:**
- After restore, verify `agent->message_count > 0` (if session had non-system messages)
- Verify `agent->messages` array populated
- System message should NOT be in array (handled separately)

**tests/integration/agent/restore_test.c:**
- Full restore test: create session, add messages, restart, restore
- Verify messages populated correctly
- Verify tool calls and results restored with correct content blocks

### Create New Test

**tests/unit/message/db_conversion_test.c:**

Test cases:
- test_from_db_msg_user - Convert user message, verify OK and provider_msg fields
- test_from_db_msg_assistant - Convert assistant message, verify OK
- test_from_db_msg_system - Verify returns OK with provider_msg set to NULL
- test_from_db_msg_tool_call - Parse data_json, verify OK and tool_call content block
- test_from_db_msg_tool_result - Parse data_json, verify OK and tool_result content block
- test_from_db_msg_invalid_json - Verify returns ERR for malformed data_json

**data_json examples for tests:**

Tool call JSON:
```json
{
  "tool_call_id": "call_abc123",
  "name": "file_read",
  "arguments": "{\"path\":\"foo.txt\"}"
}
```

Tool result JSON:
```json
{
  "tool_call_id": "call_abc123",
  "name": "file_read",
  "output": "file contents here",
  "success": true
}
```

## Build Verification

```bash
make clean
make all
make check
```

**Expected:**
- Build succeeds
- All tests pass
- Restore functionality works
- System messages handled correctly (not in array)

## Postconditions

- [ ] src/repl/agent_restore.c does NOT add system messages to array
- [ ] src/repl/agent_restore_replay.c uses `ik_message_from_db_msg()`
- [ ] Database messages converted to provider format
- [ ] Tool calls/results parsed from data_json
- [ ] Include statements updated
- [ ] Database conversion tests created
- [ ] All restore tests pass
- [ ] Build succeeds
- [ ] `make check` passes

## Success Criteria

After this task:
1. Session restore populates new storage
2. Database messages correctly converted to provider format
3. Tool calls and results properly parsed from data_json
4. System messages skipped (NULL return)
5. All restore tests pass
6. Ready to delete legacy OpenAI code
