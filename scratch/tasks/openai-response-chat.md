# Task: OpenAI Chat Completions Response Parsing

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** openai-core.md, openai-request-chat.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/provider.h` - Response types
- `src/providers/openai/request.h` - Request serialization
- `src/providers/anthropic/response.c` - Example response parsing pattern

**Plan:**
- `scratch/plan/transformation.md` - Response transformation rules
- `scratch/plan/error-handling.md` - Error mapping

## Objective

Implement response parsing for OpenAI Chat Completions API. Parse `choices[0].message` JSON format into internal `ik_response_t` structure with proper tool call argument parsing.

## Key Differences from Anthropic

| Field | Anthropic | OpenAI Chat |
|-------|-----------|-------------|
| Content location | `content[]` | `choices[0].message` |
| Finish reason | `stop_reason` | `finish_reason` |
| Tool call args | Parsed object | **JSON string** (must parse) |
| Usage | `usage` | `usage` |

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_parse_chat_response(ctx, json, json_len, out_resp)` | Parse Chat Completions JSON response to internal format, returns OK/ERR |
| `ik_openai_parse_error(ctx, http_status, json, json_len, out_category, out_message)` | Parse error response and map HTTP status to error category |
| `ik_openai_map_chat_finish_reason(finish_reason)` | Map OpenAI finish_reason string to internal finish reason enum |

### Helper Functions (Internal)

- `parse_chat_tool_call(ctx, tc_val, out_block)` - Parse tool call from JSON, parse arguments string
- `parse_chat_usage(usage_val, out_usage)` - Extract usage tokens including reasoning_tokens

## Behaviors

### Response Structure

- Parse `choices[0].message` for content
- Extract `choices[0].finish_reason` for completion status
- Extract `usage` for token counts
- Extract `model` for response model name

### Finish Reason Mapping

- "stop" → IK_FINISH_STOP
- "length" → IK_FINISH_LENGTH
- "tool_calls" → IK_FINISH_TOOL_USE
- "content_filter" → IK_FINISH_CONTENT_FILTER
- "error" → IK_FINISH_ERROR
- NULL or unknown → IK_FINISH_UNKNOWN

### Content Extraction

- Text content from `message.content` field
- Tool calls from `message.tool_calls` array
- Create content blocks array with both text and tool calls
- If message.content is null but tool_calls exist, content field should be null

### Tool Call Parsing

- Each tool call has: id, type, function.name, function.arguments
- Arguments is JSON string that must be parsed to yyjson_val
- Use `yyjson_read()` to parse arguments string
- Store parsed arguments in content_block.data.tool_call.arguments
- Store doc reference in content_block.data.tool_call._args_doc for cleanup

### Usage Extraction

- `prompt_tokens` → input_tokens
- `completion_tokens` → output_tokens
- `total_tokens` → total_tokens
- Extract `reasoning_tokens` from `completion_tokens_details` if present

### Error Handling

- Check for `error` field in root JSON
- Return ERR(PROVIDER) if error present
- Return ERR(PARSE) if JSON is malformed
- HTTP status mapping:
  - 400 → IK_ERR_CAT_INVALID_ARG
  - 401, 403 → IK_ERR_CAT_AUTH
  - 404 → IK_ERR_CAT_NOT_FOUND
  - 429 → IK_ERR_CAT_RATE_LIMIT
  - 500, 502, 503 → IK_ERR_CAT_SERVER
  - Other → IK_ERR_CAT_UNKNOWN

### Error Message Format

- Extract from `error.message` field
- Include error.type and error.code if present
- Format: "{type} ({code}): {message}" or "{type}: {message}"
- Fallback to "HTTP {status}" if no JSON

## Test Scenarios

### Simple Response

- JSON with single text message
- Verify model extracted
- Verify finish_reason = STOP
- Verify content has 1 text block
- Verify usage tokens extracted

### Tool Call Response

- JSON with tool_calls array
- Verify finish_reason = TOOL_USE
- Verify content has tool call blocks
- Verify tool call id, name extracted
- Verify arguments parsed from JSON string

### Usage with Reasoning Tokens

- Response with completion_tokens_details.reasoning_tokens
- Verify thinking_tokens extracted correctly

### Finish Reason Mapping

- Test all finish reason strings map correctly
- Test NULL returns UNKNOWN

### Error Response Parsing

- Test authentication error (401)
- Test rate limit error (429)
- Test server error (500)
- Verify category mapping
- Verify message extraction

### Malformed JSON

- Invalid JSON string
- Verify returns ERR(PARSE)

### Empty Response

- Response with no choices
- Verify content_count = 0
- Verify finish_reason = UNKNOWN

## Postconditions

- [ ] `src/providers/openai/response.h` declares parsing functions
- [ ] `src/providers/openai/response_chat.c` implements Chat parsing
- [ ] Tool call arguments parsed from JSON strings
- [ ] Usage correctly extracts all token fields
- [ ] Finish reason mapping complete
- [ ] Error parsing maps HTTP status and extracts messages
- [ ] Makefile updated with response_chat.c
- [ ] All response parsing tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
