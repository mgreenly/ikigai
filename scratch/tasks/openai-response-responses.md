# Task: OpenAI Responses API Response Parsing

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** openai-core.md, openai-response-chat.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/response.h` - Response header (from openai-response-chat.md)
- `src/providers/provider.h` - Response types

**Plan:**
- `scratch/plan/transformation.md` - Response transformation rules

## Objective

Implement response parsing for OpenAI Responses API. Parse `output[]` array into internal `ik_response_t`. Handle `status` field for finish reason and extract reasoning token usage.

## Key Differences: Chat Completions vs Responses API

| Field | Chat Completions | Responses API |
|-------|------------------|---------------|
| Content location | `choices[0].message` | `output[]` |
| Finish reason | `finish_reason` | `status` + `incomplete_details` |
| Text content | `message.content` | `output[].content[].text` |
| Content type | string | `output_text` type |

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_parse_responses_response(ctx, json, json_len, out_resp)` | Parse Responses API JSON to internal format, returns OK/ERR |
| `ik_openai_map_responses_status(status, incomplete_reason)` | Map status string and incomplete reason to internal finish reason |

### Helper Functions (Internal)

- `parse_usage(usage_val, out_usage)` - Extract usage tokens including reasoning_tokens
- `count_content_blocks(output)` - Count total content blocks in output array
- `parse_function_call(ctx, item, out_block)` - Parse function_call output item

## Behaviors

### Response Structure

- Parse `output[]` array for content items
- Extract `status` for completion status
- Check `incomplete_details.reason` if status is "incomplete"
- Extract `usage` for token counts
- Extract `model` for response model name

### Status Mapping

- "completed" → IK_FINISH_STOP
- "failed" → IK_FINISH_ERROR
- "cancelled" → IK_FINISH_STOP
- "incomplete" + "max_output_tokens" → IK_FINISH_LENGTH
- "incomplete" + "content_filter" → IK_FINISH_CONTENT_FILTER
- "incomplete" + other/NULL → IK_FINISH_LENGTH
- NULL or unknown → IK_FINISH_UNKNOWN

### Output Array Processing

- Iterate through output[] array
- Process items by type field
- "message" type contains content[] array
- "function_call" type is a tool call

### Message Content Extraction

- Each message has content[] array
- Extract items with type "output_text"
- Extract text field from output_text items
- Also handle "refusal" type content

### Function Call Parsing

- Extract id and call_id (prefer call_id)
- Extract name field
- Extract arguments as JSON string
- Parse arguments string to yyjson_val
- Store in tool call content block

### Usage Extraction

- Same format as Chat Completions
- `prompt_tokens` → input_tokens
- `completion_tokens` → output_tokens
- `total_tokens` → total_tokens
- Extract `reasoning_tokens` from `completion_tokens_details.reasoning_tokens`

### Error Handling

- Check for `error` field in root JSON
- Return ERR(PROVIDER) if error present
- Return ERR(PARSE) if JSON is malformed
- Handle missing output array gracefully

## Test Scenarios

### Simple Response

- JSON with output message
- Verify model extracted
- Verify status = "completed" → FINISH_STOP
- Verify content has text blocks
- Verify usage tokens extracted

### Function Call Response

- JSON with function_call output item
- Verify content has tool call block
- Verify call_id, name extracted
- Verify arguments parsed from JSON string

### Reasoning Token Usage

- Response with completion_tokens_details.reasoning_tokens
- Verify thinking_tokens field populated correctly

### Status Mapping

- Test "completed" → STOP
- Test "failed" → ERROR
- Test "incomplete" + "max_output_tokens" → LENGTH
- Test "incomplete" + "content_filter" → CONTENT_FILTER

### Multiple Content Items

- Output with multiple message items
- Verify all text blocks extracted
- Verify content_count correct

### Refusal Content

- Message with refusal type content
- Verify refusal text extracted to content block

### Empty Output

- Response with empty output array
- Verify content_count = 0
- Verify graceful handling

## Postconditions

- [ ] `src/providers/openai/response.h` declares Responses parsing function
- [ ] `src/providers/openai/response_responses.c` parses Responses API format
- [ ] Output array correctly parsed into content blocks
- [ ] output_text content type extracted as text
- [ ] function_call items parsed as tool calls
- [ ] Status mapped to finish reason
- [ ] incomplete_details.reason used for incomplete status
- [ ] Reasoning tokens extracted from completion_tokens_details
- [ ] Makefile updated with response_responses.c
- [ ] All tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
