# Task: Create Provider-Agnostic Message Storage (Dual Mode)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Create provider-agnostic message storage functions and add dual-mode support to agent struct. The agent will maintain both old (`conversation`) and new (`messages`) storage simultaneously during migration. Both are populated in parallel so all existing code continues working while we incrementally migrate to the new API.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load database` - Database schema, ik_msg_t structure, data_json format
- `/load log` - Logging API (ik_log_error, ik_log_debug for debugging)
- `/load naming` - Naming conventions (ik_ prefix, approved abbreviations)
- `/load style` - Code style (no static functions, include order)
- `/load tdd` - Test-driven development

**Source Files to Read:**
- `src/providers/provider.h` - Study complete struct definitions:
  - `struct ik_message`: role, content_blocks (array pointer), content_count, provider_metadata
  - `struct ik_content_block`: type discriminator + union with 4 variants:
    - text: {char *text}
    - tool_call: {char *id, char *name, char *arguments}
    - tool_result: {char *tool_call_id, char *content, bool is_error}
    - thinking: {char *text}
  - `ik_role_t` enum: IK_ROLE_USER=0, IK_ROLE_ASSISTANT=1, IK_ROLE_TOOL=2
  - `ik_content_type_t` enum: IK_CONTENT_TEXT=0, IK_CONTENT_TOOL_CALL=1, IK_CONTENT_TOOL_RESULT=2, IK_CONTENT_THINKING=3
- `src/providers/request.c` - Study existing `ik_content_block_text()`, `ik_content_block_tool_call()`, `ik_content_block_tool_result()` functions
- `src/agent.h` - Study `ik_agent_ctx_t` struct - locate conversation field to understand dual-mode addition
- `src/openai/client.h` - Study `ik_openai_conversation_t` to understand what we're replacing
- `src/db/message.h` - Study `ik_msg_t` for database format conversion

**Test Pattern Files:**
- `tests/unit/db/message_test.c` - Pattern for message creation tests
- `tests/unit/agent/fork_test.c` - Pattern for agent operation tests

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

### 1. Create src/message.c and src/message.h

**New Public Functions:**

```c
// Message creation (wraps content block creation)
ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text);
ik_message_t *ik_message_create_tool_call(TALLOC_CTX *ctx, const char *id,
                                          const char *name, const char *arguments);
ik_message_t *ik_message_create_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                             const char *content, bool is_error);

// Database conversion helper
ik_message_t *ik_message_from_db_msg(TALLOC_CTX *ctx, const ik_msg_t *db_msg);
```

**Behaviors:**

- `ik_message_create_text()`:
  - Create `ik_message_t` with given role (USER/ASSISTANT/TOOL)
  - Allocate single content block using `ik_content_block_text()`
  - Set `content_blocks` array, `content_count=1`, `provider_metadata=NULL`
  - Return allocated message (PANIC on OOM)

- `ik_message_create_tool_call()`:
  - Create `ik_message_t` with role=ASSISTANT
  - Allocate single content block using `ik_content_block_tool_call()`
  - Set `content_blocks` array, `content_count=1`, `provider_metadata=NULL`
  - Return allocated message (PANIC on OOM)

- `ik_message_create_tool_result()`:
  - Create `ik_message_t` with role=TOOL
  - Allocate single content block using `ik_content_block_tool_result()`
  - Set `content_blocks` array, `content_count=1`, `provider_metadata=NULL`
  - Return allocated message (PANIC on OOM)

- `ik_message_from_db_msg()`:
  - Convert database format (`ik_msg_t`: id/kind/content/data_json) to provider format
  - Map kind to role:
    - "user" → USER
    - "assistant" → ASSISTANT
    - "tool_call" → ASSISTANT
    - "tool_result" → TOOL
    - "tool" → TOOL (legacy)
    - "system" → return NULL (system prompts NOT stored in message array, handled via agent->system_prompt field)
  - For "tool_call": parse data_json for tool call fields, create tool_call content block
  - For "tool_result" or "tool": parse data_json for result fields, create tool_result content block
  - For other kinds (except "system"): create text content block from `content` field
  - Return allocated message, or NULL for system messages (PANIC on OOM, return ERR for parse failures)

**data_json Format Specification:**

Tool call JSON (stored in database for kind="tool_call"):
```json
{
  "tool_call_id": "call_abc123",    // string, required
  "name": "file_read",               // string, required
  "arguments": "{\"path\":\"foo\"}"  // JSON string, required
}
```

Tool result JSON (stored in database for kind="tool_result" or kind="tool"):
```json
{
  "tool_call_id": "call_abc123",  // string, required
  "name": "file_read",             // string, optional (legacy, can be null)
  "output": "file contents...",    // string, required
  "success": true,                 // boolean, required
  "summary": "Read 100 bytes"      // string, optional (legacy, can be null)
}
```

**yyjson Parsing Pattern:**

