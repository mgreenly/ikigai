# Task: Create Provider Mock Verification Tests (VCR-Style)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** vcr-core.md, vcr-mock-integration.md, vcr-fixtures-setup.md, credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Rationale: VCR-Style Mocking

**Problem:** The task chain includes 225+ tests that depend on mock infrastructure. If mocks don't accurately represent real API behavior, tests pass but code fails in production.

**Solution:** Use VCR-style mocking (named after the Ruby VCR gem):
1. **Record** - Call real APIs and capture actual HTTP responses ("cassettes")
2. **Verify** - Assert response structure matches expected format
3. **Replay** - Mocks replay recorded responses during tests

This eliminates the risk of mocks diverging from real API behavior.

**Fixture Format:** All fixtures use JSONL (JSON Lines) format as specified in `scratch/plan/vcr-cassettes.md`. Each fixture contains `_request`, `_response`, and `_chunk`/`_body` lines representing the complete HTTP exchange.

**VCR Principle:**
```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Real API      │ --> │   Cassettes     │ --> │   Mock Replay   │
│   (record)      │     │   (fixtures)    │     │   (tests)       │
└─────────────────┘     └─────────────────┘     └─────────────────┘
         │                       │                       │
         v                       v                       v
   verify-mocks            Validated               Unit tests
   (live calls)            fixtures              (deterministic)
```

**Key insight:** Mocks can't be wrong because they're real responses, not guessed ones.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `src/credentials.h` exists (from credentials-core.md)
- [ ] All provider credentials valid (verify: `make verify-credentials` exits 0)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load style` - Code style for test files

**Source:**
- `tests/integration/openai_mock_verification_test.c` - Existing pattern to follow
- `tests/fixtures/vcr/openai/` - Existing fixture structure
- `src/credentials.h` - Credential loading API

**External Documentation (for reference only - do not fetch):**
- Anthropic Messages API: https://docs.anthropic.com/en/api/messages
- Google Gemini API: https://ai.google.dev/api/generate-content

## Objective

Create verification test suites for Anthropic and Google APIs that:
1. Make real API calls to document expected behavior
2. Assert response structure, event ordering, and field presence
3. Optionally capture responses to fixture files when `CAPTURE_FIXTURES=1`
4. Validate existing fixtures match real API responses

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `tests/integration/anthropic_mock_verification_test.c` | Verification tests for Anthropic API |
| `tests/integration/google_mock_verification_test.c` | Verification tests for Google API |

**Directories to create:**

| Directory | Purpose |
|-----------|---------|
| `tests/fixtures/vcr/anthropic/` | Anthropic response fixtures |
| `tests/fixtures/vcr/google/` | Google response fixtures |

**Makefile targets to add:**

| Target | Purpose |
|--------|---------|
| `verify-credentials` | Check all provider API keys are available (env vars or credentials.json) |
| `verify-mocks-anthropic` | Run Anthropic verification (requires ANTHROPIC_API_KEY) |
| `verify-mocks-google` | Run Google verification (requires GOOGLE_API_KEY) |
| `verify-mocks-all` | Run all provider verifications |

## Behaviors

### Test Structure Pattern

Follow the existing `openai_mock_verification_test.c` pattern:

```c
// Skip if not in verification mode
static bool should_verify_mocks(void)
{
    const char *verify = getenv("VERIFY_MOCKS");
    return verify != NULL && strcmp(verify, "1") == 0;
}

// Optional fixture capture
static bool should_capture_fixtures(void)
{
    const char *capture = getenv("CAPTURE_FIXTURES");
    return capture != NULL && strcmp(capture, "1") == 0;
}

