# Task Granularity Review - Fixes Required

**Date:** 2025-12-21
**Purpose:** Prevent agent context overflow during task execution
**Scope:** All 38 task files in `scratch/tasks/`

## Context

Six sub-agents analyzed all task files to determine if steps are granular enough for agents to complete without running out of context. A typical agent can hold ~15-20 files comfortably before degradation.

**Key insight:** Tasks with complete inline code work well. Tasks requiring agents to "discover patterns" or "update all files matching X" cause context overflow.

---

## Critical Issues (Must Fix Before Execution)

### 1. credentials-tests.md - COMPLETED

**Problem:** Requires modifying 40+ test files simultaneously
**Risk:** Agent will forget changes, miss files, make inconsistent edits

**Split into 5 sub-tasks:** Created, added to `order.json`, original deleted.

| New Task | Files | Scope |
|----------|-------|-------|
| `credentials-tests-helpers.md` | 3 | `tests/test_utils.c`, `tests/helpers/test_contexts.c`, helper test |
| `credentials-tests-config.md` | 8 | All files in `tests/unit/config/` |
| `credentials-tests-openai.md` | 16 | All files in `tests/unit/openai/` |
| `credentials-tests-repl.md` | 3 | REPL test files with openai_api_key refs |
| `credentials-tests-integration.md` | 9 | `tests/integration/` + project docs |

**Pattern for each:** Add `setenv("OPENAI_API_KEY", "test-key", 1)` in setup, `unsetenv()` in teardown, remove direct `cfg->openai_api_key` assignments.

---

### 2. shared-utilities.md - COMPLETED

**Problem:** Vague deliverables ("refactor from existing"), 600+ lines to understand without guidance
**Risk:** Agent spends context understanding code without clear output spec

**Split into 3 sub-tasks:** Created with complete inline code, added to `order.json`, original deleted.

| New Task | Deliverables |
|----------|--------------|
| `http-client.md` | Complete `ik_http_client_t` struct, ~200 lines impl, unit tests |
| `sse-parser.md` | Complete `ik_sse_parser_t` struct, state machine, 9 unit tests |
| `provider-factory.md` | `ik_provider_create()` with credential loading, 8 unit tests |

**Fix approach:** Each task now has complete inline code (~100-200 lines each), not just function names.

---

### 3. error-handling.md

**Problem:** Combines 4 distinct concerns (generic + 3 providers) requiring different API knowledge
**Risk:** Agent needs Anthropic, OpenAI, AND Google API docs simultaneously

**Split into 4 sub-tasks:**

| New Task | Scope |
|----------|-------|
| `error-core.md` | `ik_error_category_name()`, `ik_error_is_retryable()`, `ik_error_user_message()` with complete implementations |
| *(merge into anthropic-core.md)* | Anthropic error mapping, `retry-after` header parsing |
| *(merge into google-core.md)* | Google error mapping, `retryDelay` body parsing |
| *(merge into openai-core.md)* | OpenAI error mapping, `x-ratelimit-reset-*` header parsing |

---

### 4. cleanup-old-openai.md

**Problem:** 15+ files to delete/modify, vague steps like "update documentation"
**Risk:** Incomplete cleanup, broken references

**Split into 4 sub-tasks:**

| New Task | Scope |
|----------|-------|
| `cleanup-openai-source.md` | Verify no deps on `src/openai/`, delete 9 source files, update Makefile |
| `cleanup-openai-adapter.md` | Delete `adapter_shim.c` after native impl verified |
| `cleanup-openai-tests.md` | Explicit test file mapping, verify new tests exist before deleting old |
| `cleanup-openai-docs.md` | `grep -r 'src/openai'` then update each reference explicitly |

---

### 5. tests-provider-common.md

**Problem:** Creates 5+ test files + mock infrastructure in one task
**Risk:** Mock infra is foundational; if rushed, all other tests suffer

**Split into 3 sub-tasks:**

| New Task | Scope |
|----------|-------|
| `tests-mock-infrastructure.md` | MOCKABLE pattern, curl mock, reusable fixtures |
| `tests-common-utilities.md` | HTTP client tests, SSE parser tests |
| `tests-provider-core.md` | Provider creation, request builders, error tests |

---

## Needs Work (Should Fix)

### configuration.md

**Problem:** Only bullet points and JSON schema, no complete struct/function implementations
**Fix:** Add complete `ik_config_t` struct layout, helper function implementations, unit test template

### request-builders.md

**Problem:** Function names without signatures, missing parameter/return types
**Fix:** Add full function signatures like:
```c
res_t ik_request_set_system(TALLOC_CTX *ctx, ik_request_t *req, const char *system_prompt);
```

### repl-provider-abstraction.md

**Problem:** Deliverable 2 (streaming callbacks) has hidden complexity
**Fix:** Split into:
- `repl-provider-routing.md` - vtable dispatch, initialization
- `repl-streaming-updates.md` - callbacks, message saving, thinking deltas

