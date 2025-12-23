# Verified Items - Do Not Re-Check

## 2024-12-22: Task Precondition Review

### Fixed
- **vcr-core.md**: Credential path corrected from `src/providers/common/credentials.h` to `src/credentials.h` (lines 29, 42)
- **tests-anthropic-basic.md**: Added vcr-core.md dependency, Recording Fixtures section, JSONL format, credential leak check
- **tests-anthropic-streaming.md**: Added vcr-core.md dependency, Recording Fixtures section, JSONL format, credential leak check
- **tests-google-basic.md**: Added vcr-core.md dependency, Recording Fixtures section, JSONL format, credential leak check
- **tests-google-streaming.md**: Added vcr-core.md dependency, Recording Fixtures section, JSONL format, credential leak check; updated precondition to reference VCR instead of deleted tests-mock-infrastructure.md
- **tests-openai-basic.md**: Added vcr-core.md dependency, Recording Fixtures section, JSONL format, credential leak check
- **tests-openai-streaming.md**: Added vcr-core.md dependency, Recording Fixtures section, JSONL format, credential leak check; updated precondition to reference VCR instead of deleted tests-mock-infrastructure.md
- **vcr-core.md**: Fixed vcr_next_chunk() signature from `const char* (void)` to `bool (const char **data_out, size_t *len_out)` to match vcr-mock-integration.md usage
- **verify-mocks-providers.md**: Added `verify-credentials` Makefile target to interface table and implementation section
- **anthropic-response.md**: Renamed `ik_anthropic_send_impl()` to async `ik_anthropic_start_request()` and added `ik_anthropic_start_stream()` to match vtable
- **google-response.md**: Renamed `ik_google_send_impl()` to async `ik_google_start_request()` and added `ik_google_start_stream()` to match vtable
- **verify-infrastructure.md**: Replaced tests-mock-infrastructure.md dependency with vcr-core.md, vcr-mock-integration.md
- **tests-provider-core.md**: Replaced tests-mock-infrastructure.md dependency with vcr-core.md, vcr-mock-integration.md; updated mock pattern section to VCR
- **tests-common-utilities.md**: Replaced tests-mock-infrastructure.md dependency with vcr-core.md, vcr-mock-integration.md
- **tests-integration-switching.md**: Replaced tests-mock-infrastructure.md precondition with VCR infrastructure preconditions
- **openai-shim-streaming.md**: Replaced tests-mock-infrastructure.md reference with VCR infrastructure references
- **openai-send-impl.md**: Added openai-core.md to dependencies (provides `ik_openai_ctx_t` type)
- **openai-shim-types.md**: Added `multi (ik_openai_multi_t*)` to `ik_openai_shim_ctx_t` struct and multi-handle initialization in factory stub
- **repl-streaming-updates.md**: Fixed `ik_repl_stream_callback` signature from `void (ik_stream_event_t*, void*)` to `res_t (const ik_stream_event_t*, void*)` to match vtable
- **vcr-makefile.md**: Removed circular dependency on tests-*-basic.md; now depends only on vcr-mock-integration.md; updated precondition from "VCR-integrated provider tests exist" to "VCR infrastructure ready"
- **vcr-core.md**: Added missing `vcr_is_active()` function declaration (called by vcr-mock-integration.md but was not in API); added behavior documentation and test scenarios

### Known Issues (Not Yet Fixed)
| Task | Issue | Severity |
|------|-------|----------|
| ~~verify-mocks-providers.md~~ | ~~References `make verify-credentials` target that doesn't exist~~ | ~~HIGH~~ FIXED |
| ~~vcr-mock-integration.md~~ | ~~`vcr_next_chunk()` signature mismatch with vcr-core.md~~ | ~~HIGH~~ FIXED |
| ~~anthropic-response.md~~ | ~~Defines blocking `ik_anthropic_send_impl()` but provider-types.md mandates async vtable~~ | ~~CRITICAL~~ FIXED |
| ~~google-response.md~~ | ~~Defines blocking `ik_google_send_impl()` but provider-types.md mandates async vtable~~ | ~~CRITICAL~~ FIXED |
| ~~provider-factory.md~~ | ~~Circular dependency - needs provider-create functions from downstream tasks~~ | ~~HIGH~~ FALSE ALARM |
| ~~openai-send-impl.md~~ | ~~Missing dependency on openai-core.md (uses `ik_openai_ctx_t` defined there)~~ | ~~HIGH~~ FIXED |
| ~~configuration.md~~ | ~~Missing dependency on credentials-migrate.md~~ | ~~MEDIUM~~ FALSE ALARM (tasks are independent, no data flow) |
| ~~configuration.md~~ | ~~Missing "update all call sites" for ik_cfg_load rename (72+ locations)~~ | ~~HIGH~~ FIXED (added Call Site Updates section documenting 65+ call sites across 13 files) |
| ~~tests-mock-infrastructure.md~~ | ~~DELETED - test tasks still reference it (replaced by VCR)~~ | ~~INFO~~ FIXED |

