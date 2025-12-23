# Verified Gaps

Fixed gaps - do not re-investigate.

| Date | Gap | Location | Fix |
|------|-----|----------|-----|
| 12-22 | SSE Parser API Mismatch | sse-parser.md | Added "Callback Integration Pattern" - API is PULL-based, caller runs extraction loop |
| 12-22 | ik_thinking_level_t Duplicate | agent-provider-fields.md | Enum defined ONLY in provider-types.md; others reference it |
| 12-22 | ik_infer_provider() Circular Dep | provider-types.md, model-command.md | Moved to provider-types.md. Chain: provider-types→agent-provider-fields→model-command |
| 12-22 | Completion Type Confusion | http-client.md | Added mapping: ik_http_completion_t=LOW-LEVEL, ik_provider_completion_t=HIGH-LEVEL |
| 12-22 | Factory Signature Mismatch | openai-shim-types.md | Changed ik_openai_create() to `const char *api_key` (matches Anthropic/Google) |
| 12-22 | Missing retry_after_ms | provider-types.md | Added `retry_after_ms \| int32_t` to struct table |
| 12-22 | Stream Callback Ownership | anthropic-streaming.md | Stream ctx takes only stream_cb; completion_cb passed to start_stream() |
| 12-22 | HTTP Multi Function Name | anthropic-streaming.md | Changed ik_http_multi_add_handle→ik_http_multi_add_request |
| 12-22 | Missing Include Order | provider-types.md | Added section: provider.h must be included first |
| 12-22 | VCR Chunk Format | vcr-core.md | Chunks are null-terminated strings; len_out for optimization |
| 12-22 | Thinking Validation Missing | provider-types.md | Added validation rules for incompatible model+thinking combinations |
| 12-22 | VCR Makefile Dependencies | vcr-makefile.md | Added test binary deps to vcr-record-* targets |
| 12-22 | Stream Event Index Semantics | provider-types.md | Canonical rules: text=0, tools sequential, thinking=0/text=1 |
| 12-22 | REPL Callback Migration | repl-streaming-updates.md | Added OLD→NEW signature comparison and migration notes |
| 12-22 | Config API Migration | configuration.md | Added ATOMIC strategy, ik_cfg_t→ik_config_t rename, sed commands, 4-phase order |
| 12-22 | Enum Prefix Inconsistency | provider-types.md | All enum values now use consistent prefixes: IK_ERR_CAT_, IK_FINISH_, IK_STREAM_, IK_CONTENT_, IK_TOOL_, IK_ROLE_, IK_THINKING_ |
| 12-22 | Responses Callback Ownership | openai-streaming-responses.md | Removed completion_cb/ctx from stream context; completion callback passed to start_stream() vtable method (matches canonical pattern) |
| 12-22 | Responses API Tool Format | openai-request-responses.md | Corrected tool format to use SAME nested structure as Chat Completions (type + function object), not "name at top level" |
| 12-22 | Config Provider Defaults | configuration.md | Removed per-provider defaults from config; providers hardcode their own defaults via lookup function. Config only stores default_provider. |
| 12-22 | VCR Clean Slate Approach | vcr-fixtures-setup.md, plan/*.md | Documented clean slate: old code/tests/fixtures DELETED, only new provider code and VCR fixtures remain. No migration. |
| 12-22 | README.md Scope Creep | scratch/README.md | Restructured from 1116→214 lines. Product-focused only; technical details in plan/*.md. |
| 12-22 | Model Naming Inconsistency | scratch/README.md | Changed dots to hyphens (claude-sonnet-4.5 → claude-sonnet-4-5) to match Anthropic naming |
| 12-22 | Refactor vs Clean Slate | scratch/plan/README.md | Clarified "Coexistence-Then-Removal" approach: build new alongside old, verify, then delete old |
| 12-22 | Plan Directory Disorganized | scratch/plan/ | Reorganized into 01-architecture/, 02-data-formats/, 03-provider-types.md, 04-application/, 05-testing/. Merged transformation+thinking docs. Deduplicated constraint block. Deleted fix-verify-mocks-precondition.md. |
| 12-22 | No Phase 2 Deletion Gate | scratch/plan/README.md | Added explicit "Phase 2 Prerequisites" checklist with 11 verification criteria before old code deletion |
| 12-22 | src/client.c Fate Contradiction | scratch/plan/01-architecture/overview.md | Clarified: src/client.c (main entry) STAYS, src/openai/client.c (HTTP client) DELETED. Fixed 3 locations. |
| 12-22 | Missing Cancel In-Flight Request | scratch/plan/01-architecture/provider-interface.md | Added cancel() method to vtable + "Request Cancellation" section with 6-step flow, memory cleanup, async-signal-safe requirements |
| 12-22 | /model Switch During Active Request | scratch/plan/04-application/commands.md | Specified: reject switch with error message, check curl_still_running, user can wait or Ctrl+C |
