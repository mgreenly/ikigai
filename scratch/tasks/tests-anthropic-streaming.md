# Task: Create Anthropic Provider Streaming Tests

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** anthropic-streaming.md, tests-anthropic-basic.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `src/providers/anthropic/streaming.c` - Streaming implementation
- `tests/unit/providers/anthropic/` - Basic tests for patterns
- `tests/helpers/mock_http.h` - Mock infrastructure

## Objective

Create tests for Anthropic streaming event handling. Verifies SSE parsing, event normalization, delta accumulation, and thinking token calculation.

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/anthropic/test_anthropic_streaming.c` | SSE event handling and normalization |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/anthropic/stream_basic.txt` | SSE stream for basic completion |
| `tests/fixtures/anthropic/stream_thinking.txt` | SSE stream with thinking deltas |

## Behaviors

**Event Types to Test:**

| Anthropic Event | Normalized Event | Handler |
|-----------------|------------------|---------|
| `message_start` | - | Initialize message state |
| `content_block_start` | `IK_STREAM_CONTENT_START` | Start content block |
| `content_block_delta` | `IK_STREAM_CONTENT_DELTA` | Append text delta |
| `content_block_delta` (thinking) | `IK_STREAM_THINKING_DELTA` | Append thinking delta |
| `content_block_stop` | - | Finalize content block |
| `message_delta` | - | Extract usage metadata |
| `message_stop` | `IK_STREAM_DONE` | Finalize message |

**SSE Format:**
```
event: message_start
data: {"type":"message_start","message":{"id":"msg_...",...}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":15}}

event: message_stop
data: {"type":"message_stop"}
```

## Test Scenarios

**Basic Streaming (4 tests):**
- Parse message_start event
- Parse content_block_start event
- Parse content_block_delta with text
- Parse message_stop event

**Content Accumulation (3 tests):**
- Accumulate multiple text deltas
- Handle multiple content blocks in sequence
- Preserve content block order

**Thinking Content (3 tests):**
- Parse thinking content_block_start
- Parse thinking content_block_delta
- Calculate thinking tokens from usage

**Tool Call Streaming (3 tests):**
- Parse tool_use content_block_start
- Parse tool_use content_block_delta with partial JSON
- Accumulate tool call arguments

**Event Normalization (3 tests):**
- Normalize text delta to IK_STREAM_CONTENT_DELTA
- Normalize thinking delta to IK_STREAM_THINKING_DELTA
- Normalize message_stop to IK_STREAM_DONE

**Error Handling (2 tests):**
- Handle malformed SSE gracefully
- Handle stream interruption

## Postconditions

- [ ] 1 test file with 18+ tests
- [ ] 2 fixture files with valid SSE format
- [ ] All event types tested
- [ ] Thinking budget calculation verified
- [ ] Event normalization verified
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