### Test tasks (tests-anthropic.md, tests-google.md, tests-openai.md)

**Problem:** Each creates 11 new files (4 tests + 7 fixtures)
**Fix:** Split each into:
- `tests-{provider}-basic.md` - adapter + client tests + basic fixtures
- `tests-{provider}-streaming.md` - streaming tests + event fixtures

### tests-integration.md

**Problem:** Cross-cutting complexity across all providers
**Fix:** Split into:
- `tests-integration-switching.md` - provider switching, fork inheritance
- `tests-integration-flows.md` - tool calls, error handling, session restoration

---

## Good Tasks (No Changes Needed)

These tasks follow best practices and are ready for execution:

### Layer 0 (Foundation)
- `credentials-core.md` - Complete inline code, 6-8 files
- `credentials-config.md` - Surgical removals, clear before/after
- `credentials-production.md` - Only 2 source files, exact locations
- `provider-types.md` - Header-only, all types verbatim
- `database-migration.md` - Exact SQL, exact struct changes, exact code

### Layer 3 (Anthropic Provider)
- `anthropic-core.md` - 4 source + 1 test, all inline
- `anthropic-request.md` - 2 source + 1 test, wire format examples
- `anthropic-response.md` - 2 source + 1 test, send_impl wiring
- `anthropic-streaming.md` - 2 source + 1 test, state machine complete

### Layer 3 (Google Provider)
- `google-core.md` - Same excellent pattern as Anthropic
- `google-request.md` - Complete serialization code
- `google-response.md` - Minor: could split send_impl but manageable
- `google-streaming.md` - State machine contained

### Layer 2-4 (OpenAI Provider)
- `openai-adapter-shim.md` - Thin wrapper, clear scope
- `openai-core.md` - 285 lines impl + 110 lines tests inline
- `openai-request-chat.md` - Complete serialization, wire examples
- `openai-request-responses.md` - Parallel structure to chat
- `openai-response-chat.md` - Parsing logic complete
- `openai-response-responses.md` - Mirrors chat pattern
- `openai-send-impl.md` - Integration point, 90 lines
- `openai-streaming-chat.md` - State machine for deltas
- `openai-streaming-responses.md` - Event-based architecture

### Commands/Integration
- `agent-provider-fields.md` - Focused on 2 core files
- `model-command.md` - Single file + capability table
- `fork-model-override.md` - Exemplary single-file task

---

## Patterns Identified

### What Works Well
1. **Complete inline code** - Agents copy, don't invent
2. **Explicit pre-read sections** - Limits files to load
3. **3-5 new files max per task** - Stays in context budget
4. **Verification postconditions** - Success is measurable
5. **Wire format examples** - Clarifies expected output

### What Causes Problems
1. **"Update all files matching X"** - Unbounded scope
2. **Vague deliverables** - "Refactor from existing" without guidance
3. **Combining multiple providers** - Different API knowledge needed
4. **"100% coverage" in one session** - Test tasks try to do too much
5. **Missing function signatures** - Agent must guess parameters

---

## Execution Order After Fixes

Once splits are applied, update `order.json` layers:

```
Layer 0: credentials-core, credentials-config, credentials-production
         credentials-tests-helpers (new)
         provider-types, database-migration

Layer 1: http-client (new), sse-parser (new), provider-factory (new)
         error-core (new), request-builders (with signatures)

Layer 2: credentials-tests-config (new), credentials-tests-openai (new)
         openai-adapter-shim, configuration (with complete code)
         agent-provider-fields, tests-mock-infrastructure (new)

Layer 3: credentials-tests-repl (new), credentials-tests-integration (new)
         model-command, repl-provider-routing (new)
         anthropic-*, google-*, tests-common-utilities (new)

Layer 4: repl-streaming-updates (new), fork-model-override
         openai-core through openai-streaming-*
         tests-provider-core (new)

Layer 5: tests-anthropic-basic (new), tests-anthropic-streaming (new)
         tests-google-basic (new), tests-google-streaming (new)
         tests-openai-basic (new), tests-openai-streaming (new)

Layer 6: cleanup-openai-source (new), cleanup-openai-adapter (new)
         cleanup-openai-tests (new), cleanup-openai-docs (new)

Layer 7: tests-integration-switching (new), tests-integration-flows (new)
```

---

## Action Items

1. [x] Split `credentials-tests.md` into 5 files
2. [x] Split `shared-utilities.md` into 3 files with complete code
3. [ ] Split `error-handling.md` - move provider-specific to core tasks
4. [ ] Split `cleanup-old-openai.md` into 4 files
5. [ ] Split `tests-provider-common.md` into 3 files
6. [ ] Add complete code to `configuration.md`
7. [ ] Add function signatures to `request-builders.md`
8. [ ] Split `repl-provider-abstraction.md` into 2 files
9. [ ] Split each `tests-{provider}.md` into 2 files
10. [ ] Split `tests-integration.md` into 2 files
11. [ ] Update `order.json` with new task files and layers