```c
// Parse data_json
yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
if (!doc) {
    return ERR(ctx, PARSE_ERROR, "Invalid JSON in data_json");
}

yyjson_val *root = yyjson_doc_get_root(doc);
if (!yyjson_is_obj(root)) {
    yyjson_doc_free(doc);
    return ERR(ctx, PARSE_ERROR, "data_json root is not object");
}

// Extract string field
yyjson_val *field_val = yyjson_obj_get(root, "field_name");
if (!yyjson_is_str(field_val)) {
    yyjson_doc_free(doc);  // Always free before returning
    return ERR(ctx, PARSE_ERROR, "Missing or invalid field_name");
}
const char *field = yyjson_get_str(field_val);
char *field_copy = talloc_strdup(msg, field);  // Copy to talloc context

// Extract boolean field
yyjson_val *bool_val = yyjson_obj_get(root, "success");
bool success = yyjson_is_true(bool_val);  // Defaults to false if missing

// Always free doc after extraction
yyjson_doc_free(doc);
```

### 2. Add Agent Message Management to src/agent.c

**New Public Functions:**

```c
res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg);
void ik_agent_clear_messages(ik_agent_ctx_t *agent);
res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src);
```

**Behaviors:**

- `ik_agent_add_message()`:
  - Validate agent and msg not NULL (assert)
  - If `messages` array NULL or at capacity: grow using geometric expansion
    - Initial capacity: 16
    - Growth factor: 2x (double capacity when full)
    - Realloc pattern: `agent->messages = talloc_realloc(agent, agent->messages, ik_message_t*, new_capacity)`
    - Update `agent->message_capacity = new_capacity`
  - Reparent msg to agent context using `talloc_steal()`
  - Add msg pointer to `messages[message_count++]`
  - Return OK(msg)

- `ik_agent_clear_messages()`:
  - Validate agent not NULL (assert)
  - Free `messages` array (talloc_free handles children)
  - Set `messages=NULL`, `message_count=0`, `message_capacity=0`

- `ik_agent_clone_messages()`:
  - Validate dest and src not NULL (assert)
  - If src has no messages, return OK immediately
  - Allocate new messages array in dest context with src->message_count capacity
  - Deep copy each message and its content blocks
  - Set dest->messages, dest->message_count, dest->message_capacity
  - Return OK(dest->messages)

### 3. Update src/agent.h

Add new fields to `ik_agent_ctx_t` struct in the "Conversation state" section:

```c
    // Conversation state (per-agent)
    ik_openai_conversation_t *conversation;  // DEPRECATED - will be removed

    // NEW: Provider-agnostic message storage
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity

    ik_mark_t **marks;
    size_t mark_count;
```

Add function declarations after existing agent functions.

### 4. Update src/agent.c initialization

In `ik_agent_create()`:
- After creating `conversation`, initialize new fields: `messages=NULL`, `message_count=0`, `message_capacity=0`

## Test Requirements

Create `tests/unit/message/creation_test.c`:

**Test Cases:**
- `test_message_create_text_user` - Create user text message, verify role and content
- `test_message_create_text_assistant` - Create assistant text message
- `test_message_create_tool_call` - Create tool call message, verify fields
- `test_message_create_tool_result` - Create tool result message
- `test_message_from_db_msg_user` - Convert database user message
- `test_message_from_db_msg_assistant` - Convert database assistant message
- `test_message_from_db_msg_tool_call` - Convert database tool_call with data_json parsing
- `test_message_from_db_msg_tool_result` - Convert database tool_result with data_json parsing

Create `tests/unit/agent/message_management_test.c`:

**Test Cases:**
- `test_agent_add_message_single` - Add one message, verify count
- `test_agent_add_message_multiple` - Add multiple messages, verify array growth
- `test_agent_add_message_capacity_growth` - Add 20 messages, verify geometric growth
- `test_agent_clear_messages` - Add messages, clear, verify empty
- `test_agent_clone_messages` - Clone between agents, verify deep copy
- `test_agent_clone_messages_empty` - Clone from agent with no messages

Update existing `tests/unit/agent/fork_test.c`:
- Verify dual mode: after fork, both `conversation` and `messages` are populated
- Verify `messages` array matches `conversation` content

## Build Integration

Update `Makefile`:
- Add `src/message.c` to `CLIENT_SOURCES`
- Add `tests/unit/message/creation_test` to unit test targets
- Add `tests/unit/agent/message_management_test` to unit test targets

## Postconditions

- [ ] `src/message.c` and `src/message.h` created with 4 public functions
- [ ] `src/agent.c` has 3 new message management functions
- [ ] `src/agent.h` has new fields in struct (dual mode)
- [ ] Agent creation initializes new fields to NULL/0
- [ ] All new tests pass
- [ ] `make check` passes (all existing tests still work)
- [ ] No compiler warnings

## Success Criteria

After this task:
1. New message creation functions exist and are tested
2. Agent struct has BOTH old and new storage fields
3. Agent initialization sets up both storage mechanisms
4. All existing code continues working (uses old `conversation` field)
5. New code can start using `messages` field via new API
6. Tests verify both storage mechanisms work independently