// Save request/response to fixture file in JSONL format
// Writes _request, _response, and _chunk/_body lines as specified in vcr-cassettes.md
// API keys are redacted before writing to ensure no credentials leak
static void capture_fixture(const char *provider, const char *name,
                            const char *method, const char *url,
                            const char *request_headers, const char *request_body,
                            int status_code, const char *response_headers,
                            const char **chunks, size_t chunk_count)
{
    if (!should_capture_fixtures()) return;

    char path[256];
    snprintf(path, sizeof(path), "tests/fixtures/vcr/%s/%s.jsonl", provider, name);

    FILE *f = fopen(path, "w");
    if (f) {
        // Write _request line (with redacted credentials)
        fprintf(f, "{\"_request\": {\"method\": \"%s\", \"url\": \"%s\", \"headers\": %s, \"body\": %s}}\n",
                method, url, redact_headers(request_headers), request_body);

        // Write _response line
        fprintf(f, "{\"_response\": {\"status\": %d, \"headers\": %s}}\n",
                status_code, response_headers);

        // Write _chunk lines (streaming) or _body line (non-streaming)
        if (chunk_count == 1) {
            fprintf(f, "{\"_body\": %s}\n", chunks[0]);
        } else {
            for (size_t i = 0; i < chunk_count; i++) {
                fprintf(f, "{\"_chunk\": %s}\n", chunks[i]);
            }
        }

        fclose(f);
        fprintf(stderr, "Captured fixture: %s\n", path);
    }
}
```

### Credential Validation (First Step)

Before running any tests, verify credentials are available. Call this in suite setup.

**Function:** `verify_credentials_available(void)`

**Behavior:**
- Check `ANTHROPIC_API_KEY` and `GOOGLE_API_KEY` environment variables
- If either is missing or empty, print diagnostic message pointing to `make verify-credentials`
- Exit with code 77 (Check framework skip code) if credentials unavailable
- This ensures early failure with clear diagnostics rather than cryptic HTTP errors later

### Anthropic Verification Tests

**File:** `tests/integration/anthropic_mock_verification_test.c`

#### Test 1: `verify_anthropic_streaming_text`

**Purpose:** Verify basic streaming text response structure and event ordering.

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 100,
  "stream": true,
  "messages": [{"role": "user", "content": "Say hello"}]
}
```

**Assertions:**
- SSE events arrive in order: `message_start` → `content_block_start` → `content_block_delta`* → `content_block_stop` → `message_delta` → `message_stop`
- `message_start` contains: `type`, `message.id`, `message.role`, `message.model`
- `content_block_start` contains: `type`, `index`, `content_block.type` ("text")
- `content_block_delta` contains: `type`, `index`, `delta.type` ("text_delta"), `delta.text`
- `message_delta` contains: `type`, `delta.stop_reason`, `usage.output_tokens`
- Final content is non-empty string
- `stop_reason` is one of: "end_turn", "max_tokens", "stop_sequence", "tool_use"

**Fixture:** `stream_text_basic.jsonl`

#### Test 2: `verify_anthropic_streaming_thinking`

**Purpose:** Verify extended thinking response structure.

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1000,
  "stream": true,
  "thinking": {"type": "enabled", "budget_tokens": 500},
  "messages": [{"role": "user", "content": "What is 15 * 17?"}]
}
```

**Assertions:**
- `content_block_start` with `content_block.type` = "thinking" appears BEFORE "text" block
- `content_block_delta` for thinking block has `delta.type` = "thinking_delta"
- Thinking block index is 0, text block index is 1
- Both blocks have corresponding `content_block_stop` events
- `message_delta.usage` includes thinking tokens

**Fixture:** `stream_text_thinking.jsonl`

#### Test 3: `verify_anthropic_tool_call`

**Purpose:** Verify tool use response structure.

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 500,
  "stream": true,
  "tools": [{
    "name": "get_weather",
    "description": "Get weather for a location",
    "input_schema": {
      "type": "object",
      "properties": {"location": {"type": "string"}},
      "required": ["location"]
    }
  }],
  "messages": [{"role": "user", "content": "What's the weather in Paris?"}]
}
```

**Assertions:**
- `content_block_start` with `content_block.type` = "tool_use" appears
- Tool block contains: `id` (starts with "toolu_"), `name`, `input` (initially empty object)
- `content_block_delta` with `delta.type` = "input_json_delta" accumulates JSON
- Accumulated `input` is valid JSON matching tool schema
- `stop_reason` = "tool_use"

**Fixture:** `stream_tool_call.jsonl`

#### Test 4: `verify_anthropic_error_auth`

**Purpose:** Verify authentication error response structure.

**Request:** Use invalid API key `sk-ant-invalid`

