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

```c
// Mock HTTP response
MOCKABLE(curl_easy_perform_);

static int mock_http_status = 200;
static const char *mock_http_body = NULL;

int mock_curl_easy_perform_(CURL *handle)
{
    // Simulate HTTP response
    curl_write_callback write_cb = get_write_callback(handle);
    void *write_ctx = get_write_context(handle);

    // Call write callback with mock body
    write_cb(mock_http_body, 1, strlen(mock_http_body), write_ctx);

    return CURLE_OK;
}

// In test
START_TEST(test_anthropic_basic_request) {
    mock_http_status = 200;
    mock_http_body = load_fixture("anthropic/response_basic.json");

    ik_provider_t *provider = NULL;
    TRY(ik_anthropic_create(ctx, "sk-ant-test", &provider));

    ik_request_t *req = create_test_request();
    ik_response_t *resp = NULL;

    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(resp->content[0].data.text.text, "Hello!");
}
END_TEST
```

### SSE Streaming Mock

```c
static const char *mock_sse_events[] = {
    "event: message_start\ndata: {\"message\":{\"id\":\"msg_123\"}}\n\n",
    "event: content_block_delta\ndata: {\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n\n",
    "event: message_stop\ndata: {}\n\n",
    NULL
};

static int mock_sse_index = 0;

int mock_curl_easy_perform_stream_(CURL *handle)
{
    curl_write_callback write_cb = get_write_callback(handle);
    void *write_ctx = get_write_context(handle);

    // Send each SSE event
    for (int i = 0; mock_sse_events[i] != NULL; i++) {
        write_cb(mock_sse_events[i], 1, strlen(mock_sse_events[i]), write_ctx);
    }

    return CURLE_OK;
}

START_TEST(test_anthropic_streaming) {
    mock_sse_events = load_sse_fixture("anthropic/stream_basic.txt");

    ik_provider_t *provider = NULL;
    TRY(ik_anthropic_create(ctx, "sk-ant-test", &provider));

    ik_stream_event_t events[10];
    int event_count = 0;

    auto callback = lambda(void, (ik_stream_event_t *e, void *ctx) {
        events[event_count++] = *e;
    });

    ik_request_t *req = create_test_request();
    res_t result = provider->vt->stream(provider->impl_ctx, req, callback, NULL);

    ck_assert(is_ok(&result));
    ck_assert_int_eq(event_count, 3);
    ck_assert_int_eq(events[0].type, IK_STREAM_START);
    ck_assert_int_eq(events[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_int_eq(events[2].type, IK_STREAM_DONE);
}
END_TEST
```

## Live Validation Tests

### Opt-In via Environment Variable

```c
#ifdef ENABLE_LIVE_API_TESTS

START_TEST(test_anthropic_live_validation) {
    // Only runs if ANTHROPIC_API_KEY is set AND ENABLE_LIVE_API_TESTS=1

    const char *api_key = getenv("ANTHROPIC_API_KEY");
    if (api_key == NULL) {
        ck_skip("ANTHROPIC_API_KEY not set");
        return;
    }

    // Make real API call
    ik_provider_t *provider = NULL;
    TRY(ik_anthropic_create(ctx, api_key, &provider));

    ik_request_t *req = create_simple_request("Hello");
    ik_response_t *resp = NULL;

    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_ok(&result));
    ck_assert(resp->content_count > 0);

    // Optionally: update fixture
    if (getenv("UPDATE_FIXTURES")) {
        save_fixture("anthropic/response_basic.json", resp);
    }
}
END_TEST

#endif
```

### Running Live Tests

```bash
# Normal tests (mocked)
make test

# Live validation tests (requires API keys)
ENABLE_LIVE_API_TESTS=1 \
ANTHROPIC_API_KEY=sk-ant-... \
OPENAI_API_KEY=sk-... \
GOOGLE_API_KEY=... \
make test

# Update fixtures
ENABLE_LIVE_API_TESTS=1 \
UPDATE_FIXTURES=1 \
ANTHROPIC_API_KEY=sk-ant-... \
make test
```

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

### Fixture Loading

```c
const char *load_fixture(const char *path)
{
    char *full_path = talloc_asprintf(NULL, "tests/fixtures/%s", path);

    FILE *f = fopen(full_path, "r");
    if (f == NULL) {
        fprintf(stderr, "Failed to load fixture: %s\n", full_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = talloc_array(NULL, char, len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';

    fclose(f);
    talloc_free(full_path);

    return data;
}
```

## Error Testing

### Authentication Errors

```c
START_TEST(test_anthropic_auth_error) {
    mock_http_status = 401;
    mock_http_body = load_fixture("anthropic/error_auth.json");

    ik_provider_t *provider = create_anthropic_provider("bad-key");
    ik_request_t *req = create_test_request();
    ik_response_t *resp = NULL;

    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err.category, ERR_AUTH);
    ck_assert_int_eq(result.err.http_status, 401);
    ck_assert_str_contains(result.err.message, "API key");
}
END_TEST
```

### Rate Limit Errors

```c
START_TEST(test_anthropic_rate_limit) {
    mock_http_status = 429;
    mock_http_body = load_fixture("anthropic/error_rate_limit.json");
    mock_http_headers["retry-after"] = "60";

    ik_provider_t *provider = create_anthropic_provider("sk-ant-test");
    ik_request_t *req = create_test_request();
    ik_response_t *resp = NULL;

    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err.category, ERR_RATE_LIMIT);
    ck_assert_int_eq(result.err.retry_after_ms, 60000);
}
END_TEST
```

## Thinking Level Testing

