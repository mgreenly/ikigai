# Testing Strategy

## Overview

The multi-provider abstraction testing follows ikigai's existing patterns:
- **Mock HTTP layer** for provider tests (no real API calls)
- **Live validation tests** (opt-in) to verify mocks against real APIs
- **Unit tests** for each provider adapter
- **Integration tests** for end-to-end flows

## Test Organization

```
tests/
  unit/
    providers/
      common/
        test_http_client.c        # Shared HTTP client
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
    anthropic/
      response_basic.json
      response_thinking.json
      response_tool_call.json
      stream_basic.txt
      stream_thinking.txt
      error_auth.json
      error_rate_limit.json

    openai/
      (similar structure)

    google/
      (similar structure)
```

## Mock HTTP Pattern

### Basic Pattern (from existing OpenAI tests)

Tests use the MOCKABLE() macro to intercept curl_easy_perform_ calls. Mock implementations set static variables for HTTP status and response body, then invoke the registered write callback with fixture data. Each test sets up mock response data by loading JSON fixtures, creates a provider instance with a test API key, constructs a test request, and invokes the provider's send method. Assertions verify that results are successful and response content matches expected values.

### SSE Streaming Mock

For streaming tests, mock implementations maintain an array of SSE event strings (with proper "event:" and "data:" formatting). The mock curl perform function iterates through these events, invoking the write callback for each chunk to simulate incremental data arrival. Tests load SSE fixtures, capture stream events via lambda callbacks, and verify event types, counts, and ordering.

### Fixture Loading

A utility function loads fixture files from the tests/fixtures/ directory using talloc for memory management. It constructs the full path, reads the file content, null-terminates the data, and returns it for use in mock responses.

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

### JSON Response Fixtures

```json
// tests/fixtures/anthropic/response_basic.json
{
  "id": "msg_123",
  "type": "message",
  "role": "assistant",
  "model": "claude-sonnet-4-5-20250929",
  "content": [
    {
      "type": "text",
      "text": "Hello! How can I assist you today?"
    }
  ],
  "stop_reason": "end_turn",
  "usage": {
    "input_tokens": 50,
    "output_tokens": 15
  }
}
```

### SSE Stream Fixtures

```
// tests/fixtures/anthropic/stream_basic.txt
event: message_start
data: {"type":"message_start","message":{"id":"msg_123","model":"claude-sonnet-4-5-20250929"}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"!"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":2}}

event: message_stop
data: {"type":"message_stop"}
```

## Error Testing

### Authentication Errors

Tests verify that authentication failures (HTTP 401) are properly mapped to ERR_AUTH category with appropriate error messages. Mock responses return auth error fixtures with invalid API key scenarios.

### Rate Limit Errors

Tests verify that rate limit responses (HTTP 429) are mapped to ERR_RATE_LIMIT category. Mock responses include retry-after headers, and tests verify that retry_after_ms is correctly extracted and converted to milliseconds.

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
