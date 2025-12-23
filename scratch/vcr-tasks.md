# VCR Cassette Implementation Tasks

**Source Plan:** `scratch/plan/vcr-cassettes.md`

**Purpose:** Guide for creating task files in `scratch/tasks/` format. Each section below becomes a task file.

**Scope:** Create VCR-style HTTP recording/replay infrastructure for deterministic tests.

**Task Size Guideline:** Each task should be completable by a sub-agent in one session (~50-150 lines of code, 1-3 files).

---

## Changes to scratch/tasks/

### Files to DELETE

| File | Reason |
|------|--------|
| `tests-mock-infrastructure.md` | Replaced by vcr-core.md + vcr-mock-integration.md |

### Files to CREATE

| File | Content Source |
|------|----------------|
| `vcr-core.md` | Phase 1 below |
| `vcr-mock-integration.md` | Phase 2 below |
| `vcr-fixtures-setup.md` | Phase 3 below |
| `vcr-makefile.md` | Phase 8 below |
| `vcr-precommit-hook.md` | Phase 9 below |

### Files to MODIFY

| File | Change |
|------|--------|
| `verify-mocks-providers.md` | Change fixture format from raw text to JSONL |
| `tests-anthropic-basic.md` | Change `Depends on:` from `tests-mock-infrastructure.md` to `vcr-mock-integration.md` |
| `tests-anthropic-streaming.md` | Change dependency |
| `tests-openai-basic.md` | Change dependency |
| `tests-openai-streaming.md` | Change dependency |
| `tests-google-basic.md` | Change dependency |
| `tests-google-streaming.md` | Change dependency |
| `tests-integration-flows.md` | Change dependency |
| `tests-integration-switching.md` | Change dependency |

### Dependency Chain (after changes)

```
credentials-core.md
       │
       v
vcr-core.md  ──────────────────────────┐
       │                               │
       v                               v
vcr-mock-integration.md          vcr-fixtures-setup.md
       │                               │
       └───────────┬───────────────────┘
                   │
                   v
       verify-mocks-providers.md (produces JSONL cassettes)
                   │
                   v
       tests-anthropic-basic.md, tests-openai-basic.md, tests-google-basic.md
                   │
                   v
       tests-*-streaming.md, tests-integration-*.md
                   │
                   v
       vcr-makefile.md, vcr-precommit-hook.md
```

---

## Phase 0: Clean Up Old Tasks

### Task 0.1: Remove Superseded Task File
**Files:** `scratch/tasks/tests-mock-infrastructure.md`
**Scope:** Delete file, update references

- [ ] Delete `scratch/tasks/tests-mock-infrastructure.md` (replaced by VCR Phase 1-2)
- [ ] Search for references to tests-mock-infrastructure.md in other task files
- [ ] Update any found references to point to vcr-tasks.md

### Task 0.2: Update verify-mocks-providers.md for JSONL
**Files:** `scratch/tasks/verify-mocks-providers.md`
**Scope:** Modify to output JSONL cassettes

- [ ] Change fixture format from raw text to JSONL
- [ ] Update capture_fixture() to write `_request`, `_response`, `_chunk` lines
- [ ] Update fixture file extensions from .txt/.json to .jsonl
- [ ] Update fixture paths in test scenarios

### Task 0.3: Update Provider Test Dependencies
**Files:** 3 task files
**Scope:** Update Depends on: lines

- [ ] Update `tests-anthropic-basic.md`: change `tests-mock-infrastructure.md` to `vcr-tasks.md Phase 2`
- [ ] Update `tests-openai-basic.md`: change dependency
- [ ] Update `tests-google-basic.md`: change dependency

---

## Phase 1: Core Infrastructure (CRITICAL PATH)

### Task 1.1: VCR Header and Macros
**Files:** `tests/helpers/vcr.h`
**Scope:** Header-only, ~60 lines

