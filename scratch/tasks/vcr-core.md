# Task: VCR Core Infrastructure

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Design Constraints

VCR (Video Cassette Recorder) provides HTTP recording/replay for deterministic tests:

- **Layer:** Operates at HTTP layer, not application layer
- **Format:** JSONL (JSON Lines) - one JSON object per line
- **Modes:** Record mode (VCR_RECORD=1) vs playback mode (default)
- **Assertions:** vcr_ck_assert_* macros are no-ops when recording
- **Credentials:** API keys MUST be redacted before writing to fixtures
- **Reusable:** Works with any HTTP API (LLM providers, search APIs, etc.)

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `src/credentials.h` exists (from credentials-core.md)

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns (ownership, destructors)
- `/load errors` - Result types (OK/ERR patterns)

**Design Documentation:**
- `scratch/plan/vcr-cassettes.md` - Complete VCR design specification
- `scratch/vcr-tasks.md` - Phase 1 tasks (1.1-1.6)

**Source (for credential redaction integration):**
- `src/credentials.h` - Credential types

## Objective

Create `tests/helpers/vcr.h` and `tests/helpers/vcr.c` - a VCR-style HTTP recording/replay system that enables deterministic tests without real API calls.

This task implements all Phase 1 core infrastructure:
1. VCR header and macros
2. Core structure with mode detection
3. JSONL parser for playback
4. Request verification
5. Recording write functions
6. Credential redaction

## JSONL Format Specification

Each fixture file contains one JSON object per line.

### Line Types

| Line Type | Purpose | Example |
|-----------|---------|---------|
| `_request` | HTTP request metadata | `{"_request": {"method": "POST", "url": "...", "headers": {...}, "body": "..."}}` |
| `_response` | HTTP response metadata | `{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}` |
| `_body` | Complete response body (non-streaming) | `{"_body": "{\"id\":\"msg_123\",\"content\":[...]}"}` |
| `_chunk` | Raw chunk as delivered by curl (streaming) | `{"_chunk": "event: message_start\ndata: {...}\n\n"}` |

### Structure

Single exchange:
```
Line 1: _request (always first)
Line 2: _response (status + headers)
Line 3+: _body (non-streaming) OR _chunk lines (streaming)
```

Multiple exchanges (multi-turn tests):
```
Line 1: _request (first request)
Line 2: _response
Line 3+: _body or _chunk lines
Line N: _request (second request - starts new exchange)
Line N+1: _response
Line N+2+: _body or _chunk lines
```

### Chunk Granularity

Each `_chunk` line contains exactly what curl's write callback received - one callback invocation = one `_chunk` line. No buffering or normalization.

### Example: Streaming (Anthropic SSE)

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json"}, "body": "{\"model\":\"claude-sonnet-4-20250514\",\"stream\":true,\"messages\":[...]}"}}
{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}
{"_chunk": "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_123\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"delta\":{\"text\":\"Hello\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"delta\":{\"text\":\" world\"}}\n\n"}
{"_chunk": "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n"}
```

### Example: Non-Streaming (Google Search)

```jsonl
{"_request": {"method": "GET", "url": "https://api.search.brave.com/res/v1/web/search?q=test", "headers": {"X-Subscription-Token": "REDACTED"}}}
{"_response": {"status": 200, "headers": {"content-type": "application/json"}}}
{"_body": "{\"web\":{\"results\":[{\"title\":\"Example\",\"url\":\"https://example.com\"}]}}"}
```

### Example: Error Response

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED"}, "body": "{...}"}}
{"_response": {"status": 429, "headers": {"content-type": "application/json", "retry-after": "30"}}}
{"_body": "{\"error\":{\"type\":\"rate_limit_error\",\"message\":\"Rate limit exceeded\"}}"}
```

## Interface

### Structs to Define (Internal)

| Struct | Members | Purpose |
|--------|---------|---------|
| `vcr_state_t` | FILE *fp, bool recording, char *fixture_path, request_data_t *recorded_request, chunk_queue_t *chunk_queue, bool skip_verification | VCR runtime state |
| `request_data_t` | char *method, char *url, char *headers, char *body | Stored request for verification |
| `chunk_queue_t` | char **chunks, size_t count, size_t index | Queue of chunks to deliver |

### Macros to Define (Public - tests/helpers/vcr.h)