**Assertions:**
- HTTP status 401
- Response contains: `type` = "error", `error.type` = "authentication_error", `error.message`

**Fixture:** `error_401_auth.json`

#### Test 5: `verify_anthropic_error_rate_limit`

**Purpose:** Document rate limit error structure (synthetic fixture).

**Note:** Cannot reliably trigger rate limit. Create synthetic fixture based on API documentation.

**Fixture:** `error_429_rate_limit.json` (synthetic)
```json
{
  "type": "error",
  "error": {
    "type": "rate_limit_error",
    "message": "Rate limit exceeded. Please retry after 30 seconds."
  }
}
```

### Google Verification Tests

**File:** `tests/integration/google_mock_verification_test.c`

#### Test 1: `verify_google_streaming_text`

**Purpose:** Verify basic streaming text response structure.

**Request:**
```
POST /v1beta/models/gemini-2.0-flash:streamGenerateContent?alt=sse&key=API_KEY
{
  "contents": [{"role": "user", "parts": [{"text": "Say hello"}]}],
  "generationConfig": {"maxOutputTokens": 100}
}
```

**Assertions:**
- Each SSE `data:` line contains complete JSON object
- Each chunk has: `candidates[0].content.parts[0].text`
- Final chunk has: `candidates[0].finishReason`, `usageMetadata`
- `finishReason` is one of: "STOP", "MAX_TOKENS", "SAFETY", "RECITATION", "OTHER"
- `usageMetadata` contains: `promptTokenCount`, `candidatesTokenCount`, `totalTokenCount`

**Fixture:** `stream_text_basic.jsonl`

#### Test 2: `verify_google_streaming_thinking`

**Purpose:** Verify thinking response structure (Gemini 2.5+).

**Request:**
```json
{
  "contents": [{"role": "user", "parts": [{"text": "What is 15 * 17?"}]}],
  "generationConfig": {
    "maxOutputTokens": 1000,
    "thinkingConfig": {"thinkingBudget": 500}
  }
}
```

**Assertions:**
- Parts with `thought: true` flag contain thinking content
- Parts without `thought` flag (or `thought: false`) contain regular content
- Thinking parts appear before regular content parts
- `usageMetadata` may include `thoughtsTokenCount` (model-dependent)

**Fixture:** `stream_text_thinking.jsonl`

#### Test 3: `verify_google_tool_call`

**Purpose:** Verify function calling response structure.

**Request:**
```json
{
  "contents": [{"role": "user", "parts": [{"text": "What's the weather in Paris?"}]}],
  "tools": [{
    "functionDeclarations": [{
      "name": "get_weather",
      "description": "Get weather for a location",
      "parameters": {
        "type": "object",
        "properties": {"location": {"type": "string"}},
        "required": ["location"]
      }
    }]
  }]
}
```

**Assertions:**
- Response contains `candidates[0].content.parts[*].functionCall`
- `functionCall` has: `name`, `args` (JSON object)
- `args` matches function parameter schema
- Note: Google does NOT provide tool call IDs - must be generated client-side

**Fixture:** `stream_tool_call.jsonl`

#### Test 4: `verify_google_error_auth`

**Purpose:** Verify authentication error response structure.

**Request:** Use invalid API key

**Assertions:**
- HTTP status 401 or 403
- Response contains: `error.code`, `error.message`, `error.status`

**Fixture:** `error_401_auth.json`

### Fixture Validation Tests

Both test files should include fixture validation tests that run without `VERIFY_MOCKS=1`:

```c
START_TEST(validate_fixture_structure)
{
    // Load each fixture and verify JSON structure matches expected schema
    // This runs in normal test mode to catch fixture corruption
}
END_TEST
```

## Makefile Updates

Add to Makefile after existing `verify-mocks` target:

```makefile
# Credential verification - checks all provider API keys are available
verify-credentials:
	@echo "Checking provider credentials..."
	@MISSING=""; \
	OPENAI_KEY="$$OPENAI_API_KEY"; \
	if [ -z "$$OPENAI_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			OPENAI_KEY=$$(jq -r '.openai.api_key // empty' "$$CONFIG_FILE" 2>/dev/null); \
		fi; \
	fi; \
	if [ -z "$$OPENAI_KEY" ]; then MISSING="$$MISSING openai"; fi; \
	ANTHROPIC_KEY="$$ANTHROPIC_API_KEY"; \
	if [ -z "$$ANTHROPIC_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			ANTHROPIC_KEY=$$(jq -r '.anthropic.api_key // empty' "$$CONFIG_FILE" 2>/dev/null); \
		fi; \
	fi; \
	if [ -z "$$ANTHROPIC_KEY" ]; then MISSING="$$MISSING anthropic"; fi; \
	GOOGLE_KEY="$$GOOGLE_API_KEY"; \
	if [ -z "$$GOOGLE_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			GOOGLE_KEY=$$(jq -r '.google.api_key // empty' "$$CONFIG_FILE" 2>/dev/null); \
		fi; \
	fi; \
	if [ -z "$$GOOGLE_KEY" ]; then MISSING="$$MISSING google"; fi; \
	if [ -n "$$MISSING" ]; then \
		echo "ERROR: Missing API keys for:$$MISSING"; \
		echo "Set environment variables or add to ~/.config/ikigai/credentials.json"; \
		exit 1; \
	fi; \
	echo "All provider credentials available"

.PHONY: verify-credentials

# Anthropic mock verification
$(BUILDDIR)/tests/integration/anthropic_mock_verification_test: \
    tests/integration/anthropic_mock_verification_test.c \
    $(BUILDDIR)/tests/test_utils.o \
    $(CLIENT_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(CLIENT_STATIC_LIBS) $(CLIENT_LIBS) -lcheck -lsubunit -lm

verify-mocks-anthropic: $(BUILDDIR)/tests/integration/anthropic_mock_verification_test
	@API_KEY="$$ANTHROPIC_API_KEY"; \
	if [ -z "$$API_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			API_KEY=$$(jq -r '.anthropic.api_key // empty' "$$CONFIG_FILE"); \
		fi; \
	fi; \
	if [ -z "$$API_KEY" ]; then \
		echo "Error: ANTHROPIC_API_KEY not found"; \
		exit 1; \
	fi; \
	echo "Running Anthropic mock verification tests..."; \
	VERIFY_MOCKS=1 ANTHROPIC_API_KEY="$$API_KEY" $<

# Google mock verification
$(BUILDDIR)/tests/integration/google_mock_verification_test: \
    tests/integration/google_mock_verification_test.c \
    $(BUILDDIR)/tests/test_utils.o \
    $(CLIENT_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(CLIENT_STATIC_LIBS) $(CLIENT_LIBS) -lcheck -lsubunit -lm

verify-mocks-google: $(BUILDDIR)/tests/integration/google_mock_verification_test
	@API_KEY="$$GOOGLE_API_KEY"; \
	if [ -z "$$API_KEY" ]; then \
		CONFIG_FILE="$$HOME/.config/ikigai/credentials.json"; \
		if [ -f "$$CONFIG_FILE" ]; then \
			API_KEY=$$(jq -r '.google.api_key // empty' "$$CONFIG_FILE"); \
		fi; \
	fi; \
	if [ -z "$$API_KEY" ]; then \
		echo "Error: GOOGLE_API_KEY not found"; \
		exit 1; \
	fi; \
	echo "Running Google mock verification tests..."; \
	VERIFY_MOCKS=1 GOOGLE_API_KEY="$$API_KEY" $<

# All provider verification
verify-mocks-all: verify-mocks verify-mocks-anthropic verify-mocks-google

.PHONY: verify-mocks-anthropic verify-mocks-google verify-mocks-all verify-credentials
```

## HTTP Client Requirements

Since provider implementations don't exist yet, verification tests must use raw HTTP:

```c
// Minimal HTTP helper for verification tests only
// Uses curl_easy (blocking is OK for verification tests - they're not in the event loop)
static char *http_post_json(TALLOC_CTX *ctx, const char *url,
                            const char *headers[], size_t num_headers,
                            const char *body, int32_t *http_status)
{
    CURL *curl = curl_easy_init();
    // ... standard curl setup ...
    // Return response body, set http_status
}

// SSE streaming helper - accumulates events
typedef struct {
    char **events;      // Array of "event: X\ndata: Y" strings
    size_t count;
    size_t capacity;
} sse_accumulator_t;

static size_t sse_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // Parse SSE format, accumulate complete events
}
```