- [ ] Create `tests/helpers/vcr.h`
- [ ] Define `vcr_recording` global (extern bool)
- [ ] Define `vcr_ck_assert()` macro (no-op when recording)
- [ ] Define `vcr_ck_assert_int_eq()` macro
- [ ] Define `vcr_ck_assert_str_eq()` macro
- [ ] Define `vcr_ck_assert_ptr_nonnull()` macro
- [ ] Define `vcr_ck_assert_ptr_null()` macro
- [ ] Declare all public functions (init, finish, is_recording, next_chunk, has_more, skip_request_verification)

### Task 1.2: VCR Playback - Core Structure
**Files:** `tests/helpers/vcr.c`
**Scope:** ~80 lines

- [ ] Create `tests/helpers/vcr.c` with basic structure
- [ ] Implement `vcr_recording` global
- [ ] Implement mode detection from `VCR_RECORD` env var
- [ ] Implement fixture path resolution (`tests/fixtures/{provider}/{test_name}.jsonl`)
- [ ] Implement `vcr_init()` stub - open fixture file
- [ ] Implement `vcr_finish()` - close file
- [ ] Implement `vcr_is_recording()`

### Task 1.3: VCR Playback - JSONL Parser
**Files:** `tests/helpers/vcr.c`
**Scope:** ~100 lines
**Depends on:** 1.2

- [ ] Parse `_request` lines (store for verification)
- [ ] Parse `_response` lines (store status, headers)
- [ ] Parse `_chunk` lines (queue for delivery)
- [ ] Parse `_body` lines (treat as single chunk)
- [ ] Implement `vcr_next_chunk()` - return next chunk from queue
- [ ] Implement `vcr_has_more()` - check queue

### Task 1.4: VCR Request Verification
**Files:** `tests/helpers/vcr.c`
**Scope:** ~50 lines
**Depends on:** 1.3

