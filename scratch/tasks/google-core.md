# Task: Google Provider Core Structure

**Layer:** 3
**Model:** sonnet/thinking
**Depends on:** provider-types.md, credentials-core.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/provider.h` - Vtable and type definitions
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/thinking-abstraction.md` - Thinking budget calculation

## Objective

Create Google (Gemini) provider directory structure, headers, factory registration, and thinking budget calculation. This establishes the skeleton that subsequent tasks will fill in.

## Key Differences from Anthropic

| Aspect | Anthropic | Google |
|--------|-----------|--------|
| API Key | Header `x-api-key` | Query param `?key=` |
| Thinking | Token budget only | Budget (2.5) or Level (3.x) |
| Tool IDs | Provider generates | We must generate |
| Roles | user/assistant | user/model |

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out_provider)` | Create Google provider instance, returns OK/ERR |
| `ik_gemini_series_t ik_google_model_series(const char *model)` | Determine which Gemini series (2.5, 3, or OTHER) |
| `int32_t ik_google_thinking_budget(const char *model, ik_thinking_level_t level)` | Calculate thinking budget for Gemini 2.5 models in tokens, returns -1 if not applicable |
| `const char *ik_google_thinking_level_str(ik_thinking_level_t level)` | Get thinking level string ("LOW"/"HIGH") for Gemini 3 models, NULL if NONE |
| `bool ik_google_supports_thinking(const char *model)` | Check if model supports thinking mode |
| `bool ik_google_can_disable_thinking(const char *model)` | Check if thinking can be disabled for model |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_gemini_series_t` | IK_GEMINI_2_5, IK_GEMINI_3, IK_GEMINI_OTHER | Enum for model series identification |
| `ik_google_budget_t` | model_pattern (string), min_budget (int32), max_budget (int32) | Model-specific thinking budget limits for Gemini 2.5 |
| `ik_google_ctx_t` | ctx (TALLOC_CTX*), api_key (string), base_url (string) | Internal provider implementation context |

## Behaviors

**Model Series Detection:**
- When model name contains "gemini-3", classify as IK_GEMINI_3
- When model name contains "gemini-2.5" or "gemini-2.0", classify as IK_GEMINI_2_5
- Otherwise classify as IK_GEMINI_OTHER
- NULL model names return IK_GEMINI_OTHER

**Thinking Budget Calculation (Gemini 2.5):**
- Match model against budget table patterns (gemini-2.5-pro, gemini-2.5-flash, gemini-2.5-flash-lite)
- Use model-specific min/max token budgets
- Map thinking levels to budget range: NONE=min, LOW=min+range/3, MED=min+2*range/3, HIGH=max
- gemini-2.5-pro: min=128, max=32768 (cannot disable)
- gemini-2.5-flash: min=0, max=24576 (can disable)
- gemini-2.5-flash-lite: min=512, max=24576 (cannot fully disable)
- Unknown 2.5 models default to min=0, max=24576

**Thinking Level String (Gemini 3):**
- NONE returns NULL (don't include config)
- LOW and MED both map to "LOW"
- HIGH maps to "HIGH"

**Provider Factory Registration:**
- Register "google" provider in ik_provider_create() dispatch
- Call ik_google_create() when provider name is "google"

**Directory Structure:**
- Create src/providers/google/ directory
- Files: google.h (public), google.c (factory+vtable), thinking.h (internal), thinking.c (implementation)

**Vtable:**
- send function forwards to ik_google_send_impl (implemented in google-request.md)
- stream function forwards to ik_google_stream_impl (implemented in google-streaming.md)
- cleanup is NULL (talloc handles cleanup)

**API Base URL:**
- Use https://generativelanguage.googleapis.com/v1beta

## Test Scenarios

**Model Series Detection:**
- gemini-2.5-pro returns IK_GEMINI_2_5
- gemini-2.5-flash returns IK_GEMINI_2_5
- gemini-2.0-flash returns IK_GEMINI_2_5
- gemini-3-pro returns IK_GEMINI_3
- gemini-1.5-pro returns IK_GEMINI_OTHER
- NULL returns IK_GEMINI_OTHER

**Thinking Budget Calculation:**
- gemini-2.5-pro with NONE returns 128 (minimum, cannot disable)
- gemini-2.5-pro with LOW returns 10008 (128 + 32640/3)
- gemini-2.5-pro with MED returns 21888 (128 + 2*32640/3)
- gemini-2.5-pro with HIGH returns 32768 (maximum)
- gemini-2.5-flash with NONE returns 0 (can disable)
- gemini-2.5-flash with MED returns 16384
- gemini-3-pro returns -1 (uses levels not budgets)

**Thinking Level Strings:**
- NONE returns NULL
- LOW returns "LOW"
- MED returns "LOW" (maps to LOW)
- HIGH returns "HIGH"

**Thinking Support:**
- gemini-2.5-pro supports thinking (true)
- gemini-3-pro supports thinking (true)
- gemini-1.5-pro does not support thinking (false)
- NULL does not support thinking (false)

**Disable Thinking:**
- gemini-2.5-pro cannot disable (min=128, returns false)
- gemini-2.5-flash can disable (min=0, returns true)
- gemini-2.5-flash-lite cannot disable (min=512, returns false)
- gemini-3-pro cannot disable (uses levels, returns false)

**Provider Creation:**
- Create with valid API key returns OK
- Provider name is "google"
- Vtable has send and stream functions
- Implementation context contains copied api_key and base_url

**Factory Registration:**
- ik_provider_create(ctx, "google", "key", &provider) dispatches to ik_google_create()

## Postconditions

- [ ] `src/providers/google/` directory exists
- [ ] `google.h` declares `ik_google_create()`
- [ ] `thinking.h` and `thinking.c` implement budget/level calculation
- [ ] `ik_google_model_series()` correctly identifies 2.5 vs 3 models
- [ ] `ik_google_thinking_budget()` returns correct values for 2.5 models
- [ ] `ik_google_thinking_level_str()` returns LOW/HIGH for 3 models
- [ ] `ik_google_create()` returns valid provider structure
- [ ] Provider registered in `ik_provider_create()` dispatch
- [ ] Makefile updated with new sources
- [ ] `make build/tests/unit/providers/google/thinking_test` succeeds
- [ ] All thinking tests pass
