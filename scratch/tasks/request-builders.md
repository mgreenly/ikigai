# Task: Create Request/Response Builders

**Layer:** 1
**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns
- `/load errors` - Result types

**Source:**
- `src/openai/client_msg.c` - Reference for message building patterns
- `src/tool.h` - Tool definition structures

**Plan:**
- `scratch/plan/request-response-format.md` - Builder pattern specification

## Objective

Create builder functions for constructing `ik_request_t` and working with `ik_response_t`. These functions provide a convenient API for assembling requests with system prompts, messages, content blocks, tools, and thinking configuration.

## Interface

### Request Builder Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_request_create` | `res_t (TALLOC_CTX *ctx, const char *model, ik_request_t **out)` | Create empty request with model |
| `ik_request_set_system` | `res_t (ik_request_t *req, const char *text)` | Set system prompt as single text block |
| `ik_request_add_message` | `res_t (ik_request_t *req, ik_role_t role, const char *text)` | Add simple text message |
| `ik_request_add_message_blocks` | `res_t (ik_request_t *req, ik_role_t role, ik_content_block_t *blocks, size_t count)` | Add message with content blocks |
| `ik_request_set_thinking` | `void (ik_request_t *req, ik_thinking_level_t level, bool include_summary)` | Configure thinking level |
| `ik_request_add_tool` | `res_t (ik_request_t *req, const char *name, const char *description, yyjson_val *parameters, bool strict)` | Add tool definition |

### Response Helper Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_response_create` | `res_t (TALLOC_CTX *ctx, ik_response_t **out)` | Create empty response |
| `ik_response_add_content` | `res_t (ik_response_t *resp, ik_content_block_t *block)` | Add content block to response |

### Content Block Builders

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_content_block_text` | `ik_content_block_t *(TALLOC_CTX *ctx, const char *text)` | Create text content block |
| `ik_content_block_tool_call` | `ik_content_block_t *(TALLOC_CTX *ctx, const char *id, const char *name, yyjson_val *arguments)` | Create tool call block |
| `ik_content_block_tool_result` | `ik_content_block_t *(TALLOC_CTX *ctx, const char *tool_call_id, const char *content, bool is_error)` | Create tool result block |
| `ik_content_block_thinking` | `ik_content_block_t *(TALLOC_CTX *ctx, const char *text)` | Create thinking block |

## Behaviors

### Request Creation

- Allocate request structure on provided context
- Set model name (talloc_strdup)
- Initialize arrays (messages, tools, system_prompt) to NULL
- Set max_output_tokens to -1 (use provider default)
- Set thinking level to NONE
- Return OK with request, ERR on allocation failure

### System Prompt Setting

- Create single text content block from string
- Store in system_prompt array with count=1
- Previous system prompt replaced (old memory freed)

### Adding Text Messages

- Create message with single text content block
- Append to messages array
- Grow array using talloc_realloc
- Return OK, ERR on allocation failure

### Adding Block Messages

- Create message with provided content blocks
- Copy block pointers (blocks owned by caller or request)
- Append to messages array
- Return OK, ERR on allocation failure

### Thinking Configuration

- Set thinking level and include_summary flag
- No error return (void function)
- Settings apply to entire request

### Adding Tools

- Create tool definition structure
- Copy name, description (talloc_strdup)
- Store parameters JSON (borrowed or cloned)
- Append to tools array
- Return OK, ERR on allocation failure

### Response Creation

- Allocate empty response structure
- Initialize content array to NULL
- Set finish_reason to STOP
- Zero usage counters
- Return OK, ERR on allocation failure

### Adding Content to Response

- Append content block to content array
- Grow array using talloc_realloc
- Block owned by response after adding
- Return OK, ERR on allocation failure

### Content Block Builders

- Allocate content block structure
- Set type enum
- Fill appropriate union member
- Copy strings via talloc_strdup
- Return block or panic on OOM

### Memory Management

- All allocations use talloc on provided context
- Request owns all messages, tools, system_prompt
- Response owns all content blocks
- Strings duplicated when stored
- Arrays grown with talloc_realloc

## Directory Structure

```
src/providers/
├── request.h
├── request.c
├── response.h
└── response.c

tests/unit/providers/
├── request_test.c
└── response_test.c
```

## Test Scenarios

### Request Builder Tests (`request_test.c`)

- Create request: Successfully create empty request with model
- Set system prompt: System prompt set and replaceable
- Add text message: Simple message added
- Add multiple messages: Array grows correctly
- Add message with blocks: Multi-block messages supported
- Set thinking: Thinking config updated
- Add tools: Tools array grows correctly
- Memory lifecycle: Freeing request frees all child allocations

### Response Builder Tests (`response_test.c`)

- Create response: Successfully create empty response
- Add content: Content blocks appended
- Multiple content: Array grows correctly
- Memory lifecycle: Freeing response frees content

### Content Block Tests

- Text block: Contains text string
- Tool call block: Contains id, name, arguments
- Tool result block: Contains tool_call_id, content, error flag
- Thinking block: Contains thinking text

## Postconditions

- [ ] `src/providers/request.h` and `request.c` exist
- [ ] `src/providers/response.h` and `response.c` exist
- [ ] Makefile updated with new sources/headers
- [ ] Can construct requests with all content types
- [ ] Can construct responses with all content types
- [ ] Builders use talloc correctly (no leaks)
- [ ] Arrays grow dynamically
- [ ] Compiles without warnings
- [ ] Unit tests pass
- [ ] `make check` passes
