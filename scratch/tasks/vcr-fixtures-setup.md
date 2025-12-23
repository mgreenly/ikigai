# Task: Create VCR Fixture Directories and Documentation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/none
**Depends on:** vcr-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

### Clean Slate Approach

**This is a REPLACEMENT, not a migration:**

- Old OpenAI code in `src/openai/` will be DELETED
- Old OpenAI tests will be DELETED
- Old fixtures in `tests/fixtures/openai/` will be DELETED (not migrated)
- Only new provider abstraction code and VCR-based tests remain
- VCR fixtures are the ONLY fixture format going forward

The approach is: record fresh VCR fixtures from real API calls, use those for all new tests, delete everything old.

**Cleanup:** The old `tests/fixtures/openai/` directory is removed as part of cleanup-openai-* tasks.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] VCR core infrastructure exists (`tests/helpers/vcr.h` and `tests/helpers/vcr.c`)

## Objective

Create the fixture directory structure for VCR cassettes and document the JSONL format. This establishes the locations where test fixtures will be stored and provides comprehensive documentation for developers creating or maintaining fixtures.

## Directory Structure to Create

```
tests/fixtures/
└── vcr/                              # Root for ALL VCR fixtures
    ├── openai/.gitkeep               # OpenAI provider fixtures
    ├── anthropic/.gitkeep            # Anthropic provider fixtures
    ├── google/.gitkeep               # Google provider fixtures
    ├── brave/.gitkeep                # Brave Search API fixtures
    └── README.md                     # VCR format documentation
```

**Naming convention:** Fixture filename matches test function name.

Example:
- Test function: `test_anthropic_streaming_basic()`
- Fixture file: `tests/fixtures/vcr/anthropic/test_anthropic_streaming_basic.jsonl`

**Important:** Old fixtures in `tests/fixtures/openai/` (if they exist) are NOT migrated - they are deleted along with old OpenAI code and tests.

## Files to Create

### 1. Create VCR Root Directory

Create the VCR root directory:

- [ ] `tests/fixtures/vcr/` (parent directory for all VCR fixtures)

### 2. Create Provider Subdirectories

Create these provider-specific directories with `.gitkeep` files:

- [ ] `tests/fixtures/vcr/openai/.gitkeep`
- [ ] `tests/fixtures/vcr/anthropic/.gitkeep`
- [ ] `tests/fixtures/vcr/google/.gitkeep`
- [ ] `tests/fixtures/vcr/brave/.gitkeep`

The `.gitkeep` files ensure empty directories are tracked by git.

### 3. Create Format Documentation

Create `tests/fixtures/vcr/README.md` with the following content:

---

# VCR Test Fixtures

## Overview

This directory contains VCR-style HTTP cassettes for deterministic testing. Fixtures record real API responses and replay them in tests without network calls.

**Benefits:**
- Deterministic tests (no flaky network failures)
- Fast execution (no network latency)
- CI/CD without API credentials
- Debug with real response data

## Clean Slate Approach

**This is a REPLACEMENT, not a migration:**

VCR fixtures are the ONLY fixture format in this project. Old OpenAI-specific fixtures, tests, and code have been completely removed:

- `src/openai/` - DELETED (replaced by provider abstraction)
- Old OpenAI tests - DELETED (replaced by VCR-based provider tests)
- `tests/fixtures/openai/` - DELETED (old fixture format, not migrated)

All fixtures in this directory are recorded fresh from real API calls using VCR capture mode. There is no migration or compatibility layer with old fixtures.

## File Format: JSONL

Each fixture file is JSONL (JSON Lines) format - one JSON object per line.

### Line Types

| Line Type | Purpose |
|-----------|---------|
| `_request` | HTTP request (method, url, headers, body) |
| `_response` | HTTP response metadata (status, headers) |
| `_body` | Complete response body (non-streaming) |
| `_chunk` | Raw chunk as delivered to write callback (streaming) |

### Structure

**Single exchange:**
```
Line 1: _request (always first)
Line 2: _response (status + headers)
Line 3+: _body (non-streaming) OR _chunk lines (streaming)
```

**Multiple exchanges (multi-turn tests):**
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

## Examples

### Non-Streaming Example (Anthropic Messages API)

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json", "anthropic-version": "2023-06-01"}, "body": "{\"model\":\"claude-sonnet-4-20250514\",\"max_tokens\":1024,\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]}"}}
{"_response": {"status": 200, "headers": {"content-type": "application/json"}}}
{"_body": "{\"id\":\"msg_01ABC123\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"Hello! How can I help you today?\"}],\"model\":\"claude-sonnet-4-20250514\",\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":10,\"output_tokens\":12}}"}
```

### Streaming Example (Anthropic SSE)

Raw SSE chunks exactly as delivered by the API:

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json", "anthropic-version": "2023-06-01"}, "body": "{\"model\":\"claude-sonnet-4-20250514\",\"max_tokens\":1024,\"stream\":true,\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]}"}}
{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}
{"_chunk": "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_01ABC123\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[],\"model\":\"claude-sonnet-4-20250514\"}}\n\n"}
{"_chunk": "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"! How\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" can I help\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" you today?\"}}\n\n"}
{"_chunk": "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"}
{"_chunk": "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":12}}\n\n"}
{"_chunk": "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n"}
```

### Streaming Example (Google NDJSON)

Google uses newline-delimited JSON, not SSE:

```jsonl
{"_request": {"method": "POST", "url": "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-pro:streamGenerateContent", "headers": {"x-goog-api-key": "REDACTED", "content-type": "application/json"}, "body": "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"Hello\"}]}]}"}}
{"_response": {"status": 200, "headers": {"content-type": "application/x-ndjson"}}}
{"_chunk": "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}],\"role\":\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"promptTokenCount\":5,\"candidatesTokenCount\":1}}\n"}
{"_chunk": "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"! How\"}],\"role\":\"model\"}}],\"usageMetadata\":{\"candidatesTokenCount\":2}}\n"}
{"_chunk": "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\" can I assist you today?\"}],\"role\":\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"candidatesTokenCount\":8}}\n"}
```

### Streaming Example (OpenAI SSE)

```jsonl
{"_request": {"method": "POST", "url": "https://api.openai.com/v1/chat/completions", "headers": {"Authorization": "Bearer REDACTED", "content-type": "application/json"}, "body": "{\"model\":\"gpt-4\",\"stream\":true,\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]}"}}
{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}
{"_chunk": "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\",\"created\":1694268190,\"model\":\"gpt-4\",\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"\"},\"finish_reason\":null}]}\n\n"}
{"_chunk": "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\",\"created\":1694268190,\"model\":\"gpt-4\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}\n\n"}
{"_chunk": "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\",\"created\":1694268190,\"model\":\"gpt-4\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"! How can I\"},\"finish_reason\":null}]}\n\n"}
{"_chunk": "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\",\"created\":1694268190,\"model\":\"gpt-4\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\" help you today?\"},\"finish_reason\":null}]}\n\n"}
{"_chunk": "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\",\"created\":1694268190,\"model\":\"gpt-4\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n"}
{"_chunk": "data: [DONE]\n\n"}
```

### Error Response Example

```jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json", "anthropic-version": "2023-06-01"}, "body": "{\"model\":\"claude-sonnet-4-20250514\",\"max_tokens\":1024,\"messages\":[]}"}}
{"_response": {"status": 429, "headers": {"content-type": "application/json", "retry-after": "30"}}}
{"_body": "{\"type\":\"error\",\"error\":{\"type\":\"rate_limit_error\",\"message\":\"Number of request tokens has exceeded your per-minute rate limit\"}}"}
```

### Non-Streaming Example (Brave Search API)

```jsonl
{"_request": {"method": "GET", "url": "https://api.search.brave.com/res/v1/web/search?q=test&count=5", "headers": {"X-Subscription-Token": "REDACTED", "Accept": "application/json"}}}
{"_response": {"status": 200, "headers": {"content-type": "application/json"}}}
{"_body": "{\"web\":{\"results\":[{\"title\":\"Test - Wikipedia\",\"url\":\"https://en.wikipedia.org/wiki/Test\",\"description\":\"A test is an assessment intended to measure...\"}]}}"}
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

VCR automatically redacts credentials during recording. Manual fixture creation must follow these rules.

### Pre-commit Hook Validation

Before committing fixtures, a pre-commit hook validates no secrets leaked:

```bash
# These patterns should return NO matches
grep -rE "Bearer [^R]" tests/fixtures/vcr/   # Bearer tokens not redacted
grep -r "sk-" tests/fixtures/vcr/            # OpenAI key patterns
grep -r "sk-ant-" tests/fixtures/vcr/        # Anthropic key patterns
grep -r "AIza" tests/fixtures/vcr/           # Google key patterns
grep -r "BSA" tests/fixtures/vcr/            # Brave key patterns
```

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
- `BRAVE_API_KEY`

**Redaction:** API keys in captured requests are replaced with `REDACTED`.

## Creating Fixtures Manually

If you need to create fixtures without running capture mode:

1. Follow the JSONL structure above
2. Ensure one JSON object per line (no pretty-printing)
3. Redact all credentials using the rules above
4. Test the fixture in playback mode
5. Run pre-commit validation before committing

## Fixture Naming

Fixture filename must match test function name:

- Test function: `test_anthropic_streaming_basic()`
- Fixture file: `tests/fixtures/vcr/anthropic/test_anthropic_streaming_basic.jsonl`

This allows VCR to automatically locate the correct fixture for each test.

**Directory structure:**
- `tests/fixtures/vcr/openai/` - OpenAI provider fixtures
- `tests/fixtures/vcr/anthropic/` - Anthropic provider fixtures
- `tests/fixtures/vcr/google/` - Google provider fixtures
- `tests/fixtures/vcr/brave/` - Brave Search API fixtures

---

## Postconditions

- [ ] Directory `tests/fixtures/vcr/` exists (root for all VCR fixtures)
- [ ] Directory `tests/fixtures/vcr/openai/` exists with `.gitkeep`
- [ ] Directory `tests/fixtures/vcr/anthropic/` exists with `.gitkeep`
- [ ] Directory `tests/fixtures/vcr/google/` exists with `.gitkeep`
- [ ] Directory `tests/fixtures/vcr/brave/` exists with `.gitkeep`
- [ ] File `tests/fixtures/vcr/README.md` exists with complete JSONL format documentation
- [ ] README includes examples for all line types (_request, _response, _body, _chunk)
- [ ] README documents credential redaction rules
- [ ] README documents playback vs capture modes
- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] Changes committed to git with message: `task: vcr-fixtures-setup.md - create VCR fixture directories and documentation`

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails.
