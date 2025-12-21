# Task: OpenAI Shim Request Transformation

**Layer:** 2
**Model:** sonnet/thinking
**Depends on:** openai-shim-types.md, provider-types.md

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types
- `/load database` - Message types (ik_msg_t)

**Source:**
- `src/providers/provider.h` - ik_request_t, ik_message_t, ik_content_block_t
- `src/openai/client.h` - ik_openai_request_t, ik_openai_conversation_t
- `src/openai/client.c` - ik_openai_serialize_request() to understand expected format
- `src/msg.h` - ik_msg_t canonical message type
- `src/openai/client_msg.c` - Message creation helpers

**Plan:**
- `scratch/plan/transformation.md` - OpenAI transformation rules
- `scratch/plan/request-response-format.md` - Internal format spec

## Objective

Implement request transformation from the normalized `ik_request_t` format to the existing `ik_openai_request_t` and `ik_openai_conversation_t` format. This enables the shim to prepare requests for the existing OpenAI client code without modifying it.

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_transform_request(TALLOC_CTX *ctx, const ik_request_t *req, ik_openai_request_t **out)` | Convert normalized request to OpenAI format |
| `res_t ik_openai_shim_build_conversation(TALLOC_CTX *ctx, const ik_request_t *req, ik_openai_conversation_t **out)` | Build conversation from normalized messages |
| `res_t ik_openai_shim_transform_message(TALLOC_CTX *ctx, const ik_message_t *msg, ik_msg_t **out)` | Convert single normalized message to legacy format |

### Files to Update

- `src/providers/openai/shim.h` - Add function declarations
- `src/providers/openai/shim.c` - Add implementations

## Behaviors

### Message Transformation Rules

| Normalized (ik_message_t) | Legacy (ik_msg_t) |
|---------------------------|-------------------|
| role=IK_ROLE_USER | kind="user" |
| role=IK_ROLE_ASSISTANT | kind="assistant" |
| content[0].type=TEXT | content=text, data_json=NULL |
| content[0].type=TOOL_CALL | kind="tool_call", data_json=structured |
| content[0].type=TOOL_RESULT | kind="tool_result", data_json=structured |

### System Prompt Handling
- Normalized: `req->system_prompt` is array of content blocks
- Legacy: First message with kind="system" and content=concatenated text
- Transform: Concatenate all system prompt text blocks, create system message

### Tool Call Transformation
- Normalized: `ik_content_block_t` with id, name, arguments (parsed JSON)
- Legacy: `ik_msg_t` with kind="tool_call", data_json containing {id, type, name, arguments}
- Use existing `ik_openai_msg_create_tool_call()` helper

### Tool Result Transformation
- Normalized: `ik_content_block_t` with tool_call_id, content, is_error
- Legacy: `ik_msg_t` with kind="tool_result", data_json containing {tool_call_id, name, output, success}
- Use existing `ik_msg_create_tool_result()` helper

### Request Assembly
1. Create `ik_openai_conversation_t`
2. Add system message if system_prompt present
3. Transform each message, add to conversation
4. Create `ik_openai_request_t` with model, temperature (default 0.7), max_tokens
5. Thinking level maps to model selection (no direct parameter in existing code)

### Error Handling
- Return ERR_INVALID_ARG if request has no messages
- Return ERR_INVALID_ARG if content block type is unsupported
- Propagate errors from message creation helpers

### Memory Management
- All output allocated on provided talloc context
- Intermediate allocations on temporary context, freed on error

## Test Scenarios

### Unit Tests
- Transform simple user message: text preserved
- Transform assistant message: text preserved
- Transform tool_call message: id, name, arguments preserved in data_json
- Transform tool_result message: tool_call_id, content preserved
- Transform system prompt: concatenated as first message
- Transform multi-message conversation: order preserved
- Empty messages array: returns ERR_INVALID_ARG
- Unknown content type: returns ERR_INVALID_ARG

### Round-Trip Verification
- Build ik_request_t with known content
- Transform to ik_openai_request_t
- Serialize with ik_openai_serialize_request()
- Parse JSON and verify structure matches expected

## Postconditions

- [ ] Request transformation compiles and works
- [ ] All message types transform correctly
- [ ] System prompt handled correctly
- [ ] Tool calls/results preserve all fields
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
