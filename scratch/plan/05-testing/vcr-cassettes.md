# VCR Cassette Design

## Status: DESIGN COMPLETE

## Overview

VCR-style test fixtures that record real HTTP responses and replay them in tests. This enables:
- Deterministic tests without real API calls
- Fast test execution (no network latency)
- CI/CD without API credentials
- Debugging with real response data

**Design principle:** VCR operates at the HTTP layer, not the application layer. It records raw HTTP requests and responses without interpreting content. This makes it reusable across any HTTP API:
- LLM providers (Anthropic, Google, OpenAI)
- Search APIs (Brave, Google Search)
- Any future HTTP-based service

## File Format: JSONL

Each fixture file is JSONL (JSON Lines) format. One JSON object per line.

### Line Types

| Line Type | Purpose |
|-----------|---------|
| `_request` | HTTP request (method, url, headers, body) |
| `_response` | HTTP response metadata (status, headers) |
| `_body` | Complete response body (non-streaming) |
| `_chunk` | Raw chunk as delivered to write callback (streaming) |

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
...
```

Each `_request` line starts a new exchange. VCR replays them in order.

### Chunk Granularity

Each `_chunk` line contains exactly what curl's write callback received - one callback invocation = one `_chunk` line. No buffering or normalization.

### Non-Streaming Example (LLM)

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json"}, "body": "{\"model\":\"claude-sonnet-4-5-20250929\",\"messages\":[...]}"}}
{"_response": {"status": 200, "headers": {"content-type": "application/json"}}}
{"_body": "{\"id\":\"msg_123\",\"type\":\"message\",\"content\":[{\"type\":\"text\",\"text\":\"Hello world\"}]}"}
```

### Non-Streaming Example (Search API)

```jsonl
{"_request": {"method": "GET", "url": "https://api.search.brave.com/res/v1/web/search?q=test", "headers": {"X-Subscription-Token": "REDACTED"}}}
{"_response": {"status": 200, "headers": {"content-type": "application/json"}}}
{"_body": "{\"web\":{\"results\":[{\"title\":\"Example\",\"url\":\"https://example.com\"}]}}"}
```

### Streaming Example (Anthropic SSE)

Raw SSE chunks exactly as delivered by the API:

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json"}, "body": "{\"model\":\"claude-sonnet-4-5-20250929\",\"stream\":true,\"messages\":[...]}"}}
{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}
{"_chunk": "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_123\"}}\n\n"}
{"_chunk": "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"delta\":{\"text\":\"Hello\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"delta\":{\"text\":\" world\"}}\n\n"}
{"_chunk": "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"}
{"_chunk": "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n"}
```

### Streaming Example (Google NDJSON)

Google uses newline-delimited JSON, not SSE:

```jsonl
{"_request": {"method": "POST", "url": "https://generativelanguage.googleapis.com/v1/models/gemini-3.0-flash:streamGenerateContent", "headers": {"x-goog-api-key": "REDACTED"}, "body": "{...}"}}
{"_response": {"status": 200, "headers": {"content-type": "application/x-ndjson"}}}
{"_chunk": "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}\n"}
{"_chunk": "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\" world\"}]}}]}\n"}
```

### Streaming Example (OpenAI SSE)

```jsonl
{"_request": {"method": "POST", "url": "https://api.openai.com/v1/responses", "headers": {"Authorization": "Bearer REDACTED"}, "body": "{\"model\":\"gpt-5-mini\",\"stream\":true,\"input\":\"Hello\"}"}}
{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}
{"_chunk": "event: response.created\ndata: {\"type\":\"response.created\",\"response\":{\"id\":\"resp_123\",\"status\":\"in_progress\"}}\n\n"}
{"_chunk": "event: response.output_text.delta\ndata: {\"type\":\"response.output_text.delta\",\"delta\":\"Hello\"}\n\n"}
{"_chunk": "event: response.output_text.delta\ndata: {\"type\":\"response.output_text.delta\",\"delta\":\" world\"}\n\n"}
{"_chunk": "event: response.completed\ndata: {\"type\":\"response.completed\",\"response\":{\"id\":\"resp_123\",\"status\":\"completed\"}}\n\n"}
```

### Error Response Example

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED"}, "body": "{...}"}}
{"_response": {"status": 429, "headers": {"content-type": "application/json", "retry-after": "30"}}}
{"_body": "{\"error\":{\"type\":\"rate_limit_error\",\"message\":\"Rate limit exceeded\"}}"}
```

## Credential Redaction

**Critical:** API keys must NEVER be written to fixture files.

### Provider Header Formats

Each provider uses their native authentication format:

| Provider | Header | Example | Redacted |
|----------|--------|---------|----------|
| OpenAI | `Authorization` | `Bearer sk-xxx` | `Bearer REDACTED` |
| Anthropic | `x-api-key` | `sk-ant-xxx` | `REDACTED` |
| Google | `x-goog-api-key` | `AIza...` | `REDACTED` |
| Brave | `X-Subscription-Token` | `BSA...` | `REDACTED` |

### Redaction Rules

1. **Case-insensitive header matching**
2. **Preserve Bearer prefix** for Authorization headers
3. **Replace entire value** for other auth headers

```c
// During capture, before writing _request line:
const char *redact_header(const char *name, const char *value)
{
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
    return value;
}
```

### Validation

Before committing fixtures, verify no secrets leaked:

```bash
# Should return no matches
grep -rE "Bearer [^R]" tests/fixtures/   # Bearer tokens not redacted
grep -r "sk-" tests/fixtures/            # OpenAI key patterns
grep -r "sk-ant-" tests/fixtures/        # Anthropic key patterns
grep -r "AIza" tests/fixtures/           # Google key patterns
grep -r "BSA" tests/fixtures/            # Brave key patterns
```

Consider adding a pre-commit hook to enforce this.

## Directory Structure

**Note:** Old fixtures in `tests/fixtures/openai/`, `tests/fixtures/anthropic/`, and `tests/fixtures/google/` are DELETED, not migrated. This release uses a clean slate VCR-based fixture format.

```
tests/fixtures/vcr/
├── anthropic/
│   ├── test_anthropic_streaming_basic.jsonl
│   ├── test_anthropic_streaming_tool_call.jsonl
│   ├── test_anthropic_thinking_high.jsonl
│   └── test_anthropic_error_rate_limit.jsonl
├── google/
│   ├── test_google_streaming_basic.jsonl
│   └── test_google_thinking_medium.jsonl
├── openai/
│   ├── test_openai_chat_basic.jsonl
│   ├── test_openai_streaming_basic.jsonl
│   └── test_openai_responses_api.jsonl
└── brave/
    └── test_brave_web_search_basic.jsonl
```

**Naming convention:** Fixture filename matches test function name.

## Modes of Operation

### Playback Mode (Default)

```bash
make check   # All tests use fixtures, no network
```

- Mock intercepts HTTP calls
- Loads fixture matching test name
- Replays recorded responses
- Test assertions run against replayed data

### Capture Mode

```bash
VCR_RECORD=1 make check                           # Re-record all fixtures
VCR_RECORD=1 ./tests/unit/providers/anthropic     # Re-record anthropic tests only
```

- Test body runs fully (same code path as playback)
- Real HTTP calls made to APIs
- Request and response captured to JSONL
- Written to fixture file (overwrites existing)
- Assertions are no-ops (via `vcr_ck_assert_*` macros)
- Test always "passes" in record mode

**Credentials:** Capture mode requires real API keys via environment:
- `ANTHROPIC_API_KEY`
- `GOOGLE_API_KEY`
- `OPENAI_API_KEY`

**Redaction:** API keys in captured requests are replaced with `REDACTED`.

## Architecture

### Layered Design

VCR operates at the HTTP layer, below provider-specific logic:

```
┌─────────────────────────────────────────────────────────┐
│ Test                                                    │
│   test_anthropic_streaming_basic()                     │
│   test_brave_web_search()                              │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│ Provider Layer (anthropic.c, google.c, brave_search.c) │
│   - Build requests                                      │
│   - Parse responses (SSE, NDJSON, JSON)                │
│   - Provider-specific logic                            │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│ HTTP Layer (http_multi.c)                                │
│   - curl_multi management                               │
│   - Write callbacks deliver raw bytes                  │
│   - VCR hooks at this layer                            │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│ VCR Layer (vcr.c)                                       │
│   - Record: intercept curl, write JSONL                │
│   - Playback: read JSONL, feed to write callback       │
│   - Knows nothing about SSE, JSON, providers           │
│   - Generic HTTP recording/replay                      │
└─────────────────────────────────────────────────────────┘
```

**Key insight:** VCR doesn't parse content-type, SSE events, or JSON. It just records/replays raw HTTP bytes. This makes it reusable for any HTTP API.

### Decision: Separate vcr.c with Runtime Mode Switch

VCR logic lives in a separate `vcr.c` module that switches mode based on `VCR_RECORD` env var:

```
┌─────────────────────────────────────────────────────────┐
│ Test                                                    │
│   vcr_begin(__func__, "anthropic")                     │
│   provider->start_stream(...)                          │
│   vcr_end()                                            │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│ vcr.c                                                   │
│                                                         │
│   vcr_begin():                                         │
│     if (VCR_RECORD) {                                  │
│         open fixture file for writing                  │
│         use real curl, capture HTTP to JSONL           │
│     } else {                                            │
│         load fixture file                              │
│         override curl_multi_* with replay stubs        │
│     }                                                   │
└─────────────────────────────────────────────────────────┘
```

