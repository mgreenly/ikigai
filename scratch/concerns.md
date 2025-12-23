# Release 07 Plan Review: Concerns

**Date:** 2025-12-22
**Status:** Open - requires resolution before task authoring

This document captures concerns identified during ultrathink plan review. Each concern should be resolved before creating implementation tasks.

---

## Table of Contents

1. [Migration Clarity](#1-migration-clarity)
2. [Async Interface Verification](#2-async-interface-verification)
3. [Phase Breakdown & Testability](#3-phase-breakdown--testability)
4. [Naming Consistency](#4-naming-consistency)
5. [Additional Concerns](#5-additional-concerns)

---

## 1. Migration Clarity

### 1.1 Two Conflicting Phase Numbering Systems

**PROBLEM:** README.md uses "Phase 1/Phase 2" (Coexistence/Removal), but overview.md describes an 8-step migration (Vtable Foundation → Cleanup). These don't align.

**IMPACT:** Implementers won't know which numbering to follow.

**ACTION:** Unify phase numbering or clarify that 8 steps are sub-steps of 2 phases.

---

### 1.2 No Explicit Gate for Phase 2 (Deletion)

**PROBLEM:** Plan says "verify new implementation works" but provides no definition of "works."

**IMPACT:** Implementer may delete old code prematurely or never.

**ACTION:** Add explicit checklist:
```
Before deleting old code:
[ ] All VCR tests for new OpenAI provider pass
[ ] All VCR tests for Anthropic provider pass
[ ] All VCR tests for Google provider pass
[ ] /model command switches providers correctly
[ ] No #include referencing src/openai/ outside src/providers/
```

---

### 1.3 Contradictory Fate of src/client.c

**PROBLEM:**
- overview.md line 344: "Removed (blocking HTTP not used)"
- overview.md line 386: "refactored into src/providers/common/http_client.c"

**IMPACT:** Implementer doesn't know whether to delete or refactor.

**ACTION:** Pick one: removed OR refactored.

---

### 1.4 No File-by-File Deletion List

**PROBLEM:** Plan says "delete old src/openai/" but doesn't enumerate specific files.

**IMPACT:** Partial deletion or missed files.

**ACTION:** Add explicit list of files/directories to delete.

---

### 1.5 No Makefile Migration Plan

**PROBLEM:** Makefile references src/openai/. No specification of when these are updated.

**ACTION:** Document when Makefile changes happen relative to code migration.

---

### 1.6 Config Loading Migration Unclear

**PROBLEM:** Current config.c loads openai_api_key, openai_model. Plan introduces new credential system. No migration path specified.

**ACTION:** Document config.c changes and sequencing.

---

## 2. Async Interface Verification

### 2.1 VERIFIED: Core Async Pattern Correct

The vtable with fdset/perform/timeout/info_read matches existing working pattern in src/openai/client_multi.c. REPL event loop integration is correctly specified.

---

### 2.2 MISSING: Cancel In-Flight Request

**PROBLEM:** No vtable method to cancel active requests.

**SCENARIO:** User presses Ctrl+C during streaming.

**ACTION:** Add `void (*cancel_active)(void *ctx)` to vtable OR document that talloc_free() safely aborts.

---

### 2.3 UNCLEAR: /model Switch with In-Flight Requests

**PROBLEM:** What happens to in-flight requests when user switches models?

**OPTIONS:**
1. Wait for completion
2. Abort immediately
3. Let old provider finish, new requests use new provider

**ACTION:** Specify behavior explicitly.

---

### 2.4 UNDOCUMENTED: Callback Context Lifetime

**PROBLEM:** Who owns stream_ctx and completion_ctx? When safe to free?

**ACTION:** Document that contexts must remain valid until completion_cb returns.

---

### 2.5 UNDOCUMENTED: Stream Callback Error Behavior

**PROBLEM:** If stream_cb returns ERR(), does it abort the transfer?

**REFERENCE:** Existing code aborts on callback error (client_multi_callbacks.c:241-246).

**ACTION:** Document this behavior in vtable specification.

---

### 2.6 CONCERN: Credential Loading Blocks

**PROBLEM:** Credential file I/O may block event loop briefly on first use.

**IMPACT:** Minor - files are small (<1KB).

**ACTION:** Accept or preload at startup.

---

## 3. Phase Breakdown & Testability

### 3.1 Phase 1.4 (Call Site Updates) Is Too Large

**PROBLEM:** Requires changing repl_actions_llm.c, commands_basic.c, completion.c, agent.c simultaneously.

**RISK:** All-or-nothing change. If any site is missed, app crashes.

**ACTION:** Split into smaller steps:
- 1.4a: Add provider field to agent struct
- 1.4b: Update agent creation
- 1.4c: Update one call site at a time
- 1.4d: Enable by default

---

### 3.2 Infrastructure Phases Not Explicitly Sequenced

**PROBLEM:** Database migration, config loading, VCR infrastructure are dependencies but not in implementation order.

**ACTION:** Add "Phase 0: Infrastructure" covering:
- 0.1: Database migration
- 0.2: Configuration loading
- 0.3: VCR infrastructure

---

### 3.3 VCR Bootstrap Circular Dependency

**PROBLEM:** Tests need VCR fixtures. Fixtures need real API calls. API calls need working provider.

**ACTION:** Add explicit bootstrap phase per provider:
1. Implement provider with minimal live test
2. Record VCR fixtures
3. Add full VCR test suite

---

### 3.4 Cleanup Phase Has No Rollback Path

**PROBLEM:** Deleting old code is irreversible.

**ACTION:** Keep old code on branch/tag for 1-2 sprints before permanent deletion.

---

### 3.5 Recommended Phase Order

```
Phase 0.1: Database migration (005-multi-provider.sql)
Phase 0.2: Configuration loading (credentials.json)
Phase 0.3: VCR infrastructure
Phase 1.1: Vtable foundation (provider.h)
Phase 1.2: Common HTTP layer (http_multi.c)
Phase 1.3: OpenAI adapter shim
Phase 1.4a-d: Call site updates (incremental)
Phase 1.5: Anthropic provider (with bootstrap)
Phase 1.6: Google provider (with bootstrap)
Phase 1.7: OpenAI native refactor
Phase 1.8: Cleanup (with branch strategy)
Phase 2.1: /model command
Phase 2.2: /fork command
```

---

## 4. Naming Consistency

### 4.1 CONFLICT: ik_config_t vs ik_cfg_t

**PROBLEM:** Plan uses `ik_config_t`, `ik_config_load()`. Existing code uses `ik_cfg_t`, `ik_cfg_load()`.

**RULE:** Per naming conventions, `cfg` is approved abbreviation.

**ACTION:** Change plan to use `ik_cfg_*` throughout.

---

### 4.2 CONFLICT: ik_error_t vs err_t

**PROBLEM:** Plan proposes `ik_error_t` for provider errors. `err_t` already exists in src/error.h with different structure.

**ACTION:** Rename to `ik_provider_error_t` to avoid collision.

---

### 4.3 MISSING: Enum Type Name for Stream Events

**PROBLEM:** streaming.md lists values (IK_STREAM_START, etc.) but no enum type name.

**ACTION:** Add `ik_stream_event_type_t` as the enum name.

---

### 4.4 MISSING: Error Codes Not Added to Existing Enum

**PROBLEM:** New codes (ERR_AUTH, ERR_RATE_LIMIT, etc.) need to be added to `err_code_t` in src/error.h.

**ACTION:** Document that src/error.h must be extended with new codes.

---

### 4.5 INCOMPLETE: Struct Definitions Missing

These types are referenced but not fully defined:
- `ik_provider_t` - only prose description
- `ik_provider_completion_t` - only prose
- `ik_stream_event_t` - data union documented but not struct
- `ik_http_multi_t` - internal structure not defined

**ACTION:** Add formal C struct definitions.

---

### 4.6 INCOMPLETE: Function Signatures Missing

These functions lack formal signatures:
- `ik_provider_get_or_create()`
- `ik_sse_parse_chunk()`
- `ik_error_category_name()`
- `ik_error_is_retryable()`
- `ik_error_user_message()`

**ACTION:** Add complete C signatures.

---

## 5. Additional Concerns

### Memory Management

#### 5.1 Provider Context Lifecycle During Model Switch

**CONCERN:** What happens to old provider memory when model switches?

**ACTION:** Document talloc ownership: agent owns provider, explicit free before replacement.

---

#### 5.2 yyjson Document Lifetime

**CONCERN:** yyjson_val pointers in request/response could become dangling if document freed.

**ACTION:** Document that yyjson documents must be talloc children of containing struct.

---

### Error Handling

#### 5.3 Content Filter Mapping Incomplete

**CONCERN:** Safety/content filter responses not explicitly mapped for Anthropic/Google.

**ACTION:** Add explicit mapping for content filter responses per provider.

---

#### 5.4 Streaming Error Partial Response Handling

**CONCERN:** Mid-stream error leaves partial response in scrollback. No indication it's incomplete.

**ACTION:** Add "incomplete" status. Display visual indicator in UI.

---

### Configuration

#### 5.5 Missing credentials.json UX

**CONCERN:** First-time user gets error about credentials.json that doesn't exist.

**ACTION:** Either create template file or include creation instructions in error message.

---

#### 5.6 NULL Model State

**CONCERN:** What happens if user sends message before /model is set?

**ACTION:** Specify explicit error: "No model configured. Use /model to select one."

---

### State Management

#### 5.7 Mid-Stream Cancellation Undefined

**CONCERN:** No mechanism for user to cancel streaming response (Ctrl+C).

**ACTION:** Implement cancel:
1. Set cancel flag checked in stream callback
2. Return error from callback to abort
3. Emit IK_STREAM_ERROR "cancelled by user"

---

#### 5.8 Provider State in /fork

**CONCERN:** Does forked agent share parent's provider instance or create its own?

**ACTION:** Document: each agent has own provider instance, created lazily.

---

### Database

#### 5.9 Migration Not Wrapped in Transaction

**CONCERN:** Migration 005 truncates then alters. If it fails partway, database is inconsistent.

**ACTION:** Wrap migration in transaction (BEGIN/COMMIT).

---

### Testing

#### 5.10 No Concurrent Request Tests

**CONCERN:** Architecture supports multiple agents with concurrent requests but no tests verify this.

**ACTION:** Add integration test with multiple simultaneous requests.

---

#### 5.11 Network Failure Simulation Missing

**CONCERN:** VCR tests HTTP responses but not network errors (CURLE_COULDNT_CONNECT, etc.).

**ACTION:** Add mock that simulates curl errors.

---

### Scope

#### 5.12 xAI/Meta Providers Referenced but Not Implemented

**CONCERN:** Provider inference tables include xai/meta but no implementation specified.

**ACTION:** Either remove from rel-07 or add explicit "not yet implemented" error.

---

## Resolution Tracking

| # | Concern | Priority | Status | Resolved In |
|---|---------|----------|--------|-------------|
| 1.1 | Phase numbering conflict | HIGH | Open | |
| 1.2 | No deletion gate | HIGH | Open | |
| 1.3 | src/client.c fate | HIGH | Open | |
| 2.2 | Cancel in-flight | HIGH | Open | |
| 2.3 | Model switch behavior | HIGH | Open | |
| 3.1 | Phase 1.4 too large | HIGH | Open | |
| 4.1 | config vs cfg naming | MED | Open | |
| 4.2 | error_t conflict | MED | Open | |
| 5.7 | Mid-stream cancel | HIGH | Open | |
| 5.9 | Migration transaction | HIGH | Open | |

---

## Next Steps

1. Review concerns with stakeholder
2. Resolve HIGH priority items
3. Update plan documents
4. Re-run plan coherence check
5. Proceed to task authoring
