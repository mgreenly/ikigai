# Task: Create OpenAI Provider Streaming Tests

**Layer:** 5
**Model:** sonnet/thinking
**Depends on:** openai-streaming-chat.md, tests-openai-basic.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `src/providers/openai/streaming.c` - Streaming implementation
- `tests/unit/providers/openai/` - Basic tests for patterns
- `tests/helpers/mock_http.h` - Mock infrastructure

## Objective

Create tests for OpenAI streaming event handling. Verifies SSE parsing, delta accumulation, tool call argument streaming, and event normalization.

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/openai/test_openai_streaming.c` | SSE streaming response handling |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/openai/stream_basic.txt` | SSE stream for basic completion |
| `tests/fixtures/openai/stream_reasoning.txt` | SSE stream from reasoning model |

## Behaviors

**SSE Format:**
```
data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{"content":" world"},"finish_reason":null}]}

data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]
```

**Tool Call Streaming:**
```
data: {"choices":[{"delta":{"tool_calls":[{"index":0,"id":"call_abc","type":"function","function":{"name":"get_weather","arguments":""}}]}}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"lo"}}]}}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"cation\":"}}]}}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\"NYC\"}"}}]}}]}
```

**Delta Types to Handle:**

| Delta Content | Normalized Event |
|---------------|------------------|
| `content` string | `IK_STREAM_CONTENT_DELTA` |
| `tool_calls` array | `IK_STREAM_TOOL_CALL_DELTA` |
| `finish_reason` set | `IK_STREAM_DONE` |
| `[DONE]` marker | Stream end signal |

## Test Scenarios

**Basic Streaming (4 tests):**
- Parse initial role delta
- Parse content delta
- Parse finish_reason delta
- Parse [DONE] marker

**Content Accumulation (3 tests):**
- Accumulate multiple content deltas
- Handle empty content deltas
- Preserve content order

**Tool Call Streaming (5 tests):**
- Parse tool_calls delta with id and name
- Parse tool_calls delta with partial arguments
- Accumulate arguments across multiple deltas
- Handle multiple tool calls (by index)
- Complete tool call with full arguments JSON

**Event Normalization (3 tests):**
- Normalize content delta to IK_STREAM_CONTENT_DELTA
- Normalize tool_calls delta to IK_STREAM_TOOL_CALL_DELTA
- Normalize finish_reason to IK_STREAM_DONE

**Error Handling (2 tests):**
- Handle malformed SSE data
- Handle stream interruption

## Postconditions

- [ ] 1 test file with 17+ tests
- [ ] 2 fixture files with valid SSE format
- [ ] Tool call argument accumulation verified
- [ ] Event normalization verified
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