- [ ] Store recorded request from `_request` line
- [ ] `vcr_verify_request(method, url, body)` - compare with recorded
- [ ] `vcr_skip_request_verification()` - set flag to skip
- [ ] Log mismatches as warnings (don't fail by default)

### Task 1.5: VCR Recording - Write Functions
**Files:** `tests/helpers/vcr.c`
**Scope:** ~80 lines
**Depends on:** 1.2

- [ ] Implement `vcr_record_request(method, url, headers, body)` - write `_request` line
- [ ] Implement `vcr_record_response(status, headers)` - write `_response` line
- [ ] Implement `vcr_record_chunk(data, len)` - write `_chunk` line
- [ ] Implement `vcr_record_body(data, len)` - write `_body` line
- [ ] All write functions output valid JSONL

### Task 1.6: Credential Redaction
**Files:** `tests/helpers/vcr.c`
**Scope:** ~40 lines
**Depends on:** 1.5

- [ ] Implement `vcr_redact_header(name, value)` function
- [ ] Handle `Authorization: Bearer X` -> `Authorization: Bearer REDACTED`
- [ ] Handle `x-api-key`, `x-goog-api-key`, `X-Subscription-Token`
- [ ] Case-insensitive header name matching
- [ ] Integrate redaction into `vcr_record_request()`

---

## Phase 2: Mock Integration

### Task 2.1: Curl Mock VCR Hooks
**Files:** `src/wrapper.c` (or new mock file)
**Scope:** ~60 lines
**Depends on:** 1.3

- [ ] Review existing curl mock infrastructure
- [ ] Add VCR hook in curl_multi_perform_ (or equivalent)
- [ ] In playback: deliver chunk from `vcr_next_chunk()`
- [ ] Set `running_handles` based on `vcr_has_more()`
- [ ] Return appropriate CURLMcode

### Task 2.2: Write Callback Integration
**Files:** `src/wrapper.c` or test helper
**Scope:** ~40 lines
**Depends on:** 2.1

- [ ] In playback: invoke write callback with VCR chunk data
- [ ] In record: capture write callback data via `vcr_record_chunk()`
- [ ] Handle HTTP status from VCR `_response` data

---

## Phase 3: Fixture Setup

### Task 3.1: Create Fixture Directories and Docs
**Files:** Multiple directories, README.md
**Scope:** ~50 lines of docs

- [ ] Create `tests/fixtures/anthropic/.gitkeep`
- [ ] Create `tests/fixtures/google/.gitkeep`
- [ ] Create `tests/fixtures/brave/.gitkeep`
- [ ] Create `tests/fixtures/README.md` with format documentation

---

## Phase 4: Convert OpenAI Fixtures

### Task 4.1: Convert stream_hello_world.txt
**Files:** `tests/fixtures/openai/stream_hello_world.jsonl`
**Scope:** Manual conversion of one fixture

- [ ] Read existing `stream_hello_world.txt`
- [ ] Create `_request` line (reconstruct from test usage)
- [ ] Create `_response` line (status 200, content-type: text/event-stream)
- [ ] Convert SSE content to `_chunk` lines
- [ ] Save as `stream_hello_world.jsonl`

### Task 4.2: Convert stream_done.txt
**Files:** `tests/fixtures/openai/stream_done.jsonl`
**Scope:** Manual conversion

- [ ] Convert `stream_done.txt` to JSONL format

### Task 4.3: Convert stream_multiline.txt
**Files:** `tests/fixtures/openai/stream_multiline.jsonl`
**Scope:** Manual conversion

- [ ] Convert `stream_multiline.txt` to JSONL format

### Task 4.4: Convert stream_with_usage.txt
**Files:** `tests/fixtures/openai/stream_with_usage.jsonl`
**Scope:** Manual conversion

- [ ] Convert `stream_with_usage.txt` to JSONL format

### Task 4.5: Convert Error Fixtures
**Files:** 3 error fixture files
**Scope:** Small, grouped together

- [ ] Convert `error_401_unauthorized.json` to JSONL
- [ ] Convert `error_429_rate_limit.json` to JSONL
- [ ] Convert `error_500_server.json` to JSONL

---

## Phase 5: Update OpenAI Tests (Group A - SSE Tests)

### Task 5.1: Update client_sse_test.c
**Files:** `tests/unit/openai/client_sse_test.c`
**Scope:** ~30 lines changed
**Depends on:** 1.3, 4.x

- [ ] Add `#include "vcr.h"`
- [ ] Add `vcr_init(__func__, "openai")` at test start
- [ ] Add `vcr_finish()` at test end
- [ ] Replace `ck_assert_*` with `vcr_ck_assert_*`

### Task 5.2: Update client_http_sse_streaming_test.c
**Files:** `tests/unit/openai/client_http_sse_streaming_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 5.3: Update client_http_sse_finish_test.c
**Files:** `tests/unit/openai/client_http_sse_finish_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

---

## Phase 6: Update OpenAI Tests (Group B - HTTP Tests)

### Task 6.1: Update client_http_mock_test.c
**Files:** `tests/unit/openai/client_http_mock_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 6.2: Update client_http_test.c
**Files:** `tests/unit/openai/client_http_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 6.3: Update http_handler_error_paths_test.c
**Files:** `tests/unit/openai/http_handler_error_paths_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 6.4: Update http_handler_tool_calls_test.c
**Files:** `tests/unit/openai/http_handler_tool_calls_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

---

## Phase 7: Update OpenAI Tests (Group C - Multi Tests)

### Task 7.1: Update client_multi_basic_test.c
**Files:** `tests/unit/openai/client_multi_basic_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 7.2: Update client_multi_write_callback_test.c
**Files:** `tests/unit/openai/client_multi_write_callback_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 7.3: Update client_multi_http_success_test.c
**Files:** `tests/unit/openai/client_multi_http_success_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 7.4: Update client_multi_callback_error_test.c
**Files:** `tests/unit/openai/client_multi_callback_error_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

### Task 7.5: Update client_multi_callback_error_with_http_errors_test.c
**Files:** `tests/unit/openai/client_multi_callback_error_with_http_errors_test.c`
**Scope:** ~30 lines changed

- [ ] Convert to VCR assertions
- [ ] Add vcr_init/vcr_finish

---

## Phase 8: Makefile and Build

### Task 8.1: Add VCR Build Rules
**Files:** `Makefile`
**Scope:** ~20 lines

- [ ] Add `$(BUILDDIR)/tests/helpers/vcr.o` compilation rule
- [ ] Add vcr.o to test object dependencies

### Task 8.2: Add VCR Record Targets
**Files:** `Makefile`
**Scope:** ~30 lines

- [ ] Add `vcr-record-openai` target
- [ ] Add `vcr-record-anthropic` target
- [ ] Add `vcr-record-google` target
- [ ] Add `vcr-record-all` target

---

## Phase 9: Pre-commit Hook

### Task 9.1: Credential Leak Detection Hook
**Files:** `.git/hooks/pre-commit`
**Scope:** ~30 lines bash

- [ ] Create or update `.git/hooks/pre-commit`
- [ ] Check for `Bearer [^R]` in fixtures
- [ ] Check for `sk-`, `sk-ant-`, `AIza`, `BSA` patterns
- [ ] Reject commit if patterns found

---

## Phase 10: Cleanup

### Task 10.1: Remove Old Fixtures
**Depends on:** All Phase 5-7 tests passing

- [ ] Remove `tests/fixtures/openai/stream_hello_world.txt`
- [ ] Remove `tests/fixtures/openai/stream_done.txt`
- [ ] Remove `tests/fixtures/openai/stream_multiline.txt`
- [ ] Remove `tests/fixtures/openai/stream_with_usage.txt`
- [ ] Remove old `.json` error fixtures

---

## Task Summary

| Phase | Tasks | Description | Priority |
|-------|-------|-------------|----------|
| 0 | 3 | Clean up old tasks | CRITICAL |
| 1 | 6 | Core VCR infrastructure | CRITICAL |
| 2 | 2 | Mock integration | CRITICAL |
| 3 | 1 | Fixture directories | CRITICAL |
| 4 | 5 | Convert existing OpenAI fixtures | OPTIONAL* |
| 5 | 3 | Update legacy SSE tests | OPTIONAL* |
| 6 | 4 | Update legacy HTTP tests | OPTIONAL* |
| 7 | 5 | Update legacy Multi tests | OPTIONAL* |
| 8 | 2 | Makefile updates | CRITICAL |
| 9 | 1 | Pre-commit hook | CRITICAL |
| 10 | 1 | Cleanup | OPTIONAL* |
| **Total** | **33** | | |

*OPTIONAL: Phase 4-7 and 10 migrate legacy `tests/unit/openai/` tests. These use the old OpenAI client (`src/openai/`), not the new provider architecture. They can be migrated later or left using existing fixtures.

---

## Dependencies

**Critical Path (must complete for new provider tests):**
```
0.1-0.3 (clean up old tasks)
    │
    v
1.1 (header) ──> 1.2 (core) ──> 1.3 (parser) ──> 1.4 (verify)
                     │
                     └──> 1.5 (record) ──> 1.6 (redact)
                              │
                              v
                         2.1 (curl mock) ──> 2.2 (callback)
                              │
                              v
                         3.1 (fixture dirs)
                              │
                              v
                         8.1-8.2 (makefile)
                              │
                              v
                         9.1 (pre-commit hook)
```

**Optional Path (legacy OpenAI test migration):**
```
Phase 1-3 complete
    │
    v
4.1-4.5 (convert fixtures) ──> 5.x, 6.x, 7.x (update tests) ──> 10.1 (cleanup)
```

---

## Execution Notes

- All tasks run serially (no parallel execution)
- Each task should result in one commit
- Run `make check` after each task to verify no regressions
- If a task fails, fix before proceeding to next task
