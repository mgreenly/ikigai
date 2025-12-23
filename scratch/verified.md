# Verified Gaps

Fixed gaps - do not re-investigate.

## Fixed

### 2024-12-22: SSE Parser API Mismatch
**Location:** `scratch/tasks/sse-parser.md`
**Problem:** SSE parser is pull-based (`ik_sse_parser_next()`) but streaming tasks assumed push-based/callback API.
**Fix:** Added "Callback Integration Pattern" section - API is PULL-based; caller runs extraction loop. Includes curl callback example.

### 2024-12-22: ik_thinking_level_t Duplicate Definition
**Location:** `scratch/tasks/agent-provider-fields.md`
**Problem:** Both `provider-types.md` and `agent-provider-fields.md` defined `ik_thinking_level_t` enum.
**Fix:** Changed agent-provider-fields.md to "Enums to use:" referencing `src/providers/provider.h`. Enum defined ONLY in provider-types.md.

### 2024-12-22: ik_infer_provider() Circular Dependency
**Location:** `scratch/tasks/model-command.md`, `scratch/tasks/agent-provider-fields.md`, `scratch/tasks/provider-types.md`
**Problem:** Circular: agent-provider-fields.md → model-command.md → agent-provider-fields.md.
**Fix:** Moved `ik_infer_provider()` to provider-types.md. Linear chain: provider-types.md → agent-provider-fields.md → model-command.md.

### 2024-12-22: ik_http_completion_t vs ik_provider_completion_t Confusion
**Location:** `scratch/tasks/http-client.md`
**Problem:** Tasks used types interchangeably without clarifying abstraction layers.
**Fix:** Added "Completion Type Mapping" section:
- `ik_http_completion_t`: LOW-LEVEL (HTTP client, raw CURL data)
- `ik_provider_completion_t`: HIGH-LEVEL (provider API, parsed responses)
- Provider implementations convert between them

### 2024-12-22: Factory Function Signature Mismatch
**Location:** `scratch/tasks/openai-shim-types.md`, `scratch/tasks/provider-factory.md`
**Problem:** OpenAI create function used `ik_credentials_t *creds` but Anthropic/Google used `const char *api_key`. Factory expected consistent signatures.
**Fix:** Changed `ik_openai_create()` to take `const char *api_key`. Removed credentials dependency from openai-shim-types.md. All providers now have consistent signatures for factory dispatch.

### 2024-12-22: Missing retry_after_ms in ik_provider_completion_t Struct Table
**Location:** `scratch/tasks/provider-types.md`
**Problem:** Struct table for `ik_provider_completion_t` omitted `retry_after_ms` field, but detailed description included it.
**Fix:** Added `retry_after_ms | int32_t | Suggested retry delay (-1 if not applicable)` to struct table. Table now matches detailed description.
