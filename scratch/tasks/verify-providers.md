# Task: Verify Providers Stage

**VERIFICATION TASK:** This task does not create code. It verifies previous tasks completed correctly.

**Model:** sonnet/thinking
**Depends on:** verify-openai-shim.md, anthropic-core.md, anthropic-request.md, anthropic-response.md, anthropic-streaming.md, google-core.md, google-request.md, google-response.md, google-streaming.md, openai-core.md, openai-request-chat.md, openai-request-responses.md, openai-response-chat.md, openai-response-responses.md, openai-streaming-chat.md, openai-streaming-responses.md, openai-send-impl.md, repl-provider-routing.md, repl-streaming-updates.md

## Context

**Working directory:** Project root (where `Makefile` lives)

This verification gate ensures ALL three providers (Anthropic, Google, OpenAI native) work correctly, and REPL integration is complete. This is the largest verification gate - it validates the entire multi-provider system before cleanup begins.

If any verification step fails, return `{"ok": false, "blocked_by": "<specific failure>"}`.

## Pre-Read

Read these for architectural understanding:
- `scratch/plan/architecture.md` - Provider vtable pattern
- `scratch/plan/streaming.md` - Streaming event normalization
- `scratch/plan/transformation.md` - Request/response transformation

## Verification Protocol

### Step 1: All Provider Files Exist

**Anthropic provider:**
- [ ] `src/providers/anthropic/adapter.c` or equivalent
- [ ] `src/providers/anthropic/client.c` or `request.c`
- [ ] `src/providers/anthropic/streaming.c`
- [ ] `src/providers/anthropic/anthropic.h`

**Google provider:**
- [ ] `src/providers/google/adapter.c` or equivalent
- [ ] `src/providers/google/client.c` or `request.c`
- [ ] `src/providers/google/streaming.c`
- [ ] `src/providers/google/google.h`

**OpenAI native provider:**
- [ ] `src/providers/openai/adapter.c` (native, not shim)
- [ ] `src/providers/openai/client.c` or `request.c`
- [ ] `src/providers/openai/streaming.c`
- [ ] `src/providers/openai/openai.h`

**REPL integration:**
- [ ] Updates to `src/repl.c` or `src/repl_actions_llm.c`
- [ ] Provider routing logic exists

### Step 2: All Providers Compile

```bash
make clean && make all 2>&1
```

- [ ] Exit code 0
- [ ] No errors in provider code
- [ ] No warnings in provider code

Check for warnings specifically in providers:
```bash
make all 2>&1 | grep -E "src/providers/(anthropic|google|openai)" | grep -i warning
```
- [ ] Empty output (no warnings)

### Step 3: Provider Factory Creates All Providers

Verify factory supports all three:
```bash
grep -n "anthropic\|google\|openai" src/providers/factory.c
```

- [ ] Factory handles "anthropic"
- [ ] Factory handles "google"
- [ ] Factory handles "openai"

### Step 4: All Providers Implement Complete Vtable

For each provider, verify vtable is complete:

**Anthropic:**
```bash
grep -c "\.fdset\|\.perform\|\.timeout\|\.info_read\|\.start_request\|\.start_stream\|\.cleanup" src/providers/anthropic/*.c
```
- [ ] All 7 vtable methods assigned

**Google:**
```bash
grep -c "\.fdset\|\.perform\|\.timeout\|\.info_read\|\.start_request\|\.start_stream\|\.cleanup" src/providers/google/*.c
```
- [ ] All 7 vtable methods assigned

**OpenAI native:**
```bash
grep -c "\.fdset\|\.perform\|\.timeout\|\.info_read\|\.start_request\|\.start_stream\|\.cleanup" src/providers/openai/*.c
```
- [ ] All 7 vtable methods assigned

### Step 5: Async Pattern Compliance (All Providers)

Check for blocking patterns in ALL providers:
```bash
grep -rn "curl_easy_perform\|sleep\s*(\|usleep\s*(" src/providers/anthropic/ src/providers/google/ src/providers/openai/
```

- [ ] No blocking calls found (empty output)

Verify curl_multi usage:
```bash
grep -rn "curl_multi\|http_multi" src/providers/
```
- [ ] All providers use async HTTP client