| Macro | Purpose |
|-------|---------|
| `vcr_ck_assert(expr)` | Assert that's a no-op when recording |
| `vcr_ck_assert_int_eq(a, b)` | Integer equality check (no-op when recording) |
| `vcr_ck_assert_str_eq(a, b)` | String equality check (no-op when recording) |
| `vcr_ck_assert_ptr_nonnull(ptr)` | Pointer non-null check (no-op when recording) |
| `vcr_ck_assert_ptr_null(ptr)` | Pointer null check (no-op when recording) |

### Functions to Implement (Public)

| Function | Signature | Purpose |
|----------|-----------|---------|
| `vcr_init` | `void (const char *test_name, const char *provider)` | Initialize VCR for test, open fixture file |
| `vcr_finish` | `void (void)` | Cleanup VCR, close fixture file |
| `vcr_is_active` | `bool (void)` | Check if VCR was initialized for this test |
| `vcr_is_recording` | `bool (void)` | Check if in record mode |
| `vcr_skip_request_verification` | `void (void)` | Disable request verification for this test |
| `vcr_next_chunk` | `bool (const char **data_out, size_t *len_out)` | Get next chunk from queue (playback mode), returns false if no more |
| `vcr_has_more` | `bool (void)` | Check if more chunks available |
| `vcr_record_request` | `void (const char *method, const char *url, const char *headers, const char *body)` | Record HTTP request (record mode) |
| `vcr_record_response` | `void (int status, const char *headers)` | Record HTTP response metadata (record mode) |
| `vcr_record_chunk` | `void (const char *data, size_t len)` | Record streaming chunk (record mode) |
| `vcr_record_body` | `void (const char *data, size_t len)` | Record complete body (non-streaming, record mode) |
| `vcr_verify_request` | `void (const char *method, const char *url, const char *body)` | Verify request matches recorded (playback mode) |

## Chunk Format

**Critical: Chunks are null-terminated C strings.**

All chunks returned by `vcr_next_chunk()` are guaranteed to be null-terminated:
- **Null terminator guarantee**: Every chunk `data_out` is a valid null-terminated C string
- **Safe for string functions**: Can be used with `strlen()`, `strcpy()`, `strstr()`, etc.
- **Length parameter**: `len_out` provides the string length for convenience/optimization to avoid redundant `strlen()` calls
- **Consistency**: `strlen(*data_out)` always equals `*len_out`

This design provides flexibility:
- Use `*data_out` directly with string functions when convenient
- Use `*len_out` to avoid redundant strlen() calls in performance-sensitive code
- Use both for validation/debugging (assert `strlen(*data_out) == *len_out`)

Implementation note: When recording chunks via `vcr_record_chunk(data, len)`, VCR internally stores them as null-terminated strings. The `len` parameter indicates the content length, but storage includes the null terminator.

## Behaviors

### VCR Active State (vcr_is_active)

- Returns `true` when VCR has been initialized via `vcr_init()`
- Returns `false` when VCR has not been initialized or after `vcr_finish()` has been called
- Implementation: Check if `g_vcr_state != NULL`
- Purpose: Allows code to check if VCR is available without checking recording mode specifically
- Used by curl mocks to determine whether to use VCR playback or real curl

### Mode Detection (vcr_init)

- Check `VCR_RECORD` environment variable
- If set to "1": recording mode (write fixture)
- If unset or other value: playback mode (read fixture)
- Set global `vcr_recording` flag accordingly

### Fixture Path Resolution (vcr_init)

- Pattern: `tests/fixtures/{provider}/{test_name}.jsonl`
- Example: `tests/fixtures/anthropic/test_anthropic_streaming_basic.jsonl`
- In record mode: open for writing (`fopen(..., "w")`)
- In playback mode: open for reading (`fopen(..., "r")`)
- If file doesn't exist in playback mode: log error, allow test to continue

### JSONL Parser (vcr_init - playback mode)

Parse all lines at initialization:

1. Read line by line with `fgets()` or `getline()`
2. Parse JSON with a JSON library (yajl, cJSON, or manual parsing)
3. Identify line type by checking for `_request`, `_response`, `_body`, `_chunk` keys
4. For `_request`: Store in `recorded_request` for verification
5. For `_response`: Store status and headers (for future use)
6. For `_body`: Add to chunk_queue as single chunk
7. For `_chunk`: Append to chunk_queue
8. Continue until EOF

### Chunk Queue Management (playback mode)

- `vcr_next_chunk()`: Return next chunk from queue, increment index
- `vcr_has_more()`: Check if `index < count`
- Chunks owned by VCR state, freed at vcr_finish()

### Request Verification (vcr_verify_request - playback mode)

- Compare provided method/url/body against `recorded_request`
- If `skip_verification` flag set: return immediately
- If mismatch: log warning via fprintf(stderr, ...), don't fail test
- This catches API drift without breaking playback

