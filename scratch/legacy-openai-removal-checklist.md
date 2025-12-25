# Legacy OpenAI Code Removal Checklist

Complete inventory of all legacy OpenAI code that must be removed or migrated.

## Summary

- **11 external files** depend on legacy OpenAI code
- **19 legacy files** in `src/openai/` to delete
- **28+ legacy functions** in use
- **7 legacy types** in use
- **1 critical struct field** blocking deletion (`agent->conversation`)

---

## Files That Use Legacy OpenAI Code (MUST BE MIGRATED)

### Core Agent Management
- `src/agent.h` - Defines `ik_openai_conversation_t *conversation` field (LINE 96)
- `src/agent.c` - Creates conversations, clones messages during fork

### REPL System
- `src/repl.h` - References `struct ik_openai_multi`
- `src/repl.c` - Main event loop
- `src/repl_init.c` - Initialization
- `src/repl_actions_llm.c` - User message creation and conversation management
- `src/repl_event_handlers.c` - Assistant message creation

### Agent Restoration
- `src/repl/agent_restore.c` - System prompt message creation
- `src/repl/agent_restore_replay.c` - Conversation replay during restore

### Commands
- `src/commands_fork.c` - User message creation for fork
- `src/commands_basic.c` - Conversation clearing (`ik_openai_conversation_clear`)

### Tool System
- `src/repl_tool.c` - Tool call and result message creation

### Infrastructure
- `src/marks.c` - References conversation type in comments
- `src/wrapper.c` - MOCKABLE wrapper for `ik_openai_conversation_add_msg_`
- `src/wrapper_internal.h` - MOCKABLE declaration for conversation functions

### Provider Factory
- `src/providers/factory.c` - Calls `ik_openai_create()` to instantiate provider
- `src/providers/stubs.h` - Declares `ik_openai_create()` stub

---

## Legacy Types (TO DELETE)

### Conversation Types
1. **`ik_openai_conversation_t`** - Legacy conversation storage
   - Defined in: `src/openai/client.h`
   - Used in: `src/agent.h` (struct field), all REPL code

### Request/Response Types
2. **`ik_openai_request_t`** - Legacy request format
   - Defined in: `src/providers/openai/request.h`
   - Used by: Shim layer to convert new→old format

3. **`ik_openai_multi_t`** - Legacy multi-handle manager
   - Defined in: `src/openai/client_multi.h`
   - Used in: `src/repl.h` forward declaration

### Stream Processing Types
4. **`ik_openai_sse_parser_t`** - SSE parser state
   - Defined in: `src/openai/sse_parser.h`
   - Used by: HTTP handler for streaming

5. **`ik_openai_chat_stream_ctx_t`** - Chat API streaming context
   - Defined in: `src/providers/openai/streaming.h`
   - Used by: Chat completion streaming

6. **`ik_openai_responses_stream_ctx_t`** - Responses API streaming context
   - Defined in: `src/providers/openai/streaming.h`
   - Used by: Responses API streaming (o1 models)

### Context Types
7. **`ik_openai_ctx_t`** - Provider implementation context
   - Defined in: `src/providers/openai/openai.c`
   - Used by: OpenAI provider vtable implementation

### Callback Types
8. **`ik_openai_stream_cb_t`** - Legacy stream callback signature
   - Defined in: `src/openai/http_handler.h`
   - Signature: `res_t (*)(const char *chunk, void *ctx)`

---

## Legacy Functions (TO DELETE OR REPLACE)

### Conversation Management (BLOCKING DELETION - IN ACTIVE USE)
1. `ik_openai_conversation_create()` - Create conversation
   - Called in: `src/agent.c` (2 places)

2. `ik_openai_conversation_add_msg()` - Add message to conversation
   - Called in: `src/repl_actions_llm.c`, `src/repl_tool.c`, `src/commands_fork.c`, `src/repl/agent_restore.c`, `src/repl/agent_restore_replay.c`, `src/agent.c`

3. `ik_openai_conversation_clear()` - Clear conversation
   - Called in: `src/commands_basic.c`

### Message Creation (BLOCKING DELETION - IN ACTIVE USE)
4. `ik_openai_msg_create()` - Create user/assistant message
   - Called in: `src/repl_actions_llm.c`, `src/commands_fork.c`, `src/repl_event_handlers.c`, `src/agent.c`, `src/providers/openai/shim.c`

5. `ik_openai_msg_create_tool_call()` - Create tool call message
   - Called in: `src/repl_tool.c` (2 places)

6. `ik_openai_msg_create_tool_result()` - Create tool result message
   - Called in: `src/repl_tool.c` (2 places)

7. `ik_openai_get_message_at_index()` - Get message from conversation
   - Called in: `src/providers/openai/request_chat.c`, `src/providers/openai/request_responses.c`

### Provider Creation
8. `ik_openai_create()` - Create OpenAI provider instance
   - Called in: `src/providers/factory.c`
   - **NOTE**: This stays but moves to `src/providers/openai/openai.c`

### Shim Functions (Used by New Provider)
9. `ik_openai_shim_build_conversation()` - Convert new→old conversation format
10. `ik_openai_shim_map_finish_reason()` - Map finish reasons

