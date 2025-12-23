# Verified Gaps

Gaps that have been reviewed and fixed. Do not re-investigate these.

## Fixed

### 2024-12-22: SSE Parser API Mismatch (CRITICAL)

**Location:** `scratch/tasks/sse-parser.md`

**Problem:** SSE parser task defined a pull-based API (`ik_sse_parser_next()`) but streaming tasks (anthropic-streaming.md, google-streaming.md, openai-streaming-*.md) incorrectly assumed a push-based/callback API where the parser would invoke callbacks.

**Fix:** Added "Callback Integration Pattern" section to sse-parser.md clarifying:
- API is PULL-based, not callback-based
- Parser accumulates data and provides events on demand
- Caller is responsible for the extraction loop
- Includes complete code example of curl write callback integration

**Impact:** Streaming implementations now have clear guidance on how to integrate with the SSE parser using the correct pull-based pattern.

### 2024-12-22: ik_thinking_level_t Duplicate Definition (CRITICAL)

**Location:** `scratch/tasks/agent-provider-fields.md`

**Problem:** Both `provider-types.md` and `agent-provider-fields.md` instructed to "define" the same `ik_thinking_level_t` enum, which would cause a compiler "redefinition" error.

**Fix:** Changed `agent-provider-fields.md` from "Enums to define:" to "Enums to use:" with explicit reference to `src/providers/provider.h` (defined by provider-types.md). The enum is now defined ONLY in provider-types.md.

**Impact:** No duplicate definition - enum defined once in provider-types.md, referenced elsewhere.

### 2024-12-22: ik_infer_provider() Circular Dependency (CRITICAL)

**Location:** `scratch/tasks/model-command.md`, `scratch/tasks/agent-provider-fields.md`, `scratch/tasks/provider-types.md`

**Problem:** Circular dependency between tasks:
- `agent-provider-fields.md` referenced `ik_infer_provider()` from `model-command.md`
- `model-command.md` depended on `agent-provider-fields.md`
- Neither task could be implemented first

**Fix:** Moved `ik_infer_provider()` function definition to `provider-types.md` (the foundation task):
- Added function spec to provider-types.md with model prefix mappings
- Updated agent-provider-fields.md to reference provider-types.md
- Updated model-command.md to use function from provider-types.md instead of defining it

**Impact:** Dependency chain is now linear: provider-types.md → agent-provider-fields.md → model-command.md

### 2024-12-22: ik_http_completion_t vs ik_provider_completion_t Type Confusion (CRITICAL)

**Location:** `scratch/tasks/http-client.md`

**Problem:** Multiple tasks used `ik_http_completion_t` and `ik_provider_completion_t` interchangeably without clarifying:
- Which type belongs to which abstraction layer
- When to use which type
- How provider implementations convert between them

**Fix:** Added "Completion Type Mapping: HTTP to Provider Layer" section to http-client.md documenting:
- `ik_http_completion_t` is LOW-LEVEL (HTTP client internal, raw HTTP/CURL data)
- `ik_provider_completion_t` is HIGH-LEVEL (provider API, parsed responses, error categories)
- Provider implementations are responsible for the conversion
- Includes pseudo-code example of the conversion pattern

**Impact:** Clear abstraction boundary documented; implementers know which type to use at each layer.
