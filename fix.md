# Known Issues and Technical Debt

This document tracks known issues, technical debt, and improvement tasks.

## Active Issues

(No active issues)

---

## Resolved Issues

### Oversize Source File: `src/openai/client.c`

**Resolved:** 2025-11-22
**Original Size:** 18,995 bytes (+2,611 bytes over 16KB limit)
**Final Size:** 8,877 bytes (54% of limit)

**Problem:**
The `src/openai/client.c` file exceeded the 16KB file size limit due to growth from adding finish_reason extraction functionality and HTTP/SSE streaming logic.

**Solution:**
Implemented **Option B: Extract HTTP response handling to separate file**. This created a clean architectural separation between the high-level API layer and low-level transport layer.

**Changes:**
1. Created `src/openai/http_handler.c` (9,531 bytes) containing:
   - HTTP request/response handling via libcurl
   - SSE streaming and callback management
   - `extract_finish_reason()` function
   - Internal `http_write_callback()` and context structures

2. Refactored `src/openai/client.c` (8,877 bytes) to contain:
   - High-level API functions (messages, conversations, requests)
   - JSON serialization
   - Public API orchestration via `ik_openai_chat_create()`

3. Created `src/openai/http_handler.h` with internal API:
   - `ik_openai_http_post()` - HTTP transport abstraction
   - `ik_openai_http_response_t` - response structure

4. Updated Makefile to include new source file in build

**Results:**
- `src/openai/client.c`: Reduced by 10,118 bytes (53% reduction)
- Both files well under 16KB limit
- 100% test coverage maintained (lines, functions, branches)
- All tests passing
- Improved module cohesion and separation of concerns

---

### LCOV Exclusions in finish_reason Extraction

**Resolved:** 2025-11-22
**Location:** `src/openai/client.c` - `extract_finish_reason()` function
**Commit:** c39322c

**Problem:**
During Phase 1.8, the `extract_finish_reason()` function was added with 14 LCOV exclusion markers, pushing the project to 681/681 markers (at capacity). Analysis was needed to determine if these exclusions represented missing test coverage or genuinely untestable defensive code.

**Solution:**
After thorough analysis, LCOV exclusions were refined with explanatory comments:

1. **Unreachable defensive paths** (6 markers): Three error conditions (missing "data: " prefix, invalid JSON, non-object root) are unreachable because `ik_openai_parse_sse_event()` validates and returns ERR for these cases before `extract_finish_reason()` is called. These defensive checks remain with LCOV exclusions and explanatory comments documenting why they're unreachable.

2. **Compound condition sub-branches** (3 markers): Three compound OR conditions have certain branch combinations that are impractical to test due to short-circuit evaluation. Primary execution paths are fully tested.

3. **Invariants** (3 markers): Assert and OOM PANIC checks remain excluded as per standard policy.

**Results:**
- LCOV markers in `extract_finish_reason()`: 12 (down from 14)
- Coverage maintained at 100% (lines, functions, branches)
- All defensive checks documented with inline comments
- Total LCOV markers: 681/681 (still at capacity)

---

## Notes

### File Size Limit Rationale

The 16KB file size limit is enforced to:
- Encourage modular, focused code
- Improve maintainability and readability
- Prevent "god objects" that accumulate responsibilities
- Make code reviews more manageable

However, the limit is a guideline, not an absolute requirement. Exceeding by 10-15% during active development is acceptable if:
- The excess is temporary (will be refactored)
- The file has strong cohesion (single responsibility)
- Refactoring is tracked as technical debt

### LCOV Exclusion Policy

LCOV exclusions should be used sparingly and only for:
- Defensive assertions that cannot fail in practice
- Error handling for system-level failures (OOM, etc.)
- Platform-specific code paths that cannot be tested in CI

Each exclusion should have a clear comment explaining why coverage is impractical.

Current count: **681 markers** (at limit of 681)

---

---

### Oversize Test File: `tests/unit/openai/client_http_sse_test.c`

**Resolved:** 2025-11-22
**Original Size:** 21,366 bytes (+4,982 bytes over 16KB limit)
**Final Sizes:**
- `client_http_sse_streaming_test.c`: 15,360 bytes (94% of limit)
- `client_http_sse_finish_test.c`: 12,288 bytes (75% of limit)

**Problem:**
The `tests/unit/openai/client_http_sse_test.c` file exceeded the 16KB file size limit due to growth from adding finish_reason extraction functionality tests in Phase 1.8.

**Solution:**
Implemented **Option A: Split by feature area**. This created a logical separation between general SSE streaming tests and finish_reason-specific tests.

**Changes:**
1. Created `tests/unit/openai/client_http_sse_streaming_test.c` (15,360 bytes) containing:
   - Mock curl infrastructure for all tests
   - 6 general SSE streaming tests:
     - `test_http_callback_with_sse_streaming`
     - `test_http_callback_empty_response`
     - `test_http_callback_sse_parser_feed_error`
     - `test_http_callback_sse_parse_error`
     - `test_http_callback_user_success`
     - `test_http_callback_user_error`

2. Created `tests/unit/openai/client_http_sse_finish_test.c` (12,288 bytes) containing:
   - Mock curl infrastructure for all tests
   - 4 finish_reason extraction tests:
     - `test_http_callback_with_finish_reason`
     - `test_http_callback_without_finish_reason`
     - `test_http_callback_malformed_finish_reason`
     - `test_http_callback_finish_reason_edge_cases`

3. Removed original `tests/unit/openai/client_http_sse_test.c`

4. Build system automatically detected new test files (wildcard-based discovery)

**Results:**
- Both files well under 16KB limit
- All 10 tests passing (100% success rate)
- 100% test coverage maintained
- Clear logical separation makes future maintenance easier
- Each file has a focused purpose aligned with feature areas

---

**Last Updated:** 2025-11-22
**Next Review:** Before Phase 2.0 (Database Integration)