### Verified Groups (No Blocking Issues)
- provider-types.md - Clear
- http-client.md - Clear
- sse-parser.md - Clear
- error-core.md - Clear

## 2024-12-22: Comprehensive Subagent Review

### Review Groups Completed
- Credentials tasks (credentials-core, credentials-migrate, credentials-tests-*)
- Provider core tasks (provider-factory, provider-types, openai-core, anthropic-core, google-core)
- OpenAI shim tasks (openai-shim-types, openai-shim-request, openai-shim-response, openai-shim-streaming, openai-shim-send)
- VCR tasks (vcr-core, vcr-fixtures-setup, vcr-makefile, vcr-mock-integration, vcr-precommit-hook)
- Request/response tasks (request-builders, *-request, *-response for all providers)
- REPL integration tasks (repl-provider-routing, repl-streaming-updates, agent-provider-fields, configuration, database-migration)

### Additional Issues Discovered
| Task | Issue | Severity |
|------|-------|----------|
| ~~All response tasks~~ | ~~Finish reason mapping inconsistent across providers (529, 504 handled differently)~~ | ~~MEDIUM~~ NOT A BUG - intentional per-API design, added documentation notes |
| ~~All request tasks~~ | ~~Tool argument serialization differs (OpenAI stringifies, others use object) - not documented as intentional~~ | ~~MEDIUM~~ NOT A BUG - OpenAI API requires stringified args, added documentation note |
| ~~vcr-makefile.md~~ | ~~References test binaries that don't exist yet (tests/unit/providers/*)~~ | ~~HIGH (ordering)~~ FALSE ALARM |
| ~~credentials-tests-helpers.md~~ | ~~client_multi_info_read_helpers.h has undefined `ctx` variable at line 11~~ | ~~HIGH~~ FALSE ALARM |
| ~~verify-infrastructure.md~~ | ~~References deleted tests-mock-infrastructure.md in dependencies~~ | ~~HIGH~~ FIXED |
| ~~tests-provider-core.md~~ | ~~References deleted tests-mock-infrastructure.md in dependencies~~ | ~~HIGH~~ FIXED |
| ~~tests-common-utilities.md~~ | ~~References deleted tests-mock-infrastructure.md in dependencies~~ | ~~HIGH~~ FIXED |
| ~~tests-integration-switching.md~~ | ~~References deleted tests-mock-infrastructure.md in preconditions~~ | ~~HIGH~~ FIXED |
| ~~openai-shim-streaming.md~~ | ~~References deleted tests-mock-infrastructure.md in mock pattern section~~ | ~~HIGH~~ FIXED |
| ~~agent-provider-fields.md + model-command.md~~ | ~~Duplicate definition of ik_infer_provider() would cause linker error~~ | ~~HIGH~~ FIXED (model-command.md owns function, agent-provider-fields.md depends on it) |
| ~~repl-provider-routing.md~~ | ~~Missing dependency on provider-factory.md (uses ik_provider_create indirectly)~~ | ~~MEDIUM~~ FALSE ALARM (transitive via agent-provider-fields.md) |
| ~~repl-provider-routing.md~~ | ~~Missing dependency on provider-types.md (uses ik_provider_t, ik_request_t)~~ | ~~HIGH~~ FIXED |
| ~~cleanup-openai-source.md~~ | ~~Missing dependency on openai-equivalence-validation.md~~ | ~~CRITICAL~~ FALSE ALARM (already present) |
| ~~openai-equivalence-validation.md~~ | ~~Missing dependency on openai-shim-streaming.md~~ | ~~CRITICAL~~ FALSE ALARM (already present) |

