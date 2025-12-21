# Task: OpenAI Shim Response Transformation

**Layer:** 2
**Model:** sonnet/thinking
**Depends on:** openai-shim-types.md, openai-shim-request.md

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types

**Source:**
- `src/providers/provider.h` - ik_response_t, ik_content_block_t, ik_finish_reason_t, ik_usage_t
- `src/openai/client.h` - ik_openai_response_t, ik_openai_http_response_t
- `src/openai/http_handler.h` - HTTP response structure with tool_call
- `src/tool.h` - ik_tool_call_t structure
- `src/msg.h` - ik_msg_t returned by ik_openai_chat_create()

**Plan:**
- `scratch/plan/transformation.md` - OpenAI response transformation rules
- `scratch/plan/request-response-format.md` - Internal response format

## Objective

Implement response transformation from the existing OpenAI response format (`ik_msg_t` from `ik_openai_chat_create()`) to the normalized `ik_response_t` format. This enables the shim to return responses in the unified format expected by the provider abstraction.

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_shim_transform_response(TALLOC_CTX *ctx, const ik_msg_t *msg, ik_response_t **out)` | Convert legacy response message to normalized format |
| `ik_finish_reason_t ik_openai_shim_map_finish_reason(const char *openai_reason)` | Map OpenAI finish_reason string to enum |

### Files to Update

- `src/providers/openai/shim.h` - Add function declarations
- `src/providers/openai/shim.c` - Add implementations

## Behaviors

### Response Message Transformation

The existing `ik_openai_chat_create()` returns `ik_msg_t*`:
- For text: kind="assistant", content=text, data_json=NULL
- For tool call: kind="tool_call", content=summary, data_json=structured

Transform to `ik_response_t`:
- Allocate response with single content block
- For text: type=IK_CONTENT_TEXT, text=content
- For tool_call: type=IK_CONTENT_TOOL_CALL, extract id/name/arguments from data_json

### Tool Call Extraction

Parse data_json to extract tool call fields:
```
data_json = {"id": "call_abc", "type": "function", "name": "glob", "arguments": "{...}"}
```

Map to content block:
- id = data_json.id
- name = data_json.name
- arguments = parse data_json.arguments string to yyjson_val

### Finish Reason Mapping

| OpenAI String | Normalized Enum |
|---------------|-----------------|
| "stop" | IK_FINISH_STOP |
| "length" | IK_FINISH_LENGTH |
| "tool_calls" | IK_FINISH_TOOL_USE |
| "content_filter" | IK_FINISH_CONTENT_FILTER |
| NULL or unknown | IK_FINISH_UNKNOWN |

### Usage Statistics

The existing code extracts tokens from SSE events into `ik_http_completion_t`:
- completion_tokens available
- prompt_tokens not currently extracted

For shim:
- output_tokens = completion_tokens (from completion callback)
- input_tokens = 0 (not available in current implementation)
- thinking_tokens = 0 (OpenAI doesn't expose for Chat Completions)
- total_tokens = output_tokens

Note: Full usage extraction deferred to native OpenAI provider (Layer 4).

### Memory Management

- Response allocated on provided context
- Content blocks are children of response
- Parsed JSON values use talloc allocator
- Single talloc_free on response releases everything

### Error Handling

- Return ERR_PARSE if data_json is malformed for tool_call
- Return ERR_INVALID_ARG if msg is NULL
- Unknown kind treated as text with empty content

## Test Scenarios

### Unit Tests
- Transform text response: content preserved
- Transform tool_call response: id, name, arguments extracted correctly
- Map finish_reason "stop": returns IK_FINISH_STOP
- Map finish_reason "tool_calls": returns IK_FINISH_TOOL_USE
- Map finish_reason NULL: returns IK_FINISH_UNKNOWN
- Malformed tool_call data_json: returns ERR_PARSE
- NULL message: returns ERR_INVALID_ARG

### Integration with Request Transform
- Full round-trip: build request, mock response message, transform response
- Verify tool_call_id matches between request tool_result and response

## Postconditions

- [ ] Response transformation compiles and works
- [ ] Text responses transform correctly
- [ ] Tool call responses extract all fields
- [ ] Finish reason mapping is complete
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