### Record Functions (record mode only)

All record functions write valid JSONL to fixture file:

**vcr_record_request:**
- Build JSON object: `{"_request": {"method": "...", "url": "...", "headers": {...}, "body": "..."}}`
- Apply credential redaction to headers
- Write line to file with `fprintf()` ending in `\n`
- Flush buffer with `fflush()`

**vcr_record_response:**
- Build JSON object: `{"_response": {"status": 200, "headers": {...}}}`
- Write line to file
- Flush buffer

**vcr_record_chunk:**
- Build JSON object: `{"_chunk": "..."}`
- Escape data for JSON string (handle `\n`, `\r`, `"`, `\`, etc.)
- Write line to file
- Flush buffer

**vcr_record_body:**
- Build JSON object: `{"_body": "..."}`
- Escape data for JSON string
- Write line to file
- Flush buffer

### Credential Redaction (vcr_record_request)

Before writing `_request` line, redact authentication headers:

| Provider | Header | Redaction Rule |
|----------|--------|----------------|
| OpenAI | `Authorization` | `Bearer sk-xxx` → `Bearer REDACTED` |
| Anthropic | `x-api-key` | `sk-ant-xxx` → `REDACTED` |
| Google | `x-goog-api-key` | `AIza...` → `REDACTED` |
| Brave | `X-Subscription-Token` | `BSA...` → `REDACTED` |

Implementation:
```c
// Case-insensitive header matching
if (strcasecmp(name, "authorization") == 0) {
    if (strncasecmp(value, "Bearer ", 7) == 0) {
        return "Bearer REDACTED";
    }
    return "REDACTED";
}
if (strcasecmp(name, "x-api-key") == 0 ||
    strcasecmp(name, "x-goog-api-key") == 0 ||
    strcasecmp(name, "x-subscription-token") == 0) {
    return "REDACTED";
}
return value;  // No redaction
```

### Assertion Macros (vcr.h)

Define global:
```c
extern bool vcr_recording;
```

Define macros that check `vcr_recording` before calling ck_assert:
```c
#define vcr_ck_assert(expr) \
    do { if (!vcr_recording) ck_assert(expr); } while(0)

#define vcr_ck_assert_int_eq(a, b) \
    do { if (!vcr_recording) ck_assert_int_eq(a, b); } while(0)

#define vcr_ck_assert_str_eq(a, b) \
    do { if (!vcr_recording) ck_assert_str_eq(a, b); } while(0)

#define vcr_ck_assert_ptr_nonnull(ptr) \
    do { if (!vcr_recording) ck_assert_ptr_nonnull(ptr); } while(0)

#define vcr_ck_assert_ptr_null(ptr) \
    do { if (!vcr_recording) ck_assert_ptr_null(ptr); } while(0)
```

**Rationale:** In record mode, tests must run to make HTTP calls, but assertions should not fail on dynamic data.

### Memory Management

- Use malloc/free for VCR state (talloc not required for test helpers)
- Store chunks as dynamically allocated strings
- Free all memory in vcr_finish()
- Close file handle in vcr_finish()

### Error Handling

- Missing fixture file in playback: log warning, continue (allows test to fail gracefully)
- JSON parse errors: log warning, skip malformed line
- File write errors: fprintf to stderr, continue
- No crashes on invalid input

## Directory Structure

```
tests/helpers/
    vcr.h
    vcr.c

tests/fixtures/
    anthropic/.gitkeep
    google/.gitkeep
    brave/.gitkeep
    README.md
```

## Test Scenarios

Create `tests/unit/helpers/vcr_test.c`:

**Mode detection:**
- VCR_RECORD=1: vcr_is_recording() returns true
- VCR_RECORD unset: vcr_is_recording() returns false

**Lifecycle:**
- vcr_init + vcr_finish: no crash
- Multiple init/finish cycles: no memory leak
- vcr_is_active() returns false before vcr_init()
- vcr_is_active() returns true after vcr_init()
- vcr_is_active() returns false after vcr_finish()

**Playback mode:**
- Load fixture with single chunk: vcr_next_chunk(&data, &len) returns true with data, vcr_has_more() false after
- Load fixture with multiple chunks: iterate with vcr_next_chunk(), returns true until exhausted
- Load fixture with _body: treated as single chunk
- vcr_next_chunk() returns false when no more chunks
- Missing fixture file: vcr_init() doesn't crash

**Record mode:**
- vcr_record_request: writes valid JSONL _request line
- vcr_record_response: writes valid JSONL _response line
- vcr_record_chunk: writes valid JSONL _chunk line
- vcr_record_body: writes valid JSONL _body line
- Multiple chunks recorded in sequence

**Credential redaction:**
- Authorization header with Bearer: replaced with "Bearer REDACTED"
- x-api-key header: replaced with "REDACTED"
- x-goog-api-key header: replaced with "REDACTED"
- X-Subscription-Token header: replaced with "REDACTED"
- Case-insensitive matching works
- Other headers unchanged

**Assertion macros:**
- vcr_ck_assert in playback mode: calls ck_assert
- vcr_ck_assert in record mode: no-op (test passes)

**Request verification:**
- Matching request: no warning
- Mismatched method: warning logged
- Mismatched URL: warning logged
- vcr_skip_request_verification: suppresses warnings

## Fixtures Directory Setup

Create fixture directories with README:

**tests/fixtures/README.md:**
```markdown
# VCR Test Fixtures

JSONL format HTTP recordings for deterministic tests.

## Format

Each fixture file contains one JSON object per line:
- `_request`: HTTP request metadata
- `_response`: HTTP response metadata
- `_body`: Complete response body (non-streaming)
- `_chunk`: Raw streaming chunk (one per curl callback)

## Recording

To re-record fixtures:
```bash
VCR_RECORD=1 make check
```

## Security

API keys are redacted automatically. Verify before committing:
```bash
grep -rE "Bearer [^R]" tests/fixtures/   # Should return nothing
grep -r "sk-" tests/fixtures/            # Should return nothing
grep -r "sk-ant-" tests/fixtures/        # Should return nothing
```

## Provider Directories

- `anthropic/` - Anthropic API fixtures
- `google/` - Google Gemini API fixtures
- `brave/` - Brave Search API fixtures
- `openai/` - OpenAI API fixtures (legacy and new)
```

## Makefile Updates

Add to Makefile:
- `tests/helpers/vcr.c` to TEST_HELPER_SOURCES or similar variable
- Ensure vcr.o is linked with test binaries
- If needed, create HELPER_OBJECTS variable

## Postconditions

- [ ] `tests/helpers/vcr.h` exists with all public functions and macros
- [ ] `tests/helpers/vcr.c` implements all VCR functions
- [ ] Directory `tests/helpers/` exists
- [ ] Directory `tests/fixtures/anthropic/` exists with .gitkeep
- [ ] Directory `tests/fixtures/google/` exists with .gitkeep
- [ ] Directory `tests/fixtures/brave/` exists with .gitkeep
- [ ] File `tests/fixtures/README.md` exists with format documentation
- [ ] Unit tests exist in `tests/unit/helpers/vcr_test.c`
- [ ] All vcr_ck_assert_* macros defined
- [ ] vcr_recording global declared extern in header
- [ ] Record mode vs playback mode switching via VCR_RECORD env var
- [ ] Credential redaction implemented for all known auth headers
- [ ] JSONL parser handles _request, _response, _body, _chunk lines
- [ ] Request verification implemented with opt-out
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Compiles without warnings
- [ ] Changes committed to git with message: `task: vcr-core.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).