**Rationale:**
- VCR is a distinct concern from mocking - separation makes each easier to understand
- `VCR_RECORD=1` env var is simple and familiar
- Avoids build complexity of maintaining two test binaries

### Decision: Assertions Use vcr_ck_assert_* Macros

In capture mode, tests run fully (to exercise HTTP paths) but assertions are no-ops.

```c
// tests/helpers/vcr.h

extern bool vcr_recording;

#define vcr_ck_assert(expr) \
    do { if (!vcr_recording) ck_assert(expr); } while(0)

#define vcr_ck_assert_int_eq(a, b) \
    do { if (!vcr_recording) ck_assert_int_eq(a, b); } while(0)

#define vcr_ck_assert_str_eq(a, b) \
    do { if (!vcr_recording) ck_assert_str_eq(a, b); } while(0)

#define vcr_ck_assert_ptr_nonnull(ptr) \
    do { if (!vcr_recording) ck_assert_ptr_nonnull(ptr); } while(0)

// ... etc for other ck_assert variants
```

**Rationale:**
- Tests must run to make the HTTP calls that get recorded
- Using same test code for record and playback ensures no divergence
- Mechanical update to convert existing `ck_assert_*` to `vcr_ck_assert_*`

### Decision: Request Verification with Opt-Out

In playback mode, VCR verifies the test sends the same request that was recorded. This catches API drift.

```c
// Skip verification for tests where request legitimately varies
vcr_skip_request_verification();
```

### Decision: Timestamps and IDs Recorded As-Is

API responses contain dynamic values (timestamps, message IDs). These are recorded without normalization. Tests should not assert on these fields.

If this becomes problematic, we'll revisit with normalization or field-ignore helpers.

## Playback Implementation

### Loading Fixtures

```c
// tests/helpers/vcr.h

// Initialize VCR for a test - call at test start
void vcr_init(const char *test_name, const char *provider);

// In capture mode: record this request
// In playback mode: verify request matches (optional)
void vcr_record_request(const char *method, const char *url,
                        const char *headers, const char *body);

// In capture mode: record this response line
// In playback mode: return next line from fixture
const char *vcr_next_line(void);

// Check if more lines available (streaming)
bool vcr_has_more(void);

// Cleanup
void vcr_finish(void);
```

### Mock Integration

```c
// In curl_multi_perform_ override:
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    if (vcr_is_recording()) {
        // Real curl call, capture response
        return real_curl_multi_perform(multi, running_handles);
    }

    // Playback: deliver next chunk from fixture
    const char *line = vcr_next_line();
    if (line) {
        deliver_to_write_callback(parse_jsonl_line(line));
        *running_handles = vcr_has_more() ? 1 : 0;
    }
    return CURLM_OK;
}
```

## Clean Slate: No Migration from Old Fixtures

**This release does NOT migrate old fixtures.** All old fixtures are deleted:
- `tests/fixtures/openai/` - DELETED
- `tests/fixtures/anthropic/` - DELETED (if existed)
- `tests/fixtures/google/` - DELETED (if existed)

New VCR fixtures are created from scratch using `VCR_RECORD=1`:
1. Delete old `tests/fixtures/` directory
2. Create new `tests/fixtures/vcr/` directory structure
3. Record new fixtures with VCR capture mode
4. All fixtures follow JSONL format specification

## Design Decisions Summary

| Decision | Choice |
|----------|--------|
| Architecture | Separate `vcr.c` with runtime mode switch |
| Capture mode assertions | `vcr_ck_assert_*` macros (no-ops when recording) |
| Multi-request tests | Multiple exchanges in one fixture file |
| Chunk granularity | Raw curl delivery (one callback = one `_chunk`) |
| Request verification | Verify by default, `vcr_skip_request_verification()` to opt out |
| Timestamps/IDs | Record as-is, tests don't assert on dynamic fields |
| Fixture naming | Match test function name |
| Credential redaction | Provider-native headers, deterministic redaction |

## Next Steps

1. Create `tests/helpers/vcr.h` and `tests/helpers/vcr.c`
2. Implement record mode (capture HTTP to JSONL)
3. Implement playback mode (replay JSONL through mock)
4. Define `vcr_ck_assert_*` macros
5. Create fixture directories for each provider
6. Record initial fixtures with `VCR_RECORD=1`
7. Update existing tests to use `vcr_ck_assert_*`
8. Add pre-commit hook for credential leak detection
