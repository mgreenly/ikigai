# Fix: ik_openai_chat_create Return Type and Canonical Message Conversion

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `coverage.md` - 100% coverage requirement
- `style.md` - Code style conventions
- `naming.md` - Naming conventions

## Files to Explore

### Source files:
- `src/openai/client.h` - API definitions
- `src/openai/client.c` - Implementation of `ik_openai_chat_create`
- `src/openai/client_msg.c` - Canonical message creation functions
- `src/openai/http_handler.h` - HTTP response structure
- `src/openai/http_handler.c` - HTTP/SSE handling
- `src/tool.h` - Tool call structure definition

### Test files:
- `tests/unit/openai/http_handler_tool_calls_test.c` - Currently failing test
- `tests/unit/openai/client_tool_call_test.c` - Check if this needs updates

## Situation

### Architecture Overview

The codebase has a **canonical message format** that serves as:
1. The database record format (superset of all message types)
2. The in-memory representation
3. The rendering source

The flow is:
- User input → Database (canonical) → Render
- Model output → Database (canonical) → Render

### Three Record Types

1. **OpenAI API format** - JSON over the wire (OpenAI-specific)
2. **`ik_openai_http_response_t`** - Intermediate layer after parsing HTTP/SSE
   - Has `content` (text response)
   - Has `finish_reason`
   - Has `tool_call` (ik_tool_call_t with id, name, arguments)
3. **`ik_openai_msg_t`** - **CANONICAL FORMAT** (poorly named, represents internal canonical format, not OpenAI-specific)
   - Polymorphic with `role` as discriminator: "user", "assistant", "system", "tool_call"
   - `content` - text or human-readable summary
   - `data_json` - structured data (only for role="tool_call")

### Current Bug

`ik_openai_chat_create` currently:
1. Calls `ik_openai_http_post` → gets `ik_openai_http_response_t *`
2. Creates `ik_openai_response_t *` (wrong type!)
3. Copies `content` and `finish_reason` but **IGNORES `tool_call`**
4. Returns `ik_openai_response_t *` instead of canonical `ik_openai_msg_t *`

The test `tests/unit/openai/http_handler_tool_calls_test.c` fails because:
- It calls `ik_openai_chat_create` expecting to get back an `ik_openai_msg_t *`
- It tries to access `msg->tool_call` and `msg->finish_reason` which don't exist on the struct
- The returned type is wrong and the tool_call data is lost

### Current Signature (WRONG)

```c
res_t ik_openai_chat_create(...)
// Returns: OK(ik_openai_response_t*) - WRONG!
```

## High-Level Goal

**Fix `ik_openai_chat_create` to properly convert OpenAI responses to canonical format.**

### Required Changes

1. **Change return type**: `ik_openai_chat_create` should return `res_t` with `ik_openai_msg_t *` in ok field

2. **Implement conversion logic**:
   - If `http_resp->tool_call` is present:
     - Extract id, type="function", name, arguments from `ik_tool_call_t`
     - Generate human-readable summary for content (e.g., "glob(pattern=\"*.c\")")
     - Call `ik_openai_msg_create_tool_call(parent, id, type, name, arguments, content)`
     - Return canonical message with role="tool_call"

   - If `http_resp->content` is present (regular text response):
     - Call `ik_openai_msg_create(parent, "assistant", content)`
     - Return canonical message with role="assistant"

3. **Update tests**: Fix `tests/unit/openai/http_handler_tool_calls_test.c` to work with canonical format:
   - Access tool call data via `msg->data_json` (parse JSON to verify id, name, arguments)
   - Remove references to `msg->tool_call` and `msg->finish_reason` (don't exist on canonical struct)

4. **Update ALL callers** of `ik_openai_chat_create`:
   - Search codebase for all uses
   - Update to handle `ik_openai_msg_t *` return type instead of `ik_openai_response_t *`

5. **Consider deprecation**:
   - `ik_openai_response_t` may now be unused/obsolete
   - If nothing uses it, remove from `src/openai/client.h`

## Success Criteria

- `make check` passes (100% tests)
- `make coverage` shows 100% coverage
- Tool calls are properly converted to canonical format
- All message types flow through the same canonical representation
