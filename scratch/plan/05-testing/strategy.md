# Testing Strategy

## Clean Slate Approach

This release REPLACES all old OpenAI code and tests. Old tests/fixtures are deleted, not migrated.

**What is deleted:**
- `src/openai/` - Entire directory and all code
- Old unit/integration tests for OpenAI client
- Old fixtures in `tests/fixtures/` (non-VCR format)

**What remains:**
- New provider abstraction in `src/providers/`
- VCR-based tests with fixtures in `tests/fixtures/vcr/`
- Only new code - no migration, no compatibility layer

## Overview

The multi-provider abstraction testing follows ikigai's existing patterns:
- **Mock HTTP layer** for provider tests (no real API calls)
- **Live validation tests** (opt-in) to verify mocks against real APIs
- **Unit tests** for each provider adapter
- **Integration tests** for end-to-end flows

**Key testing principle:** Tests must verify the async behavior, not blocking behavior. Mock implementations must exercise the fdset/perform/info_read pattern.

## Test Organization

```
tests/
  unit/
    providers/
      common/
        test_http_multi.c          # Shared HTTP client
        test_sse_parser.c          # Shared SSE parser

      anthropic/
        test_anthropic_adapter.c   # Vtable implementation
        test_anthropic_client.c    # Request serialization
        test_anthropic_streaming.c # SSE event handling
        test_anthropic_errors.c    # Error mapping

      openai/
        test_openai_adapter.c
        test_openai_client.c
        test_openai_streaming.c
        test_openai_errors.c

      google/
        test_google_adapter.c
        test_google_client.c
        test_google_streaming.c
        test_google_errors.c

    test_provider_common.c         # Provider creation, credentials

  integration/
    test_multi_provider_e2e.c      # End-to-end multi-provider flows
    test_thinking_levels_e2e.c     # Thinking abstraction across providers

  contract_validations/            # SEPARATE from normal tests
    openai_contract_test.c         # OpenAI API contract validation
    anthropic_contract_test.c      # Anthropic API contract validation
    google_contract_test.c         # Google API contract validation

  fixtures/
    vcr/
      anthropic/
        test_anthropic_streaming_basic.jsonl
        test_anthropic_streaming_tool_call.jsonl
        test_anthropic_thinking_high.jsonl
        test_anthropic_error_rate_limit.jsonl

      openai/
        test_openai_chat_basic.jsonl
        test_openai_streaming_basic.jsonl
        test_openai_responses_api.jsonl

      google/
        test_google_streaming_basic.jsonl
        test_google_thinking_medium.jsonl
```

## Mock HTTP Pattern

### Basic Pattern (Async via curl_multi)

Tests use the MOCKABLE() macro to intercept curl_multi_* calls. The mock pattern must simulate the async fdset/perform/info_read cycle, not blocking calls.

**Mock curl_multi functions:**
- `curl_multi_fdset_` - Returns mock FDs (can return -1 for no FDs)
- `curl_multi_perform_` - Simulates progress, updates running handles
- `curl_multi_info_read_` - Returns completion messages with mock responses

**Test flow for non-streaming:**
```c
// Test captures response via callback
static ik_response_t *captured_response;
static res_t captured_result;

static void test_completion_cb(void *ctx, res_t result, ik_response_t *resp) {
    captured_result = result;
    captured_response = resp;
}

START_TEST(test_non_streaming_request)
{
    captured_response = NULL;

    // Setup: Load JSONL fixture and initialize VCR
    vcr_init(__func__, "anthropic");  // Loads test_non_streaming_request.jsonl

    // Create provider
    res_t r = ik_anthropic_create(ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Start request with callback
    r = provider->vt->start_request(provider->ctx, req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    while (captured_response == NULL) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        select(max_fd + 1, &read_fds, &write_fds, &exc_fds, NULL);
        provider->vt->perform(provider->ctx, NULL);
    }

    // Assert on captured response
    ck_assert(is_ok(&captured_result));
    ck_assert_str_eq(captured_response->content, "expected");
}
END_TEST
```

### SSE Streaming Mock

For streaming tests, mock implementations maintain an array of SSE event strings. The mock curl_multi_perform function feeds chunks to the write callback incrementally.

