# Verified Fixes (2025-12-23)

| Fix | Files | Change |
|-----|-------|--------|
| Error enum prefix | `tasks/verify-foundation.md` | `IK_ERR_AUTH` → `IK_ERR_CAT_AUTH` |
| Vtable missing cancel | `tasks/provider-types.md`, `tasks/verify-foundation.md` | Added `cancel` method |
| Wrong struct in Step 3 | `tasks/verify-foundation.md` | `ik_http_completion_t` → `ik_provider_completion_t` |
| overview.md callback type | `plan/01-architecture/overview.md` | `ik_http_completion_t` → `ik_provider_completion_t` |
| verify-foundation Step 7/12 | `tasks/verify-foundation.md` | Added `ik_http_completion_t` check, fixed Step 12 refs |
| Provider test callback types | `tasks/tests-openai-basic.md`, `tasks/tests-common-utilities.md` | Provider callbacks use `ik_provider_completion_t` |
| Provider tasks completion flow | `anthropic-streaming.md`, `openai-send-impl.md`, `openai-streaming-chat.md`, `openai-shim-send.md`, `openai-shim-response.md` | HTTP layer delivers `ik_http_completion_t` → provider builds `ik_provider_completion_t` |
| Finish reason enum | `plan/05-testing/contract-anthropic.md` | `IK_FINISH_TOOL_CALLS` → `IK_FINISH_TOOL_USE` |
| Google finish reason | `plan/05-testing/contract-google.md` | `IK_FINISH_SAFETY` → `IK_FINISH_CONTENT_FILTER` |
| Error category names | `plan/05-testing/contract-*.md`, `plan/01-architecture/overview.md` | `ERR_INVALID` → `ERR_INVALID_ARG`, `ERR_PROVIDER` → `ERR_SERVER` |
| Thinking budget values | `plan/05-testing/tests-thinking-levels.md` | Anthropic LOW: 22,016; Google NONE: 128; Google LOW: 11,008 |
| HTTP layer naming | `plan/05-testing/strategy.md`, `plan/01-architecture/overview.md`, `plan/05-testing/tests-performance-benchmarking.md`, `plan/05-testing/vcr-cassettes.md` | `http_client.c` → `http_multi.c` |
| Google MED budget | `plan/03-provider-types.md`, `plan/02-data-formats/request-response.md`, `plan/05-testing/tests-thinking-levels.md` | 21,760 → 21,888 (10 changes) |
| Anthropic MED budget | `plan/02-data-formats/request-response.md` | 43000 → 43008 |
| Error category prefix | `plan/02-data-formats/error-handling.md` | `ERR_*` → `IK_ERR_CAT_*` (58 changes) |
| credentials.json format | `plan/01-architecture/overview.md`, `plan/04-application/configuration.md` | Flat → nested `{"anthropic": {"api_key": "..."}}` |
| Model name suffix | `plan/03-provider-types.md`, `plan/05-testing/strategy.md`, `plan/02-data-formats/request-response.md`, `plan/04-application/database-schema.md`, `plan/05-testing/vcr-cassettes.md` | Standardized to `claude-sonnet-4-5-20250929` (11 changes) |
| Credentials load description | `plan/01-architecture/provider-interface.md` | Document two-function design: `ik_credentials_load()` + `ik_credentials_get()` |
| Dead reference fix | `plan/04-application/commands.md` | `thinking-abstraction.md` → `../03-provider-types.md` |
| Contract thinking budgets | `plan/05-testing/contract-anthropic.md` | ~21000/~42000 → 22,016/43,008 (exact values) |
| Agent field name | `plan/01-architecture/overview.md` | `provider_name` → `provider` (match DB schema) |
| VCR OpenAI example | `plan/05-testing/vcr-cassettes.md` | Chat Completions → Responses API format |
| Cache tokens clarity | `plan/02-data-formats/request-response.md` | Clarified cached_tokens = cache_creation + cache_read |
| Network errors retryable | `plan/02-data-formats/error-handling.md` | IK_ERR_CAT_NETWORK moved to retryable category |
| provider_name cleanup | `plan/04-application/commands.md`, `plan/01-architecture/provider-interface.md` | 3 remaining `provider_name` → `provider` |
| Error prefix cleanup | `plan/05-testing/*.md`, `plan/01-architecture/*.md` | All `ERR_*` → `IK_ERR_CAT_*` (20+ occurrences) |
| Google thinking budgets | `plan/05-testing/contract-google.md` | ~5000/~10000/~20000 → 11,008/21,888/32,768 (exact values) |
| thinking_level DB type | `plan/01-architecture/overview.md` | INTEGER → TEXT (match database-schema.md) |
| Agent context duplicate field | `plan/01-architecture/overview.md` | Renamed duplicate `provider` → `provider_ctx`; fixed `(LOW/MED/HIGH/MAX)` → `(NONE/LOW/MED/HIGH)` |
| Vtable methods in overview | `plan/01-architecture/overview.md` | Added missing methods (timeout, info_read, cleanup, cancel) to vtable description |
| Error enum prefix in task | `tasks/error-core.md` | `IK_ERR_*` → `IK_ERR_CAT_*` (13 occurrences) |
| VCR fixture path alignment | `tasks/vcr-core.md`, `tasks/tests-*.md`, `tasks/verify-mocks-providers.md`, `tasks/openai-shim-compat-tests.md` | `tests/fixtures/{provider}/` → `tests/fixtures/vcr/{provider}/` (42 edits across 9 files) |
| Shim tool call clarification | `tasks/openai-shim-streaming.md` | Clarified shim emits START+DONE (no DELTA) because legacy code accumulates internally |
| VCR provider-types dependency | `tasks/vcr-core.md`, `tasks/vcr-mock-integration.md` | Added `provider-types.md` dependency for callback type definitions |
| verify-mocks VCR dependency | `tasks/verify-mocks-providers.md` | Added `vcr-core.md`, `vcr-mock-integration.md`, `vcr-fixtures-setup.md` dependencies |
| Test tasks fixtures dependency | `tasks/tests-*.md` (9 files) | Added `vcr-fixtures-setup.md` dependency for fixture directory availability |
| Provider vtable cancel method | `plan/03-provider-types.md` | Added `cancel()` method to vtable diagram and async data flow |
| Error enum prefix batch 2 | `tasks/verify-openai-shim.md`, `tasks/http-client.md`, `tasks/provider-types.md`, `tasks/verify-providers.md` | `IK_ERR_*` → `IK_ERR_CAT_*` (17 occurrences across 4 files) |
| verify-infrastructure fixture paths | `tasks/verify-infrastructure.md` | `tests/fixtures/responses/` → `tests/fixtures/vcr/` (JSONL format) |
| HTTP layer filename in cleanup docs | `tasks/cleanup-openai-docs.md` | `http_client.c` → `http_multi.c` |
| Anthropic MED budget consistency | `plan/03-provider-types.md` | `43000` → `43008` in transformation example |
| Model date suffix update | `plan/*.md`, `tasks/*.md`, `findings/*.md`, `README.md` | `20250514` → `20250929` across 12 files (35 replacements); also fixed `claude-sonnet-4-20250514` → `claude-sonnet-4-5-20250929` and `claude-*.5` dot notation → hyphen notation |
| openai-core dependency fix | `tasks/openai-core.md` | Removed `openai-shim-send.md` from dependencies (architecture inversion - native provider should not depend on shim) |
| REPL routing dependency fix | `tasks/repl-provider-routing.md` | Removed `openai-shim-send.md` from dependencies (REPL should be provider-agnostic via vtable) |
| Error enum prefix in test tasks | `tasks/tests-provider-core.md`, `tasks/tests-google-basic.md`, `tasks/tests-anthropic-basic.md` | `ERR_*` → `IK_ERR_CAT_*` (59 replacements across 3 files) |
| Google LOW budget typo | `tasks/google-core.md` | `10008` → `11008` (correct calculation: 128 + 32640/3 = 11008) |
| VCR fixture path in verify-cleanup | `tasks/verify-cleanup.md` | `tests/fixtures/responses/` → `tests/fixtures/vcr/` (line 197) |