### Serialization/Parsing
11. `ik_openai_serialize_request()` - Convert request to JSON
12. `ik_openai_parse_sse_event()` - Parse SSE content delta
13. `ik_openai_parse_tool_calls()` - Parse SSE tool calls delta

### SSE Parser
14. `ik_openai_sse_parser_create()` - Create SSE parser
15. `ik_openai_sse_parser_feed()` - Feed data to parser
16. `ik_openai_sse_parser_get_event()` - Extract complete event

### HTTP Handler
17. `ik_openai_http_extract_finish_reason()` - Extract finish reason from event

### Multi-handle Operations (Legacy Async)
18. `ik_openai_multi_create()` - Create multi-handle manager
19. `ik_openai_multi_add_request()` - Add request to multi-handle
20. `ik_openai_multi_fdset()` - Get file descriptors for select()
21. `ik_openai_multi_perform()` - Perform non-blocking transfers
22. `ik_openai_multi_timeout()` - Get timeout for next action
23. `ik_openai_multi_info_read()` - Read completed transfer info

### Stream Processing
24. `ik_openai_chat_stream_ctx_create()` - Create chat stream context
25. `ik_openai_chat_stream_process_data()` - Process chat stream data
26. `ik_openai_chat_stream_get_finish_reason()` - Get chat stream finish reason
27. `ik_openai_responses_stream_get_finish_reason()` - Get responses stream finish reason

### Model Detection Utilities
28. `ik_openai_is_reasoning_model()` - Check if model is o1/o3 reasoning model
29. `ik_openai_prefer_responses_api()` - Check if should use responses API
30. `ik_openai_supports_temperature()` - Check if model supports temperature

### Finish Reason Mapping
31. `ik_openai_map_chat_finish_reason()` - Map chat API finish reason
32. `ik_openai_map_responses_status()` - Map responses API status

### Thinking/Reasoning
33. `ik_openai_reasoning_effort()` - Convert thinking level to reasoning_effort string

---

## Legacy Files in src/openai/ (TO DELETE ENTIRELY)

### Client Core (Old, Pre-Provider System)
1. `src/openai/client.c` - Legacy client implementation
2. `src/openai/client.h` - Legacy client header (defines `ik_openai_conversation_t`)
3. `src/openai/client_msg.c` - Legacy message creation
4. `src/openai/client_serialize.c` - Legacy serialization

### Multi-handle Manager (Old Async)
5. `src/openai/client_multi.c` - Legacy multi-handle core
6. `src/openai/client_multi.h` - Legacy multi-handle header
7. `src/openai/client_multi_internal.h` - Legacy multi-handle internals
8. `src/openai/client_multi_request.c` - Legacy request management
9. `src/openai/client_multi_callbacks.c` - Legacy callbacks
10. `src/openai/client_multi_callbacks.h` - Legacy callback header

### HTTP/Streaming (Old)
11. `src/openai/http_handler.c` - Legacy HTTP client
12. `src/openai/http_handler.h` - Legacy HTTP header
13. `src/openai/http_handler_internal.h` - Legacy HTTP internals
14. `src/openai/sse_parser.c` - Legacy SSE parser
15. `src/openai/sse_parser.h` - Legacy SSE parser header

### Tool Choice (Old)
16. `src/openai/tool_choice.c` - Legacy tool choice implementation
17. `src/openai/tool_choice.h` - Legacy tool choice header

### Build Artifacts
18. `src/openai/client_multi_request.o` - Build artifact
19. `src/openai/client.o` - Build artifact

---

## Legacy Files in src/providers/openai/ (SHIM LAYER - PARTIAL DELETION)

These files are part of the new provider system but contain shim code to interface with legacy conversation types. After migration, only core provider implementation should remain.

### Files to Keep (Core Provider)
- `src/providers/openai/openai.c` - Provider vtable implementation
- `src/providers/openai/openai.h` - Provider header
- `src/providers/openai/error.c` - Error mapping
- `src/providers/openai/error.h` - Error header
- `src/providers/openai/reasoning.c` - Model detection utilities
- `src/providers/openai/reasoning.h` - Reasoning utilities header

### Files to Refactor (Remove Shim Code)
- `src/providers/openai/shim.c` - **DELETE** after conversation migration
- `src/providers/openai/shim.h` - **DELETE** after conversation migration
- `src/providers/openai/request_chat.c` - Remove `ik_openai_conversation_t` usage
- `src/providers/openai/request_responses.c` - Remove `ik_openai_conversation_t` usage
- `src/providers/openai/request.h` - Remove legacy types
- `src/providers/openai/response_chat.c` - May need cleanup
- `src/providers/openai/response_responses.c` - May need cleanup
- `src/providers/openai/response.h` - May need cleanup
- `src/providers/openai/streaming_chat.c` - May need cleanup
- `src/providers/openai/streaming_responses.c` - May need cleanup
- `src/providers/openai/streaming.h` - May need cleanup

---

## Critical Blocker: agent->conversation Field

**Location:** `src/agent.h:96`