### Step 6: Request Serialization Tests Pass

Run request serialization tests for each provider:

**Anthropic:**
```bash
make build/tests/unit/providers/anthropic/request_test && ./build/tests/unit/providers/anthropic/request_test
```
- [ ] All tests pass

**Google:**
```bash
make build/tests/unit/providers/google/request_test && ./build/tests/unit/providers/google/request_test
```
- [ ] All tests pass

**OpenAI:**
```bash
make build/tests/unit/providers/openai/request_test && ./build/tests/unit/providers/openai/request_test
```
- [ ] All tests pass

### Step 7: Response Parsing Tests Pass

Run response parsing tests for each provider:

**Anthropic:**
```bash
make build/tests/unit/providers/anthropic/response_test && ./build/tests/unit/providers/anthropic/response_test
```
- [ ] All tests pass

**Google:**
```bash
make build/tests/unit/providers/google/response_test && ./build/tests/unit/providers/google/response_test
```
- [ ] All tests pass

**OpenAI:**
```bash
make build/tests/unit/providers/openai/response_test && ./build/tests/unit/providers/openai/response_test
```
- [ ] All tests pass

### Step 8: Streaming Tests Pass

Run streaming tests for each provider:

**Anthropic:**
```bash
make build/tests/unit/providers/anthropic/streaming_test && ./build/tests/unit/providers/anthropic/streaming_test
```
- [ ] All tests pass

**Google:**
```bash
make build/tests/unit/providers/google/streaming_test && ./build/tests/unit/providers/google/streaming_test
```
- [ ] All tests pass

**OpenAI:**
```bash
make build/tests/unit/providers/openai/streaming_test && ./build/tests/unit/providers/openai/streaming_test
```
- [ ] All tests pass

### Step 9: Normalized Event Types

Verify all providers emit normalized stream events:

For each provider, check streaming code uses normalized types:
```bash
grep -rn "IK_STREAM_START\|IK_STREAM_TEXT_DELTA\|IK_STREAM_TOOL_CALL\|IK_STREAM_DONE" src/providers/
```

- [ ] Anthropic uses IK_STREAM_* types
- [ ] Google uses IK_STREAM_* types
- [ ] OpenAI uses IK_STREAM_* types
- [ ] No provider-specific event types leak through

### Step 10: Error Category Mapping

Verify all providers map errors to normalized categories:

```bash
grep -rn "IK_ERR_AUTH\|IK_ERR_RATE_LIMIT\|IK_ERR_SERVER" src/providers/
```

- [ ] Anthropic maps HTTP errors to IK_ERR_* categories
- [ ] Google maps HTTP errors to IK_ERR_* categories
- [ ] OpenAI maps HTTP errors to IK_ERR_* categories

### Step 11: Thinking Level Support

Verify providers handle thinking configuration:

**Anthropic (extended thinking):**
```bash
grep -n "thinking\|budget\|IK_THINKING" src/providers/anthropic/*.c
```
- [ ] Thinking level is transformed to Anthropic format

**Google (thinking budget/level):**
```bash
grep -n "thinking\|thinkingBudget\|thinkingLevel" src/providers/google/*.c
```
- [ ] Thinking level is transformed to Google format

**OpenAI (reasoning effort):**
```bash
grep -n "reasoning\|effort\|IK_THINKING" src/providers/openai/*.c
```
- [ ] Thinking level is transformed to OpenAI format (for o-series models)

### Step 12: REPL Provider Routing

Verify REPL routes through provider abstraction:

```bash
grep -n "provider->vt->\|vt->start_stream\|vt->fdset\|vt->perform" src/repl*.c
```

- [ ] REPL calls provider vtable methods
- [ ] REPL does NOT call legacy ik_openai_* functions directly

Verify old OpenAI calls are removed:
```bash
grep -n "ik_openai_client\|ik_openai_multi_add_request" src/repl*.c
```
- [ ] No direct calls to legacy OpenAI functions (empty output)

### Step 13: REPL Event Loop Integration

Verify REPL integrates with provider event loop:

```bash
grep -A 20 "select\|fdset" src/repl.c | head -40
```