**Mock streaming flow:**
```c
// Setup: Load JSONL fixture with SSE chunks
vcr_init(__func__, "anthropic");  // Loads JSONL with _chunk lines

// Start async stream (returns immediately)
r = provider->vt->start_stream(provider->ctx, req, stream_cb, &events, ...);
assert(is_ok(&r));

// Simulate event loop - each perform() delivers some SSE events
int running = 1;
while (running > 0) {
    provider->vt->perform(provider->ctx, &running);
    // stream_cb is invoked during perform() as data arrives
}
provider->vt->info_read(provider->ctx, logger);

// Verify stream events were captured
assert(events.count == expected_event_count);
assert(events.items[0].type == IK_STREAM_START);
assert(events.items[events.count-1].type == IK_STREAM_DONE);
```

### Fixture Loading

A utility function loads JSONL fixture files from the tests/fixtures/vcr/ directory using talloc for memory management. The loader:
- Constructs the full path to the `.jsonl` file
- Reads file line-by-line (one JSON object per line)
- Parses each line to extract `_request`, `_response`, `_body`, or `_chunk` objects
- Returns structured data for use in VCR playback mode

**Example usage:**
```c
// Load fixture for VCR playback
const char *fixture_path = "tests/fixtures/vcr/anthropic/test_anthropic_basic.jsonl";
vcr_cassette_t *cassette = vcr_load_cassette(ctx, fixture_path);
```

## Live Validation Tests

### Opt-In via Environment Variable

Live API tests are wrapped in ENABLE_LIVE_API_TESTS preprocessor guards. They check for provider API keys in environment variables and skip if not present. When run with real credentials, they make actual API calls to verify that mock responses accurately represent live API behavior. An UPDATE_FIXTURES environment variable allows tests to save live responses back to fixture files for maintaining test data accuracy.

### Credentials Source

**The credentials file `~/.config/ikigai/credentials.json` already exists with valid API keys for all three providers.** The Makefile's `verify-mocks` target reads from this file and sets environment variables:

```makefile
CREDS_FILE="$$HOME/.config/ikigai/credentials.json"
OPENAI_KEY=$$(jq -r '.openai.api_key // empty' "$$CREDS_FILE")
ANTHROPIC_KEY=$$(jq -r '.anthropic.api_key // empty' "$$CREDS_FILE")
GOOGLE_KEY=$$(jq -r '.google.api_key // empty' "$$CREDS_FILE")
```

### Running Live Tests

Standard test execution uses mocks and requires no API keys. Live validation tests require setting ENABLE_LIVE_API_TESTS=1 plus provider API keys. The UPDATE_FIXTURES=1 flag enables saving live responses to fixture files.

Command examples:
- Normal tests: `make test`
- Verify all fixtures: `make verify-mocks-all` (reads from credentials.json)
- Verify single provider: `make verify-mocks-anthropic`
- Update fixtures: `UPDATE_FIXTURES=1 make verify-mocks-all`

## Test Fixtures

All fixtures use JSONL format (JSON Lines, newline-delimited JSON). Each line is a complete JSON object representing one part of an HTTP exchange.

### JSONL Format Benefits

- **Streamable:** Can be processed line-by-line without loading entire file
- **Appendable:** New exchanges can be added to multi-turn tests
- **Easier diffs:** Line-based format shows clear before/after in version control
- **File extension:** `.jsonl` (not `.json`)

### Line Types

| Line Type | Purpose |
|-----------|---------|
| `_request` | HTTP request (method, url, headers, body) |
| `_response` | HTTP response metadata (status, headers) |
| `_body` | Complete response body (non-streaming) |
| `_chunk` | Raw chunk as delivered to write callback (streaming) |

### Non-Streaming JSONL Fixture

```jsonl
// tests/fixtures/vcr/anthropic/test_anthropic_basic.jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json"}, "body": "{\"model\":\"claude-sonnet-4-5-20250514\",\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]}"}}
{"_response": {"status": 200, "headers": {"content-type": "application/json"}}}
{"_body": "{\"id\":\"msg_123\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-sonnet-4-5-20250514\",\"content\":[{\"type\":\"text\",\"text\":\"Hello! How can I assist you today?\"}],\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":50,\"output_tokens\":15}}"}
```

### Streaming JSONL Fixture

For streaming responses, each `_chunk` line contains exactly what curl's write callback received (one callback invocation = one `_chunk` line).