### Budget Calculation

```c
START_TEST(test_anthropic_thinking_budget_calculation) {
    // Sonnet 4.5: min=1024, max=64000
    int32_t budget_none = ik_anthropic_thinking_budget("claude-sonnet-4-5",
                                                       IK_THINKING_NONE);
    ck_assert_int_eq(budget_none, 1024);

    int32_t budget_low = ik_anthropic_thinking_budget("claude-sonnet-4-5",
                                                      IK_THINKING_LOW);
    ck_assert_int_eq(budget_low, 22016);  // 1024 + 1/3 * 62976

    int32_t budget_med = ik_anthropic_thinking_budget("claude-sonnet-4-5",
                                                      IK_THINKING_MED);
    ck_assert_int_eq(budget_med, 43008);  // 1024 + 2/3 * 62976

    int32_t budget_high = ik_anthropic_thinking_budget("claude-sonnet-4-5",
                                                       IK_THINKING_HIGH);
    ck_assert_int_eq(budget_high, 64000);
}
END_TEST
```

### Cross-Provider Consistency

```c
START_TEST(test_thinking_levels_consistent) {
    // All providers should accept same thinking levels

    ik_provider_t *anthropic = create_anthropic_provider("key");
    ik_provider_t *openai = create_openai_provider("key");
    ik_provider_t *google = create_google_provider("key");

    ik_request_t *req = create_test_request();
    req->thinking.level = IK_THINKING_MED;

    // Should serialize successfully for all providers
    char *anthropic_json = NULL;
    TRY(ik_anthropic_serialize_request(ctx, req, &anthropic_json));
    ck_assert_str_contains(anthropic_json, "thinking");

    char *openai_json = NULL;
    TRY(ik_openai_serialize_request(ctx, req, &openai_json));
    ck_assert_str_contains(openai_json, "reasoning");

    char *google_json = NULL;
    TRY(ik_google_serialize_request(ctx, req, &google_json));
    ck_assert_str_contains(google_json, "thinkingConfig");
}
END_TEST
```

## Integration Tests

### End-to-End Provider Switching

```c
START_TEST(test_e2e_switch_providers) {
    // Create agent
    ik_agent_t *agent = create_test_agent(ctx, "root");

    // Set to Anthropic
    agent->provider = "anthropic";
    agent->model = "claude-sonnet-4-5";
    agent->thinking_level = IK_THINKING_MED;

    // Send message (mocked)
    mock_anthropic_response();
    send_message(agent, "Hello");

    // Verify stored with correct provider
    ik_message_t *msg = get_last_message(agent);
    ck_assert_str_eq(msg->data.provider, "anthropic");

    // Switch to OpenAI
    agent->provider = "openai";
    agent->model = "gpt-4o";
    agent->thinking_level = IK_THINKING_NONE;

    // Send another message
    mock_openai_response();
    send_message(agent, "How are you?");

    // Verify new message uses OpenAI
    msg = get_last_message(agent);
    ck_assert_str_eq(msg->data.provider, "openai");
}
END_TEST
```

### Tool Calls Across Providers

```c
START_TEST(test_e2e_tool_calls_multi_provider) {
    // Test that tool calls work consistently across all providers

    ik_tool_def_t tool = {
        .name = "read_file",
        .description = "Read file contents",
        .parameters = create_file_params_schema()
    };

    // Test Anthropic
    test_tool_call_flow(create_anthropic_provider(), &tool);

    // Test OpenAI
    test_tool_call_flow(create_openai_provider(), &tool);

    // Test Google (generates UUID for tool call ID)
    test_tool_call_flow(create_google_provider(), &tool);
}
END_TEST

void test_tool_call_flow(ik_provider_t *provider, ik_tool_def_t *tool)
{
    ik_request_t *req = create_test_request();
    req->tools = &tool;
    req->tool_count = 1;

    // Mock response with tool call
    mock_tool_call_response(provider->name);

    ik_response_t *resp = NULL;
    res_t result = provider->vt->send(provider->impl_ctx, req, &resp);

    ck_assert(is_ok(&result));
    ck_assert_int_eq(resp->content_count, 1);
    ck_assert_int_eq(resp->content[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content[0].data.tool_call.name, "read_file");

    // Verify tool call ID format
    if (strcmp(provider->name, "google") == 0) {
        // Google: should be 22-char base64url UUID
        ck_assert_int_eq(strlen(resp->content[0].data.tool_call.id), 22);
    } else {
        // Others: provider-generated ID
        ck_assert(strlen(resp->content[0].data.tool_call.id) > 0);
    }
}
```

## Performance Testing

### Request Serialization Benchmark

```c
START_TEST(test_perf_serialization) {
    ik_request_t *req = create_large_request();  // 10 messages, 5 tools

    struct timespec start, end;

    // Benchmark Anthropic
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < 1000; i++) {
        char *json = NULL;
        ik_anthropic_serialize_request(ctx, req, &json);
        talloc_free(json);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    long anthropic_ns = (end.tv_sec - start.tv_sec) * 1000000000L +
                       (end.tv_nsec - start.tv_nsec);

    printf("Anthropic serialization: %ld ns/request\n", anthropic_ns / 1000);

    // Should be < 100us per request
    ck_assert(anthropic_ns / 1000 < 100000);
}
END_TEST
```

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
ENABLE_LIVE_API_TESTS=1 \
ANTHROPIC_API_KEY=$ANTHROPIC_KEY \
OPENAI_API_KEY=$OPENAI_KEY \
GOOGLE_API_KEY=$GOOGLE_KEY \
make test-live
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
