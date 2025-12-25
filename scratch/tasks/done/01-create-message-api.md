# Task: Create Provider-Agnostic Message API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Create new provider-agnostic message API (src/message.c/h) and agent message management functions. Update agent struct to use ONLY new storage (no dual mode). This replaces the legacy OpenAI conversation API with a clean provider-agnostic interface.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load database` - Database schema, ik_msg_t structure
- `/load naming` - Naming conventions
- `/load style` - Code style
- `/load tdd` - Test-driven development

**Source Files to Read:**
- `src/providers/provider.h` - Study complete struct definitions:
  - `struct ik_message`: role, content_blocks (array pointer), content_count
  - `struct ik_content_block`: type discriminator + union (text/tool_call/tool_result/thinking)
  - `ik_role_t` enum: IK_ROLE_USER, IK_ROLE_ASSISTANT, IK_ROLE_TOOL
  - `ik_content_type_t` enum: IK_CONTENT_TEXT, IK_CONTENT_TOOL_CALL, IK_CONTENT_TOOL_RESULT, IK_CONTENT_THINKING
- `src/providers/request.c` - Study existing content block creation functions
- `src/agent.h` - Current agent struct (has conversation field - we'll replace it)
- `src/openai/client.h` - Study ik_openai_conversation_t to understand what we're replacing
- `src/db/message.h` - Study ik_msg_t for database format conversion

## Implementation

### 1. Create src/message.c and src/message.h

**New Public Functions:**

```c
// Message creation
ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text);
ik_message_t *ik_message_create_tool_call(TALLOC_CTX *ctx, const char *id,
                                          const char *name, const char *arguments);
ik_message_t *ik_message_create_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                             const char *content, bool is_error);

// Database conversion (sets *out to NULL for system messages - they go in request->system_prompt)
res_t ik_message_from_db_msg(TALLOC_CTX *ctx, const ik_msg_t *db_msg, ik_message_t **out);
```

**Implementation Notes:**

Each create function:
- Allocates ik_message_t with talloc
- Creates single content block using existing `ik_content_block_*` functions from request.c
- Sets content_blocks array, content_count=1, provider_metadata=NULL
- PANIC on OOM with LCOV_EXCL_BR_LINE

`ik_message_from_db_msg`:
- Parse db_msg->kind to determine message type
- "system" → set *out to NULL, return OK (system prompts handled differently)
- "user"/"assistant" → create text message from content field, set *out, return OK
- "tool_call" → parse data_json, create tool_call message, set *out, return OK
- "tool_result"/"tool" → parse data_json, create tool_result message, set *out, return OK
- Return ERR for JSON parse failures (error allocated on ctx, *out untouched)

**data_json parsing:**
- Use yyjson to parse JSON strings
- Tool call: extract tool_call_id, name, arguments
- Tool result: extract tool_call_id, output, success (map to is_error)

### 2. Add Agent Message Management to src/agent.c

**New Public Functions:**

```c
res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg);
void ik_agent_clear_messages(ik_agent_ctx_t *agent);
res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src);
```

**Behaviors:**

`ik_agent_add_message`:
- Grow messages array if needed (geometric growth, initial capacity 16)
- Reparent msg to agent context (talloc_steal)
- Add to messages[message_count++]
- Return OK(msg)

`ik_agent_clear_messages`:
- Free messages array (talloc_free)
- Set messages=NULL, message_count=0, message_capacity=0

`ik_agent_clone_messages`:
- Deep copy all messages from src to dest
- Allocate new array in dest context
- Deep copy each message and its content blocks
- Return OK(dest->messages)

### 3. Update src/agent.h

**Replace conversation field:**

OLD (delete this):
```c
    // Conversation state
    ik_openai_conversation_t *conversation;
```

NEW (use this):
```c
    // Conversation state (per-agent)
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity
```

**Remove forward declaration:**
```c
typedef struct ik_openai_conversation ik_openai_conversation_t;  // DELETE
```

**Add function declarations** after existing agent functions.

### 4. Update src/agent.c initialization

In `ik_agent_create()`:
- DELETE: `agent->conversation = ik_openai_conversation_create(agent);`
- ADD: `agent->messages = NULL; agent->message_count = 0; agent->message_capacity = 0;`

In fork operation (if it exists):
- DELETE: Any conversation cloning code
- ADD: Call to `ik_agent_clone_messages(child, parent)`

## Test Requirements

Create `tests/unit/message/creation_test.c`:
- test_message_create_text_user - Create user text message
- test_message_create_text_assistant - Create assistant text message
- test_message_create_tool_call - Create tool call, verify all fields
- test_message_create_tool_result - Create tool result
- test_message_from_db_msg_user - Convert DB user message, verify OK and *out set
- test_message_from_db_msg_tool_call - Parse data_json for tool call, verify OK
- test_message_from_db_msg_tool_result - Parse data_json for tool result, verify OK
- test_message_from_db_msg_system - Verify returns OK with *out set to NULL
- test_message_from_db_msg_invalid_json - Verify returns ERR for malformed data_json

Create `tests/unit/agent/message_management_test.c`:
- test_agent_add_message_single - Add one message
- test_agent_add_message_growth - Add 20 messages, verify capacity grows
- test_agent_clear_messages - Add then clear
- test_agent_clone_messages - Clone and verify deep copy

Update `Makefile`:
- Add `src/message.c` to CLIENT_SOURCES
- Add test targets for new tests

## Build Verification

**IMPORTANT:** After removing `agent->conversation` field, you will see compilation errors in:
- REPL files (repl_actions_llm.c, repl_event_handlers.c, repl_tool.c, commands_*.c)
- Restore files (agent_restore.c, agent_restore_replay.c)
- Test files that reference conversation

**This is EXPECTED and ACCEPTABLE.** Task 02 will fix REPL, Task 04 will fix restore.

For now, temporarily comment out broken files from Makefile CLIENT_SOURCES to verify new code compiles:

```bash
# Comment out in Makefile temporarily:
# src/repl_actions_llm.c
# src/repl_event_handlers.c
# src/repl_tool.c
# src/commands_fork.c
# src/commands_basic.c
# src/repl/agent_restore.c
# src/repl/agent_restore_replay.c

make clean
make all       # Should succeed with reduced source set
make check     # New tests should pass, commented-out tests won't run
```

**Expected:**
- Reduced build succeeds (new message.c compiles)
- New tests pass (creation_test.c, message_management_test.c)
- Full build will be restored in Task 02

**Alternative approach:** Keep full build but expect failures until Task 02 complete.

## Postconditions

- [ ] src/message.c and src/message.h created with 4 functions
- [ ] src/agent.c has 3 message management functions
- [ ] src/agent.h has ONLY new fields (no conversation)
- [ ] Agent creation initializes new fields
- [ ] New tests pass
- [ ] Build succeeds (old tests may fail - that's OK)

## Success Criteria

After this task:
1. New message API exists and is tested
2. Agent struct uses new storage ONLY (no dual mode)
3. New code compiles and new tests pass
4. Old code may be broken (we'll fix in next task)
5. Foundation is ready for migration