```c
// Current (LEGACY):
ik_openai_conversation_t *conversation;

// Target (PROVIDER-AGNOSTIC):
ik_message_t *messages;     // Array of normalized messages
size_t message_count;       // Number of messages
size_t message_capacity;    // Allocated capacity
```

**Why This Blocks Everything:**

The agent struct uses an OpenAI-specific type to store conversation history. Every piece of REPL code that manages conversations must use the legacy OpenAI API:
- Adding user messages
- Adding assistant responses
- Adding tool calls/results
- Forking conversations
- Clearing conversations
- Restoring conversations from database

**Migration Required:**

1. Change agent struct to use `ik_message_t[]` array (provider-agnostic)
2. Update all conversation management code to work with message arrays
3. Update database replay to populate message arrays
4. Delete legacy `ik_openai_conversation_t` and all functions operating on it

---

## External References by File

### src/agent.c
- `ik_openai_conversation_create()` - Line 182, 252
- `ik_openai_msg_create()` - Line 298
- `ik_openai_conversation_add_msg()` - Line 301

### src/repl_actions_llm.c
- `ik_openai_msg_create()` - Message creation
- `ik_openai_conversation_add_msg()` - Conversation management

### src/repl_event_handlers.c
- `ik_openai_msg_create()` - Assistant message creation
- `ik_openai_conversation_add_msg()` - Add assistant response

### src/repl_tool.c
- `ik_openai_msg_create_tool_call()` - Line ~520, ~580
- `ik_openai_msg_create_tool_result()` - Line ~540, ~600
- `ik_openai_conversation_add_msg()` - Line ~530, ~545, ~590, ~605

### src/commands_fork.c
- `ik_openai_msg_create()` - User message for fork
- `ik_openai_conversation_add_msg()` - Add fork message

### src/commands_basic.c
- `ik_openai_conversation_clear()` - Clear command

### src/repl/agent_restore.c
- `ik_openai_msg_create()` - System message creation
- `ik_openai_conversation_add_msg()` - Add system prompt

### src/repl/agent_restore_replay.c
- `ik_openai_conversation_add_msg()` - Replay messages

### src/wrapper.c
- `ik_openai_conversation_add_msg_()` - MOCKABLE wrapper

### src/wrapper_internal.h
- `ik_openai_conversation_add_msg_()` - MOCKABLE declaration

### src/providers/factory.c
- `ik_openai_create()` - Provider instantiation

---

## Migration Strategy

### Phase 1: Prepare Provider-Agnostic Message Storage
1. Add new fields to `ik_agent_ctx_t`:
   - `ik_message_t *messages`
   - `size_t message_count`
   - `size_t message_capacity`

2. Create new conversation management functions:
   - `ik_agent_add_message(agent, role, content_blocks, count)`
   - `ik_agent_clear_messages(agent)`
   - `ik_agent_clone_messages(dest, src)`

### Phase 2: Migrate External Code
3. Update each external file to use new API:
   - Replace `ik_openai_msg_create()` → `ik_message_create()`
   - Replace `ik_openai_conversation_add_msg()` → `ik_agent_add_message()`
   - Replace `ik_openai_conversation_clear()` → `ik_agent_clear_messages()`

4. Update database replay to populate message arrays

5. Update fork logic to clone message arrays

### Phase 3: Remove Legacy Code
6. Remove `conversation` field from `ik_agent_ctx_t`

7. Delete entire `src/openai/` directory (19 files)

8. Remove shim layer from `src/providers/openai/`:
   - Delete `shim.c`, `shim.h`
   - Refactor request builders to work directly with `ik_message_t[]`

9. Remove legacy function declarations from all files

10. Update Makefile to remove legacy source files

### Phase 4: Verification
11. Run `make check` - all tests pass
12. Run `make lint` - no legacy references remain
13. Run integration tests with all three providers
14. Delete failed cleanup tasks from task database

---

## Verification Commands

```bash
# Find remaining legacy references
grep -r "ik_openai_" src/ --include="*.c" --include="*.h" | grep -v "src/openai/" | grep -v "src/providers/openai/"

# Find remaining legacy includes
grep -r "#include.*openai" src/ --include="*.c" --include="*.h" | grep -v "src/openai/" | grep -v "src/providers/openai/"

# Find remaining conversation_t references
grep -r "ik_openai_conversation_t" src/ --include="*.c" --include="*.h"

# Verify cleanup
ls src/openai/  # Should not exist
grep -r "ik_openai_msg_create\|ik_openai_conversation" src/ --include="*.c" --include="*.h"  # Should be empty
```

---

## Estimated Impact

- **Files to modify:** 11 external files
- **Files to delete:** 19 legacy files + 2 shim files = 21 total
- **Functions to replace:** 6 core conversation functions
- **Tests to update:** All tests that mock conversation functions
- **Struct changes:** 1 critical field in `ik_agent_ctx_t`

**Complexity:** HIGH - Touches core agent/REPL architecture
**Risk:** MEDIUM - Well-defined migration path, provider system already exists
**Benefit:** Complete removal of OpenAI-specific coupling from core codebase
