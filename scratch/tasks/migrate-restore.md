# Task: Migrate Conversation Restore to New API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** migrate-agent-ops.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Update conversation restore code to populate new provider-agnostic message storage. During restore, load messages from database into BOTH old `conversation` and new `messages` arrays. Use `ik_message_from_db_msg()` to convert database format to provider format.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load database` - Database schema and replay API
- `/load naming` - Naming conventions
- `/load style` - Code style
- `/load tdd` - Test-driven development

**Source Files to Read:**
- `src/message.h` - New message API including `ik_message_from_db_msg()`
- `src/db/message.h` - Database message structure (`ik_msg_t`)
- `src/db/replay.h` - Replay context and message loading
- `src/repl/agent_restore.c` - System prompt setup (line 40-80)
- `src/repl/agent_restore_replay.c` - Message replay logic (line 30-120)

**Test Files:**
- `tests/unit/repl/agent_restore_test.c` - Restore operation tests
- `tests/integration/agent/restore_test.c` - End-to-end restore tests

## Implementation

### 1. Update src/repl/agent_restore.c - System Prompt

**Location:** In the function that restores system prompt - search for where system message is created and added during agent restore

**Current Code Pattern:**
```c
ik_msg_t *system_msg = ik_openai_msg_create(agent->conversation, IK_ROLE_SYSTEM, system_prompt);
ik_openai_conversation_add_msg(agent->conversation, system_msg);
```

**New Code (DUAL MODE):**
```c
// Old API (keep for now)
ik_msg_t *system_msg = ik_openai_msg_create(agent->conversation, IK_ROLE_SYSTEM, system_prompt);
ik_openai_conversation_add_msg(agent->conversation, system_msg);

// New API: System prompts are NOT added to message array
// They are handled via request->system_prompt field when building requests.
// No action needed here - the new provider system handles system prompts differently.
// The agent context will have system prompt set elsewhere (in agent configuration).
```

**Include Updates:**
- Add `#include "message.h"` after existing includes

### 2. Update src/repl/agent_restore_replay.c - Message Replay

**Location:** In the replay function - search for the loop that processes messages from replay context and adds them to conversation

**Current Code Pattern:**
```c
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *msg = replay_ctx->messages[i];

    if (ik_msg_is_conversation_kind(msg->kind)) {
        ik_openai_conversation_add_msg(agent->conversation, msg);
    }
}
```

**New Code (DUAL MODE):**
```c
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *db_msg = replay_ctx->messages[i];

    if (ik_msg_is_conversation_kind(db_msg->kind)) {
        // Old API (keep for now)
        ik_openai_conversation_add_msg(agent->conversation, db_msg);

        // New API (add in parallel)
        ik_message_t *provider_msg = ik_message_from_db_msg(agent, db_msg);
        if (provider_msg != NULL) {
            res_t res = ik_agent_add_message(agent, provider_msg);
            if (is_err(&res)) {
                ik_log_error("Failed to add replayed message to new storage: %s", res.err->msg);
            }
        }
        // Note: ik_message_from_db_msg() returns NULL for system messages (intentional skip)
        // Only log error if conversion fails for non-system messages
    }
}
```

**Include Updates:**
- Add `#include "message.h"` after existing includes

### 3. Handle Database Message Conversion Edge Cases

**In `ik_message_from_db_msg()` implementation:**

Database message kinds that need special handling:
- `"system"` → SKIP (system prompts handled via request->system_prompt field, not in message array)
- `"user"` → Create text message with USER role
- `"assistant"` → Create text message with ASSISTANT role
- `"tool_call"` → Parse data_json, create tool_call message with ASSISTANT role
- `"tool_result"` → Parse data_json, create tool_result message with TOOL role
- `"tool"` → Parse data_json, create tool_result message with TOOL role (legacy name)

**data_json Format Examples:**

Tool call (from database):
```json
{
  "tool_call_id": "call_abc123",
  "name": "file_read",
  "arguments": "{\"path\":\"foo.txt\"}"
}
```

Tool result (from database):
```json
{
  "tool_call_id": "call_abc123",
  "name": "file_read",
  "output": "file contents...",
  "success": true
}
```

Parse these fields using yyjson and construct appropriate content blocks.

## Test Requirements

### Update Existing Tests

**tests/unit/repl/agent_restore_test.c:**
- After restore, verify `agent->message_count > 0` (if conversation had non-system messages)
- NOTE: message counts will NOT match due to system message skip
- Verify `agent->message_count == agent->conversation->message_count - 1` (accounting for system message)
- First message in new storage should be first user/assistant message, not system prompt

**tests/integration/agent/restore_test.c:**
- Full restore test: create session, add messages, restart, restore
- Verify both storages populated correctly
- Verify tool calls and results restored with correct content blocks

### Create New Tests

**tests/unit/message/db_conversion_test.c:**

**Test Cases:**
- `test_from_db_msg_user` - Convert user message, verify role and content
- `test_from_db_msg_assistant` - Convert assistant message
- `test_from_db_msg_system` - Verify returns NULL (system messages skipped)
- `test_from_db_msg_tool_call` - Parse data_json, verify tool_call content block
- `test_from_db_msg_tool_result` - Parse data_json, verify tool_result content block
- `test_from_db_msg_tool_legacy` - Handle legacy "tool" kind
- `test_from_db_msg_invalid_json` - Handle malformed data_json gracefully
- `test_from_db_msg_null_content` - Handle NULL content field

**tests/integration/repl/restore_dual_mode_test.c:**

**Test Cases:**
- `test_restore_text_messages` - Restore session with user/assistant messages
- `test_restore_tool_calls` - Restore session with tool invocations
- `test_restore_mixed_conversation` - Restore complex conversation
- `test_restore_empty_session` - Restore session with no messages
- `test_restore_dual_storage_match` - Verify new storage has all non-system messages from old storage

## Postconditions

- [ ] `src/repl/agent_restore.c` adds system prompt to old storage (new storage skips system messages)
- [ ] `src/repl/agent_restore_replay.c` replays conversation messages (skipping system) to new storage
- [ ] `ik_message_from_db_msg()` handles all message kinds correctly (returns NULL for system)
- [ ] data_json parsing works for tool calls and results
- [ ] Include statements updated in modified files
- [ ] Database conversion tests created and passing
- [ ] Existing restore tests updated to verify dual storage
- [ ] Integration tests verify end-to-end restore
- [ ] `make check` passes
- [ ] No compiler warnings

## Success Criteria

After this task:
1. Session restore populates BOTH storage mechanisms
2. Database messages correctly converted to provider format
3. Tool calls and results properly parsed from data_json
4. Both storages synchronized after restore operation
5. All restore tests verify dual mode operation
6. Edge cases (NULL content, invalid JSON) handled gracefully
