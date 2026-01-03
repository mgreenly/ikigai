# Task: OpenAI Responses API Request Serialization

**Model:** sonnet/thinking
**Depends on:** openai-core.md, openai-request-chat.md

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
- `src/providers/openai/request.h` - Request header (from openai-request-chat.md)
- `src/providers/openai/reasoning.h` - Reasoning effort mapping
- `src/providers/provider.h` - Request types

**Plan:**
- `scratch/plan/transformation.md` - OpenAI transformation rules

## Objective

Implement request serialization for OpenAI Responses API. This API is used for reasoning models (o1, o3) and supports `instructions` field and `reasoning.effort` parameter.

## Key Differences: Chat Completions vs Responses API

| Field | Chat Completions | Responses API |
|-------|------------------|---------------|
| System | First message role=system | `instructions` field |
| Messages | `messages[]` | `input` (string or array) |
| Max tokens | `max_completion_tokens` | `max_output_tokens` |
| Thinking | N/A | `reasoning.effort` |
| Tools | `tools[].function` | `tools[].function` (same format) |

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_serialize_responses_request(ctx, req, streaming, out_json)` | Serialize internal request to Responses API JSON, returns OK/ERR |
| `ik_openai_build_responses_url(ctx, base_url, out_url)` | Build URL for Responses API endpoint (/v1/responses) |

### Helper Functions (Internal)

- `get_openai_role(role)` - Map `ik_role_t` to OpenAI role string
- `serialize_input_message(doc, msg)` - Serialize message for input array
- `concat_system_blocks(ctx, blocks, count)` - Build instructions from system prompt
- `add_tool_choice(doc, root, choice)` - Add tool_choice field

## Behaviors

### System Prompt Handling

- Concatenate all system prompt blocks with "\n\n" separator
- Add as top-level `instructions` field
- If no system prompt, omit instructions field

### Input Field Format

- Single user message with simple text → use string input
- Multi-turn conversation → use array format
- Each array element has role and content
- Tool calls use same format as Chat Completions

### Reasoning Configuration

- Check if model is reasoning model via `ik_openai_is_reasoning_model()`
- Only add reasoning config for reasoning models
- Map thinking level to effort string via `ik_openai_reasoning_effort()`
- Add as `reasoning: {effort: "low"/"medium"/"high"}`
- Omit reasoning field if level is NONE or model doesn't support it

### Tool Definition Format

- Tool definitions in Responses API use identical format to Chat Completions API
- Each tool has nested structure: `{type: "function", function: {name, description, parameters}}`
- The `function` object contains: name, description, parameters
- Add "strict": true for structured outputs
- Same nested function object as Chat Completions

### Request Fields

- `model` - required
- `instructions` - system prompt (optional)
- `input` - string or array of messages
- `max_output_tokens` - from req->max_output_tokens (if > 0)
- `stream` - true if streaming
- `reasoning` - {effort: string} for reasoning models only
- `tools` - array of tool definitions (if tool_count > 0)
- `tool_choice` - "none"/"auto"/"required" (if tools present)

### Error Handling

- Return ERR(INVALID_ARG) if model is NULL
- Return ERR(PARSE) if JSON serialization fails
- PANIC on out-of-memory

## Test Scenarios

### Simple String Input

- Single user message
- Verify input is string (not array)
- Verify reasoning.effort present for o1/o3 models
- Verify max_output_tokens field

### Request with Instructions

- System prompt blocks + user message
- Verify instructions field contains concatenated system prompt
- Verify input contains user message

### Multi-turn Array Input

- Multiple messages in conversation
- Verify input is array (not string)
- Verify array contains all messages with roles

### Reasoning Configuration

- Request with thinking level for o1 model
- Verify reasoning.effort = "low"/"medium"/"high"
- Verify reasoning field omitted for non-reasoning models
- Verify reasoning field omitted if level is NONE

### Tool Format

- Request with tools
- Verify tools use nested format with `type: "function"` and `function` object
- Verify function object contains: name, description, parameters
- Verify "strict": true added

### URL Building

- Build URL from base_url
- Verify result is "{base_url}/v1/responses"

## Postconditions

- [ ] `src/providers/openai/request.h` declares Responses API functions
- [ ] `src/providers/openai/request_responses.c` implements Responses serialization
- [ ] System prompt becomes `instructions` field
- [ ] Single user message uses string input, multi-turn uses array
- [ ] Reasoning effort included for reasoning models only
- [ ] Tool definitions use same nested format as Chat Completions (type + function object)
- [ ] Makefile updated with request_responses.c
- [ ] All serialization tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: openai-request-responses.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).