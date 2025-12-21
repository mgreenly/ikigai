# Task: OpenAI Provider Core Structure

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** openai-adapter-shim.md, provider-types.md, credentials-core.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/provider.h` - Vtable and type definitions
- `src/providers/openai/adapter_shim.c` - Existing shim to replace
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/thinking-abstraction.md` - Reasoning effort mapping

## Objective

Create native OpenAI provider to replace the adapter shim. This establishes directory structure, headers, factory registration, and reasoning effort mapping. The shim remains functional until all 4 tasks complete; `cleanup-old-openai.md` deletes it.

## Key Differences from Anthropic/Google

| Aspect | Anthropic | Google | OpenAI |
|--------|-----------|--------|--------|
| Thinking | Token budget | Budget (2.5) or Level (3.x) | Effort string (low/med/high) |
| APIs | Single | Single | Two (Chat Completions + Responses) |
| System prompt | `system` field | `systemInstruction` | First message with role "system" |
| Tool args | Parsed object | Parsed object | **JSON string** (must parse) |

## Interface

### Functions to Implement

| Function | Purpose |
|----------|---------|
| `ik_openai_create(ctx, api_key, out_provider)` | Create OpenAI provider with Chat Completions API (default) |
| `ik_openai_create_with_options(ctx, api_key, use_responses_api, out_provider)` | Create OpenAI provider with optional Responses API mode |
| `ik_openai_is_reasoning_model(model)` | Check if model supports reasoning.effort parameter (o1, o3, o1-mini, o3-mini) |
| `ik_openai_reasoning_effort(level)` | Map internal thinking level to "low", "medium", "high", or NULL |
| `ik_openai_supports_temperature(model)` | Check if model supports temperature (reasoning models do NOT) |
| `ik_openai_prefer_responses_api(model)` | Determine if model should use Responses API (reasoning models perform 3% better) |

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_ctx_t` | ctx (TALLOC_CTX*), api_key (char*), base_url (char*), use_responses_api (bool) | Provider implementation context |

### Constants

- `IK_OPENAI_BASE_URL` = "https://api.openai.com"
- `IK_OPENAI_CHAT_ENDPOINT` = "/v1/chat/completions"
- `IK_OPENAI_RESPONSES_ENDPOINT` = "/v1/responses"
- Reasoning model prefixes: "o3", "o1", "o4" (array with NULL terminator)

## Behaviors

### Model Detection

- `ik_openai_is_reasoning_model()` checks if model name starts with recognized prefixes (o1, o3, o4)
- Must validate character after prefix is '\0', '-', or '_' to avoid false matches (e.g., "o30" is not reasoning)
- Return false for NULL or empty model names

### Reasoning Effort Mapping

- `IK_THINKING_NONE` → NULL (don't include reasoning config)
- `IK_THINKING_LOW` → "low"
- `IK_THINKING_MED` → "medium"
- `IK_THINKING_HIGH` → "high"
- Unknown levels → NULL

### API Selection

- Reasoning models (o1/o3) should prefer Responses API for 3% better performance
- Non-reasoning models (gpt-4o, gpt-5) use Chat Completions API
- `use_responses_api` context flag can override default behavior

### Temperature Support

- Reasoning models do NOT support temperature parameter
- Check via `!ik_openai_is_reasoning_model(model)`

### Provider Factory

- Factory functions allocate provider on talloc context
- Copy api_key and base_url to provider-owned memory
- Set vtable to `&OPENAI_VTABLE` (defined in openai.c)
- Provider name should be "openai"
- Vtable functions `send` and `stream` are forward-declared (implemented in later tasks)

### Directory Structure

```
src/providers/openai/
├── openai.h         - Public interface
├── openai.c         - Factory and vtable
├── reasoning.h      - Reasoning effort mapping
├── reasoning.c      - Reasoning implementation
├── adapter_shim.c   - (KEEP for now, deleted by cleanup task)
```

## Test Scenarios

### Reasoning Model Detection

- Verify o1, o1-mini, o1-preview identified as reasoning models
- Verify o3, o3-mini, o4-preview identified as reasoning models
- Verify gpt-4o, gpt-4o-mini, gpt-5 are NOT reasoning models
- Verify NULL and empty strings return false
- Verify "o30" is NOT identified as reasoning model (false prefix match)

### Reasoning Effort Mapping

- NONE → NULL
- LOW → "low"
- MED → "medium"
- HIGH → "high"

### Temperature Support

- Reasoning models (o1, o3) → false
- Regular models (gpt-4o, gpt-5) → true

### API Preference

- Reasoning models → prefer Responses API (true)
- Regular models → prefer Chat Completions (false)

### Provider Creation

- `ik_openai_create()` returns valid provider with default (Chat Completions)
- `ik_openai_create_with_options()` respects use_responses_api flag
- Provider name is "openai"
- Vtable has non-null send and stream function pointers
- API key and base URL copied to provider context

### Factory Integration

- `ik_provider_create(ctx, "openai", api_key, &provider)` uses native implementation
- Shim remains in codebase but is no longer referenced

## Postconditions

- [ ] `src/providers/openai/openai.h` exists with factory declarations
- [ ] `src/providers/openai/openai.c` implements factory functions
- [ ] `src/providers/openai/reasoning.h` and `.c` exist
- [ ] Reasoning model detection works for o1/o3 prefixes
- [ ] Reasoning effort mapping returns correct strings
- [ ] Temperature support check works correctly
- [ ] API preference logic implemented
- [ ] Provider factory updated in `provider_common.c`
- [ ] `adapter_shim.c` still exists but is unused
- [ ] Makefile updated with new sources
- [ ] All reasoning tests pass
- [ ] Compiles without warnings
- [ ] `make check` passes
