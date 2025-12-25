# Task: Google Thought Signature Handling

**Model:** sonnet/thinking
**Depends on:** google-core.md, google-request.md, google-response.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/google/response.h` - Response parsing
- `src/providers/google/request.h` - Request serialization
- `src/providers/provider.h` - Response and request types

**Plan:**
- `scratch/plan/03-provider-types.md` - Thought signature storage and resubmission logic
- `scratch/plan/02-data-formats/request-response.md` - Provider data field usage

## Objective

Implement thought signature extraction and resubmission for Google Gemini 3 models. Thought signatures are encrypted tokens returned by Gemini 3 when using extended thinking with function calling. These signatures must be captured from responses and resubmitted in subsequent requests to maintain conversation context.

## Key Concepts

**What are thought signatures?**
- Encrypted verification tokens returned by Gemini 3 in responses with thinking content
- Required for function calling with extended thinking enabled
- Provider-specific feature unique to Google Gemini 3 models
- Opaque strings that must be preserved and resubmitted verbatim

**When are they used?**
- Only for Gemini 3 models (gemini-3-pro, gemini-3-flash, etc.)
- Only when extended thinking is enabled (thinkingLevel present)
- Only when function calling is involved in the conversation
- Not used for Gemini 2.5 models or non-thinking responses

## Interface

Functions to implement or modify:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_extract_thought_signature(TALLOC_CTX *ctx, yyjson_val *response_json, char **out_signature)` | Extract thought signature from response JSON if present, returns OK with NULL if not found |
| `res_t ik_google_add_thought_signature_part(yyjson_mut_doc *doc, yyjson_mut_val *parts_array, const char *signature)` | Add thoughtSignature part to request parts array |
| `res_t ik_google_store_thought_signature(TALLOC_CTX *ctx, ik_response_t *response, const char *signature)` | Store signature in response->provider_data JSON field |
| `res_t ik_google_get_thought_signature_from_message(ik_message_t *msg, char **out_signature)` | Extract thought signature from message provider_data if present |

## Behaviors

**Signature Extraction (Response Parsing):**

When parsing a Gemini 3 response:
1. Check if response JSON contains a `thoughtSignature` field
2. If present, extract the string value
3. Store in response->provider_data as JSON object: `{"thought_signature": "value"}`
4. If not present, leave provider_data as NULL
5. Signature location in response JSON varies by API version - check candidates[].content or root level

**Signature Storage Format:**

Store in `ik_response_t.provider_data` as yyjson_val:
```json
{
  "thought_signature": "encrypted_signature_string_from_gemini"
}
```

**Signature Resubmission (Request Building):**

When building a request with conversation history:
1. Iterate through messages in reverse order (most recent first)
2. For each ASSISTANT message, check provider_data for thought_signature
3. If found, extract signature value
4. When serializing that message to request, add thoughtSignature part FIRST in parts array
5. Only include the most recent thought signature (don't accumulate)

**Request Wire Format:**

When thought signature is present, add as first part in content.parts[]:
```json
{
  "role": "model",
  "parts": [
    {
      "thoughtSignature": "signature_value_here"
    },
    {
      "text": "The actual response content..."
    },
    {
      "functionCall": {...}
    }
  ]
}
```

**Model Detection:**

Only apply thought signature logic for Gemini 3 models:
- Use `ik_google_model_series()` to check if model is IK_GEMINI_3
- For Gemini 2.5 and other models, skip signature extraction and resubmission
- This optimization avoids unnecessary JSON parsing overhead

**Error Handling:**

- If signature extraction fails due to JSON parsing error, log warning but continue (non-fatal)
- If signature resubmission fails during request building, return ERR
- Missing signatures in responses are valid (not all responses include them)
- NULL or empty signatures should not create parts in request

## Test Scenarios

**Signature Extraction:**
- Response with thoughtSignature field extracts correctly
- Response without thoughtSignature returns OK with NULL signature
- Invalid JSON in response returns ERR
- Signature stored in provider_data matches extracted value

**Signature Storage:**
- Extracted signature creates provider_data JSON object
- provider_data has correct schema: {"thought_signature": "value"}
- NULL signature leaves provider_data as NULL
- Signature persists through response talloc lifecycle

**Signature Resubmission:**
- Message with thought signature in provider_data adds thoughtSignature part
- thoughtSignature part is FIRST in parts array
- Message without signature does not add thoughtSignature part
- Only most recent signature from conversation history is used

**Model Series Gating:**
- Gemini 3 models process thought signatures
- Gemini 2.5 models skip signature extraction (returns OK with NULL)
- Non-Gemini models skip signature logic entirely

**Round-Trip Integration:**
- Parse response with signature → store in provider_data → serialize to request
- Verify signature value preserved exactly (no encoding/escaping issues)
- Multi-turn conversation maintains signature across turns

**Edge Cases:**
- Empty string signature treated as NULL (no part added)
- Malformed provider_data JSON logs warning and continues
- Multiple messages with signatures uses only most recent
- Signature in non-ASSISTANT message is ignored

## Implementation Notes

**Response Parsing Integration:**

Modify `ik_google_parse_response()` to:
1. After parsing content blocks, call `ik_google_extract_thought_signature()`
2. If signature found, call `ik_google_store_thought_signature()` to add to response->provider_data
3. Use yyjson_mut API to build provider_data JSON object

**Request Building Integration:**

Modify `ik_google_serialize_request()` to:
1. Before serializing each message's content blocks, call `ik_google_get_thought_signature_from_message()`
2. If signature found, call `ik_google_add_thought_signature_part()` to prepend to parts array
3. Ensure thoughtSignature part is added before any text/tool parts

**Memory Management:**

- All signature strings use talloc for automatic cleanup
- provider_data yyjson_val owned by response struct
- Signature extraction creates temporary strings on response context
- No manual free() calls needed

**Performance Considerations:**

- Signature extraction only runs for Gemini 3 models (early exit for others)
- JSON parsing uses yyjson fast path for simple string extraction
- Signature lookup in messages uses single iteration (not nested loops)

## Postconditions

- [ ] `src/providers/google/thought_signature.h` exists (if separate file preferred)
- [ ] `src/providers/google/thought_signature.c` implements extraction and resubmission
- [ ] `ik_google_extract_thought_signature()` extracts signature from response JSON
- [ ] `ik_google_store_thought_signature()` stores signature in provider_data
- [ ] `ik_google_get_thought_signature_from_message()` retrieves signature from message
- [ ] `ik_google_add_thought_signature_part()` adds thoughtSignature to request parts
- [ ] Response parsing integrated with signature extraction
- [ ] Request serialization integrated with signature resubmission
- [ ] Model series check gates signature logic (Gemini 3 only)
- [ ] Makefile updated with thought_signature.c (if separate file)
- [ ] All thought signature tests pass
- [ ] Integration test verifies round-trip preservation
- [ ] Changes committed to git with message: `task: google-thought-signatures.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