## Implementation Notes

### JSON Handling

You may choose any of these approaches:
1. Use existing JSON library if available in the project
2. Use lightweight cJSON library (single header)
3. Implement minimal JSONL parser (just enough to extract _chunk strings)

### Minimal JSONL Parser Approach

Since VCR only needs to:
- Identify line type (`_request`, `_response`, `_body`, `_chunk`)
- Extract the string value from `_chunk` or `_body`

A minimal parser is acceptable:
```c
// Find key in JSONL: {"_chunk": "value"}
const char *extract_jsonl_value(const char *line, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "{\"%s\": \"", key);
    const char *start = strstr(line, search);
    if (!start) return NULL;
    start += strlen(search);
    // Find end quote (handle escaped quotes)
    // ... implementation ...
}
```

### File I/O

- Use standard C file functions: fopen, fprintf, fgets, fclose
- Always flush after writing in record mode: fflush(fp)
- Handle EOF gracefully in playback mode

### Global State

VCR uses global state (acceptable for test infrastructure):
```c
static vcr_state_t *g_vcr_state = NULL;
bool vcr_recording = false;  // Extern in header
```

### Integration Points

This task creates standalone VCR infrastructure. Future tasks will:
- Integrate with curl mock (Phase 2)
- Update existing tests to use vcr_ck_assert_* (Phase 5-7)
- Add Makefile targets for VCR_RECORD (Phase 8)