Look for pattern:
- [ ] REPL calls provider->vt->fdset() before select()
- [ ] REPL calls provider->vt->perform() after select()
- [ ] REPL calls provider->vt->info_read() for completions
- [ ] REPL calls provider->vt->timeout() for select timeout

### Step 14: REPL Streaming Callback

Verify REPL handles normalized streaming events:

```bash
grep -n "IK_STREAM_TEXT_DELTA\|IK_STREAM_TOOL_CALL\|IK_STREAM_DONE" src/repl*.c
```

- [ ] REPL has cases for all stream event types
- [ ] REPL updates scrollback on text delta
- [ ] REPL handles tool calls
- [ ] REPL finalizes on stream done

### Step 15: Provider Switching Tests

Run provider switching tests:
```bash
make build/tests/unit/providers/switching_test 2>/dev/null && ./build/tests/unit/providers/switching_test
```

Or look for switching tests in integration:
```bash
grep -rn "switch.*provider\|change.*provider" tests/
```

- [ ] Tests exist for switching providers mid-session
- [ ] Tests verify message history is preserved

### Step 16: Full Test Suite

Run complete test suite:
```bash
make check 2>&1 | tail -20
```

- [ ] All tests pass
- [ ] No failures in provider tests
- [ ] No failures in REPL tests

Count test results:
```bash
make check 2>&1 | grep -c "PASS"
```
- [ ] Significant number of passing tests (should be 100+)

### Step 17: Memory Leak Check

Run valgrind on provider tests:
```bash
make BUILD=valgrind check-unit 2>&1 | grep -E "definitely lost|providers"
```

- [ ] No memory leaks in provider code
- [ ] No memory leaks in REPL integration

### Step 18: Cross-Provider Consistency

Verify all providers produce consistent output format:

Read response parsing code for each provider and verify:
- [ ] All produce `ik_response_t` with same structure
- [ ] All populate content blocks the same way
- [ ] All set finish_reason using same enum
- [ ] All populate usage statistics

### Step 19: Tool Call Handling

Verify all providers handle tool calls:

```bash
grep -rn "tool_call\|TOOL_CALL\|function_call" src/providers/
```

- [ ] Anthropic parses tool_use blocks
- [ ] Google parses functionCall parts
- [ ] OpenAI parses tool_calls array
- [ ] All produce normalized `ik_content_block_t` with TOOL_CALL type

### Step 20: Cross-Task Contract Verification

Verify provider implementation meets downstream expectations:

**From `openai-equivalence-validation.md`:**
- Native OpenAI provider must produce same results as shim
- Verify: Equivalence tests pass (or are ready to run)

**From `cleanup-openai-source.md`:**
- Expects native OpenAI to fully replace shim
- Verify: Native provider implements all shim functionality

**From `tests-integration-flows.md`:**
- Expects complete request/response cycle through vtable
- Verify: REPL integration tests pass

## Postconditions

- [ ] All 20 verification steps pass
- [ ] All three providers compile without warnings
- [ ] All three providers implement complete vtable
- [ ] All providers maintain async behavior
- [ ] All provider tests pass
- [ ] REPL routes through provider abstraction
- [ ] REPL event loop integrates with providers
- [ ] No memory leaks
- [ ] Providers produce consistent output format

## Success Criteria

Return `{"ok": true}` if ALL verification steps pass.

Return `{"ok": false, "blocked_by": "<specific failure>"}` if any step fails.

**Example failures:**

```json
{
  "ok": false,
  "blocked_by": "Step 4: Vtable Completeness - Google provider missing 'info_read' method. Found methods: fdset, perform, timeout, start_request, start_stream, cleanup",
  "affected_downstream": ["tests-google-streaming.md", "cleanup tasks"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 12: REPL Routing - Found direct call to ik_openai_multi_add_request() in src/repl_actions_llm.c:245. REPL must route through provider vtable.",
  "affected_downstream": ["Multi-provider support broken"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 9: Event Normalization - Anthropic streaming uses 'ANTHROPIC_TEXT_DELTA' instead of 'IK_STREAM_TEXT_DELTA'. Provider-specific types leaking through.",
  "affected_downstream": ["repl-streaming-updates.md", "tests-anthropic-streaming.md"]
}
```