## Test Scenarios Summary

| Provider | Test | What It Verifies | Fixture |
|----------|------|------------------|---------|
| Anthropic | streaming_text | Basic SSE event order | stream_text_basic.txt |
| Anthropic | streaming_thinking | Thinking block structure | stream_text_thinking.txt |
| Anthropic | tool_call | Tool use format, ID pattern | stream_tool_call.txt |
| Anthropic | error_auth | 401 error structure | error_401_auth.json |
| Anthropic | error_rate_limit | 429 error structure | error_429_rate_limit.json (synthetic) |
| Google | streaming_text | Chunk format, finishReason | stream_text_basic.txt |
| Google | streaming_thinking | thought flag handling | stream_text_thinking.txt |
| Google | tool_call | functionCall format | stream_tool_call.txt |
| Google | error_auth | Error response format | error_401_auth.json |

## Postconditions

- [ ] `tests/integration/anthropic_mock_verification_test.c` exists
- [ ] `tests/integration/google_mock_verification_test.c` exists
- [ ] `tests/fixtures/vcr/anthropic/` directory exists with README.md
- [ ] `tests/fixtures/vcr/google/` directory exists with README.md
- [ ] Makefile has `verify-mocks-anthropic` target
- [ ] Makefile has `verify-mocks-google` target
- [ ] Makefile has `verify-mocks-all` target
- [ ] Fixture validation tests pass without `VERIFY_MOCKS=1`
- [ ] `make check` passes (new tests skip when VERIFY_MOCKS not set)
- [ ] When `VERIFY_MOCKS=1`:
  - [ ] Anthropic tests pass with valid API key
  - [ ] Google tests pass with valid API key
- [ ] When `CAPTURE_FIXTURES=1`:
  - [ ] Real responses captured to fixture files
  - [ ] No API keys in fixtures (verify: `grep -rE '(sk-[a-zA-Z0-9]{20,}|AIza[a-zA-Z0-9]{30,})' tests/fixtures/vcr/` returns empty)
- [ ] If `make verify-credentials` fails:
  - [ ] Task exits with skip status (exit 77), not failure
  - [ ] No partial fixtures created
  - [ ] Clear message: "Run 'make verify-credentials' to diagnose"
- [ ] Changes committed to git with message: `task: verify-mocks-providers.md - <summary>`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if:
1. All postconditions are met
2. Fixture validation tests pass
3. `make check` passes

Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).

## Notes

**Security: Cassettes are response bodies only**

Cassettes capture API response bodies, not request headers. API keys should never appear in responses. The postcondition grep check catches any unexpected leaks - if it finds something, investigate the test setup rather than silently redacting.

**VCR-Style Benefits:**
- **Deterministic tests** - Same cassette, same result every time
- **Fast tests** - No network latency, no rate limits
- **Offline development** - Tests work without API keys
- **Regression detection** - Re-record cassettes to detect API changes
- **Documentation** - Cassettes ARE the API documentation

**Why verification tests use blocking HTTP:**
- Verification tests run outside the event loop (standalone test binary)
- They don't need async - they just verify API behavior
- Using curl_easy is simpler and sufficient for this purpose
- The async curl_multi pattern is tested separately via mock infrastructure

**Why capture mode is optional:**
- Real API calls cost money
- Cassettes only need updating when APIs change
- Normal test runs validate cassette structure, not API liveness

**Synthetic cassettes:**
- Some error conditions (rate limit, server error) can't be reliably triggered
- Create synthetic cassettes based on API documentation
- Document that they're synthetic in cassette file header comment

**Cassette naming convention:**
```
tests/fixtures/vcr/{provider}/
├── stream_text_basic.txt       # Basic streaming response (SSE format)
├── stream_text_thinking.txt    # Streaming with thinking enabled
├── stream_tool_call.txt        # Tool/function call response
├── response_basic.json         # Non-streaming response (if needed)
├── error_401_auth.json         # Authentication error
├── error_429_rate_limit.json   # Rate limit error (synthetic)
└── error_500_server.json       # Server error (synthetic)
```
