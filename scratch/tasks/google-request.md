# Task: Google Request Serialization

**Model:** sonnet/thinking
**Depends on:** google-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/google/google.c` - Provider context
- `src/providers/google/thinking.h` - Budget/level calculation
- `src/providers/provider.h` - Request types

**Plan:**
- `scratch/plan/transformation.md` - Google transformation rules

## Objective

Implement request serialization for Google Gemini API. Transform internal `ik_request_t` to Google JSON wire format, build API URLs with key parameter, and construct HTTP headers.

## Key Differences from Anthropic

| Field | Anthropic | Google |
|-------|-----------|--------|
| System prompt | `system` (string) | `systemInstruction.parts[]` |
| Messages | `messages[]` | `contents[]` |
| Role | `assistant` | `model` |
| Content | `content[]` | `parts[]` |
| Thinking block | `type: "thinking"` | `thought: true` flag |
| Tools | `tools[]` | `tools[].functionDeclarations[]` |
| Tool choice | `tool_choice.type` | `toolConfig.functionCallingConfig.mode` |
| Max tokens | `max_tokens` | `generationConfig.maxOutputTokens` |
| Thinking | `thinking.budget_tokens` | `thinkingConfig.thinkingBudget` or `.thinkingLevel` |

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_serialize_request(TALLOC_CTX *ctx, ik_request_t *req, char **out_json)` | Serialize internal request to Google JSON, returns OK/ERR |
| `res_t ik_google_build_url(TALLOC_CTX *ctx, const char *base_url, const char *model, const char *api_key, bool streaming, char **out_url)` | Build URL with model method and API key parameter, returns OK/ERR |
| `res_t ik_google_build_headers(TALLOC_CTX *ctx, bool streaming, char ***out_headers)` | Build HTTP headers array (NULL-terminated), returns OK/ERR |

## Behaviors

**Request Serialization:**
- Build JSON object with required "contents" array
- If system_prompt exists, add "systemInstruction.parts[]" with text parts
- Transform each message to content object with "role" and "parts[]"
- Map role: IK_ROLE_USER -> "user", IK_ROLE_ASSISTANT -> "model", IK_ROLE_TOOL -> "function"
- For each content block, serialize as part:
  - IK_CONTENT_TEXT: `{"text": "..."}`
  - IK_CONTENT_THINKING: `{"text": "...", "thought": true}`
  - IK_CONTENT_TOOL_CALL: `{"functionCall": {"name": "...", "args": {...}}}`
  - IK_CONTENT_TOOL_RESULT: `{"functionResponse": {"name": "...", "response": {"content": "..."}}}`
- If tools exist, wrap in `{"tools": [{"functionDeclarations": [...]}]}`
- Add tool config: `{"toolConfig": {"functionCallingConfig": {"mode": "..."}}}`
- Map tool choice: NONE -> "NONE", AUTO -> "AUTO", REQUIRED -> "ANY"
- Add generationConfig if needed (maxOutputTokens, thinkingConfig)
- For Gemini 2.5 models with thinking: add `{"thinkingBudget": N, "includeThoughts": true}`
- For Gemini 3 models with thinking: add `{"thinkingLevel": "LOW"/"HIGH", "includeThoughts": true}`
- Use yyjson for JSON construction
- Return talloc-allocated JSON string

**URL Building:**
- Format: `{base_url}/models/{model}:{method}?key={api_key}[&alt=sse]`
- Non-streaming method: "generateContent"
- Streaming method: "streamGenerateContent" with "&alt=sse" parameter
- API key passed as query parameter (not header)
- Return talloc-allocated URL string

**Headers Building:**
- Non-streaming: ["Content-Type: application/json", NULL]
- Streaming: ["Content-Type: application/json", "Accept: text/event-stream", NULL]
- Return talloc-allocated NULL-terminated array
- Google needs fewer headers than Anthropic (no API key header)

**Error Handling:**
- Return ERR(ctx, PARSE, ...) on JSON serialization failure
- All allocations use talloc for automatic cleanup
- Panic on out-of-memory conditions

## Test Scenarios

**Simple Text Request:**
- User message with text content serializes to `{"contents": [{"role": "user", "parts": [{"text": "..."}]}]}`
- Verify contents array exists and has correct structure

**System Instruction:**
- Request with system_prompt serializes systemInstruction.parts[] correctly
- Text content extracted from system_prompt content blocks

**Assistant Role Mapping:**
- IK_ROLE_ASSISTANT maps to "model" role in contents

**Thinking Serialization (Gemini 2.5):**
- Request with thinking level and gemini-2.5-flash model includes generationConfig.thinkingConfig
- thinkingBudget calculated correctly (MED = 16384 for gemini-2.5-flash)
- includeThoughts set to true

**Thinking Serialization (Gemini 3):**
- Request with thinking HIGH and gemini-3-pro includes thinkingLevel: "HIGH"
- includeThoughts set to true

**Thinking Block Content:**
- IK_CONTENT_THINKING serializes with thought: true flag
- Text included in part

**Tool Serialization:**
- Tools wrapped in functionDeclarations array
- Tool config includes functionCallingConfig.mode
- Tool definitions include name, description, parameters schema

**URL Building (Non-Streaming):**
- Generates URL with generateContent method
- API key in query parameter
- No alt=sse parameter

**URL Building (Streaming):**
- Generates URL with streamGenerateContent method
- Includes &alt=sse parameter
- API key in query parameter

**Headers (Non-Streaming):**
- Contains Content-Type: application/json
- NULL-terminated array
- Second element is NULL

**Headers (Streaming):**
- Contains Content-Type and Accept headers
- Accept: text/event-stream present
- NULL-terminated array with third element NULL

## Postconditions

- [ ] `src/providers/google/request.h` exists
- [ ] `src/providers/google/request.c` implements serialization
- [ ] `ik_google_serialize_request()` produces valid Google JSON
- [ ] System prompt serialized as `systemInstruction.parts[]`
- [ ] Messages serialized as `contents[]` with `parts[]`
- [ ] Assistant role maps to `"model"`
- [ ] Thinking blocks have `thought: true` flag
- [ ] Tools serialized with `functionDeclarations` wrapper
- [ ] Tool choice maps: NONE->"NONE", AUTO->"AUTO", REQUIRED->"ANY"
- [ ] Gemini 2.5 thinking uses `thinkingBudget`
- [ ] Gemini 3 thinking uses `thinkingLevel`
- [ ] URL includes `?key=` parameter
- [ ] Makefile updated with request.c
- [ ] All serialization tests pass
- [ ] Changes committed to git with message: `task: google-request.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).