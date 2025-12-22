# Task: Anthropic Request Serialization

**Model:** sonnet/thinking
**Depends on:** anthropic-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - Talloc-based memory management

**Source:**
- `src/providers/anthropic/anthropic.c` - Provider context
- `src/providers/anthropic/thinking.h` - Budget calculation
- `src/providers/provider.h` - Request types
- `src/json_allocator.h` - Talloc-integrated JSON allocators

**Plan:**
- `scratch/plan/configuration.md` - Anthropic transformation rules and request format

## Objective

Implement request serialization for Anthropic API. Transform internal `ik_request_t` to Anthropic JSON wire format, including system prompts, thinking configuration, tools, and content blocks.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_anthropic_serialize_request(TALLOC_CTX *ctx, ik_request_t *req, char **out_json)` | Serializes internal request to Anthropic JSON, returns OK with JSON string or ERR on failure |
| `res_t ik_anthropic_build_headers(TALLOC_CTX *ctx, const char *api_key, char ***out_headers)` | Builds HTTP headers array for Anthropic API, returns NULL-terminated array |

Structs to define: None (uses existing provider types)

## Behaviors

### Request Serialization
- When `ik_anthropic_serialize_request()` is called, create JSON document with yyjson
- Add required fields: model, max_tokens
- If max_output_tokens is 0, default to 4096
- Return ERR(INVALID_ARG) if model is NULL

### System Prompt Handling
- Concatenate all system_prompt content blocks with "\n\n" separator
- Add as single "system" field (separate from messages array)
- Skip if system_prompt_count is 0

### Message Serialization
- When message has single text block, use string format: `"content": "text"`
- When message has multiple blocks or non-text blocks, use array format
- Serialize each content block based on type:
  - IK_CONTENT_TEXT → `{"type": "text", "text": "..."}`
  - IK_CONTENT_THINKING → `{"type": "thinking", "thinking": "..."}`
  - IK_CONTENT_TOOL_CALL → `{"type": "tool_use", "id": "...", "name": "...", "input": {...}}`
  - IK_CONTENT_TOOL_RESULT → `{"type": "tool_result", "tool_use_id": "...", "content": "...", "is_error": true/false}`

### Thinking Configuration
- When thinking.level is not IK_THINKING_NONE, calculate budget using `ik_anthropic_thinking_budget()`
- Add thinking config: `{"type": "enabled", "budget_tokens": <budget>}`
- Skip if budget is -1 (model doesn't support thinking)

### Tool Serialization
- When tool_count > 0, serialize each tool definition
- Map tool.parameters to "input_schema" (Anthropic naming)
- Add tool_choice mapping:
  - IK_TOOL_CHOICE_NONE → `{"type": "none"}`
  - IK_TOOL_CHOICE_AUTO → `{"type": "auto"}`
  - IK_TOOL_CHOICE_REQUIRED → `{"type": "any"}`

### Header Building
- Return array of 4 strings (3 headers + NULL terminator):
  - `"x-api-key: <api_key>"`
  - `"anthropic-version: 2023-06-01"`
  - `"content-type: application/json"`
  - NULL

### Memory Management
- Allocate JSON string with talloc
- Free yyjson document before returning
- All error paths free allocated resources

## Test Scenarios

### Basic Serialization
- Simple request with model and single text message produces valid JSON
- JSON contains "model" and "max_tokens" fields
- Message serialized as simple string when single text block

### System Prompt
- Request with system_prompt produces "system" field
- Multiple system blocks concatenated with "\n\n"
- Empty system_prompt_count skips "system" field

### Thinking Configuration
- Request with IK_THINKING_MED adds thinking config
- Budget calculated correctly for model (e.g., 43008 for Sonnet 4.5 MED)
- IK_THINKING_NONE skips thinking config
- Unsupported model skips thinking config

### Tool Serialization
- Request with tools produces "tools" array
- Tool parameters mapped to "input_schema"
- Tool choice correctly mapped (NONE→none, AUTO→auto, REQUIRED→any)
- No tools skips "tools" and "tool_choice" fields

### Complex Content
- Message with multiple content blocks uses array format
- Tool call blocks include id, name, and input
- Tool result blocks include tool_use_id, content, and is_error flag

### Headers
- Build headers returns 3 headers + NULL
- Headers include correct API key
- Anthropic version is "2023-06-01"

### Error Cases
- NULL model returns ERR(INVALID_ARG)
- Serialization failure returns ERR(PARSE)

## Postconditions

- [ ] `src/providers/anthropic/request.h` exists
- [ ] `src/providers/anthropic/request.c` implements serialization
- [ ] `ik_anthropic_serialize_request()` produces valid Anthropic JSON
- [ ] System prompt serialized as separate "system" field
- [ ] Thinking config includes "budget_tokens" from level calculation
- [ ] Tools serialized with "input_schema" (not "parameters")
- [ ] Tool choice correctly mapped for all enum values
- [ ] Headers include x-api-key, anthropic-version, content-type
- [ ] Compiles without warnings
- [ ] All serialization tests pass
- [ ] Changes committed to git with message: `task: anthropic-request.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).