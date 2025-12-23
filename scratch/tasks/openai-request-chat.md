# Task: OpenAI Chat Completions Request Serialization

**Model:** sonnet/thinking
**Depends on:** openai-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/openai.c` - Provider context
- `src/providers/openai/reasoning.h` - Reasoning effort mapping
- `src/providers/provider.h` - Request types
- `src/providers/anthropic/request.c` - Example request serialization pattern

**Plan:**
- `scratch/plan/transformation.md` - OpenAI transformation rules

## Objective

Implement request serialization for OpenAI Chat Completions API. Transform internal `ik_request_t` to Chat Completions JSON wire format with proper system message handling and tool call argument serialization.

## Key Differences from Anthropic

| Field | Anthropic | OpenAI Chat |
|-------|-----------|-------------|
| System | `system` field | First message role=system |
| Messages | `messages[]` | `messages[]` |
| Role | user/assistant | user/assistant/system/tool |
| Tool args | Parsed object | **JSON string** |
| Thinking | `thinking.budget_tokens` | N/A (use Responses API) |

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_serialize_chat_request(ctx, req, streaming, out_json)` | Serialize internal request to Chat Completions JSON, returns OK/ERR |
| `ik_openai_build_chat_url(ctx, base_url, out_url)` | Build URL for Chat Completions endpoint (/v1/chat/completions) |
| `ik_openai_build_headers(ctx, api_key, out_headers)` | Build HTTP headers array (Authorization: Bearer, Content-Type) |

### Helper Functions (Internal)

- `get_openai_role(role)` - Map `ik_role_t` to OpenAI role string
- `serialize_tool_call(doc, block)` - Serialize tool call content block to JSON
- `serialize_chat_message(doc, msg)` - Serialize message to Chat Completions format
- `serialize_chat_tool(doc, tool)` - Serialize tool definition with "strict": true
- `concat_system_blocks(ctx, blocks, count)` - Concatenate system prompt blocks with "\n\n"
- `add_tool_choice(doc, root, choice)` - Add tool_choice field (none/auto/required)

## Behaviors

### System Prompt Handling

- Concatenate all system prompt content blocks with "\n\n" separator
- Create first message with role "system" and concatenated content
- If no system prompt, don't add system message

### Message Serialization

- Regular messages: role + concatenated text content
- Assistant with tool calls: role "assistant", content null, tool_calls array
- Tool result messages: role "tool", tool_call_id, content
- Concatenate multiple text blocks within a message

### Tool Call Serialization

- Tool calls must be in tool_calls array under assistant message
- Each tool call has: id, type: "function", function: {name, arguments}
- Arguments must be serialized as JSON string (not object)
- Use `yyjson_val_write()` to convert arguments object to string
- **Note:** OpenAI API specification requires tool call arguments as JSON strings (not objects). This is intentional and differs from Anthropic and Google APIs which accept arguments as JSON objects. See OpenAI Chat Completions API documentation.

### Tool Definition Format

- Top-level type: "function"
- Nested function object: name, description, parameters
- Add "strict": true for structured outputs
- Parameters is JSON schema object

### Request Fields

- `model` - required, from req->model
- `messages` - array with system (optional) + conversation messages
- `max_completion_tokens` - from req->max_output_tokens (if > 0)
- `stream` - true if streaming, with stream_options.include_usage = true
- `tools` - array of tool definitions (if tool_count > 0)
- `tool_choice` - "none"/"auto"/"required" (if tools present)

### Headers

- Authorization: Bearer {api_key}
- Content-Type: application/json
- NULL-terminated array

### Error Handling

- Return ERR(INVALID_ARG) if model is NULL
- Return ERR(PARSE) if JSON serialization fails
- PANIC on out-of-memory

## Test Scenarios

### Simple Request

- Single user message with text content
- Verify model field present
- Verify messages array has 1 element
- Verify max_completion_tokens included if set

### Request with System Prompt

- System prompt blocks + user message
- Verify first message has role "system"
- Verify system content concatenated correctly
- Verify user message follows system message

### Streaming Request

- streaming=true flag set
- Verify "stream": true in JSON
- Verify "stream_options": {"include_usage": true}

### Tool Definitions

- Request with tools array
- Verify tools serialized with type: "function"
- Verify function.name, function.description present
- Verify "strict": true added to each tool
- Verify tool_choice field matches request

### Tool Call Messages

- Assistant message with tool calls
- Verify content is null
- Verify tool_calls array present
- Verify each tool call has id, type, function.name
- Verify arguments is JSON string

### Tool Result Messages

- Message with role "tool"
- Verify tool_call_id present
- Verify content from tool result

### URL Building

- Build URL from base_url
- Verify result is "{base_url}/v1/chat/completions"

### Header Building

- Build headers with API key
- Verify Authorization header format
- Verify Content-Type header
- Verify NULL terminator

## Postconditions

- [ ] `src/providers/openai/request.h` declares serialization functions
- [ ] `src/providers/openai/request_chat.c` implements Chat Completions serialization
- [ ] System prompt becomes first message with role "system"
- [ ] Tool call arguments serialized as JSON strings (not objects)
- [ ] Streaming adds stream: true and stream_options
- [ ] Headers include Authorization and Content-Type
- [ ] Makefile updated with request_chat.c
- [ ] All serialization tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: openai-request-chat.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).