```jsonl
// tests/fixtures/vcr/anthropic/test_anthropic_streaming_basic.jsonl
{"_request": {"method": "POST", "url": "https://api.anthropic.com/v1/messages", "headers": {"x-api-key": "REDACTED", "content-type": "application/json"}, "body": "{\"model\":\"claude-sonnet-4-5-20250514\",\"stream\":true,\"messages\":[...]}"}}
{"_response": {"status": 200, "headers": {"content-type": "text/event-stream"}}}
{"_chunk": "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_123\",\"model\":\"claude-sonnet-4-5-20250514\"}}\n\n"}
{"_chunk": "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n\n"}
{"_chunk": "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"!\"}}\n\n"}
{"_chunk": "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"}
{"_chunk": "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":2}}\n\n"}
{"_chunk": "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n"}
```

## Error Testing

### Authentication Errors

Tests verify that authentication failures (HTTP 401) are properly mapped to IK_ERR_CAT_AUTH category with appropriate error messages. Mock responses return auth error fixtures with invalid API key scenarios.

### Rate Limit Errors

Tests verify that rate limit responses (HTTP 429) are mapped to IK_ERR_CAT_RATE_LIMIT category. Mock responses include retry-after headers, and tests verify that retry_after_ms is correctly extracted and converted to milliseconds.

## Thinking Level Testing

### Budget Calculation

Tests verify that provider-specific thinking budget calculation correctly maps abstract thinking levels to concrete token budgets. For Anthropic Claude Sonnet 4.5, the test verifies:
- IK_THINKING_NONE maps to minimum (1024)
- IK_THINKING_LOW maps to 1/3 of range
- IK_THINKING_MED maps to 2/3 of range
- IK_THINKING_HIGH maps to maximum (64000)

### Cross-Provider Consistency

Integration tests verify that all providers accept the same thinking level enum values and correctly serialize them to provider-specific request formats. Tests create requests with thinking levels and verify that:
- Anthropic serializes to "thinking" parameter
- OpenAI serializes to "reasoning" parameter
- Google serializes to "thinkingConfig" parameter

## Integration Tests

### End-to-End Provider Switching

Tests verify that agents can dynamically switch between providers during a conversation. Tests create an agent, send messages using one provider, verify stored messages contain correct provider metadata, switch to a different provider, send more messages, and verify that new messages use the new provider. Message history correctly preserves provider-specific data.

### Tool Calls Across Providers

Tests verify that tool call functionality works consistently across all providers despite differing ID formats. Tests define a tool schema, invoke it through each provider, and verify:
- Tool call content type is correctly set
- Tool name and parameters are preserved
- Tool call IDs follow provider conventions (Google generates UUID, others use provider IDs)
- Response parsing correctly extracts tool call data

## Performance Testing

### Request Serialization Benchmark

Performance tests measure serialization time for complex requests (10 messages, 5 tools) across providers. Tests use clock_gettime with CLOCK_MONOTONIC to measure 1000 serialization iterations, calculate average time per request, and verify performance is under 100 microseconds per request. Results are printed for visibility.

## Coverage Requirements

Target: **100% coverage** for provider adapters.

Critical paths:
- ✅ Request serialization (all content types)
- ✅ Response parsing (all content types)
- ✅ Error mapping (all HTTP statuses)
- ✅ Thinking level calculation
- ✅ Tool call handling
- ✅ Streaming events (all event types)

Use existing coverage tools:

```bash
make coverage
firefox coverage/index.html
```

## Test Execution

### Run All Tests

```bash
make test
```

### Run Provider-Specific Tests

```bash
# Anthropic only
make test-anthropic

# OpenAI only
make test-openai

# Google only
make test-google
```

### Run with Valgrind

```bash
make valgrind
```

### Run Live Tests

```bash
# Credentials are read automatically from ~/.config/ikigai/credentials.json
make verify-mocks-all

# Or run specific provider validation
make verify-mocks-openai
make verify-mocks-anthropic
make verify-mocks-google
```

## Continuous Integration

GitHub Actions workflow:

```yaml
name: Multi-Provider Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libpq-dev libcurl4-openssl-dev check

      - name: Run unit tests
        run: make test

      - name: Run coverage
        run: make coverage

      - name: Upload coverage
        uses: codecov/codecov-action@v2

  # Optional: Live tests (with secrets)
  test-live:
    runs-on: ubuntu-latest
    if: github.event_name == 'workflow_dispatch'
    steps:
      - name: Run live tests
        env:
          ANTHROPIC_API_KEY: ${{ secrets.ANTHROPIC_API_KEY }}
          OPENAI_API_KEY: ${{ secrets.OPENAI_API_KEY }}
          GOOGLE_API_KEY: ${{ secrets.GOOGLE_API_KEY }}
        run: |
          ENABLE_LIVE_API_TESTS=1 make test-live
```
