# Task: Create Google Provider Streaming Tests

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** google-streaming.md, tests-google-basic.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `src/providers/google/streaming.c` - Streaming implementation
- `tests/unit/providers/google/` - Basic tests for patterns
- `tests/helpers/mock_http.h` - Mock infrastructure

## Objective

Create tests for Google Gemini streaming response handling. Verifies JSON chunk parsing, thought part detection, and event normalization.

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/google/test_google_streaming.c` | Streaming response handling |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/google/stream_basic.txt` | Streaming response for basic completion |
| `tests/fixtures/google/stream_thinking.txt` | Streaming response with thought parts |

## Behaviors

**Streaming Format:**

Gemini uses newline-delimited JSON (not SSE):
```json
{"candidates":[{"content":{"parts":[{"text":"Hello"}]}}]}
{"candidates":[{"content":{"parts":[{"text":" world"}]}}]}
{"candidates":[{"finishReason":"STOP","content":{"parts":[{"text":"!"}]}}]}
```

**With Thinking:**
```json
{"candidates":[{"content":{"parts":[{"text":"Let me think...","thought":true}]}}]}
{"candidates":[{"content":{"parts":[{"text":"The answer is 42."}]}}]}
```

**Part Types to Handle:**

| Part Type | Normalized Event | Detection |
|-----------|------------------|-----------|
| `text` (no thought) | `IK_STREAM_CONTENT_DELTA` | `thought` field absent or false |
| `text` (thought=true) | `IK_STREAM_THINKING_DELTA` | `thought: true` in part |
| `functionCall` | `IK_STREAM_TOOL_CALL_DELTA` | `functionCall` field present |
| finish | `IK_STREAM_DONE` | `finishReason` field present |

## Test Scenarios

**Basic Streaming (4 tests):**
- Parse single text part chunk
- Parse multiple text parts in one chunk
- Parse finishReason chunk
- Accumulate text across chunks

**Thought Part Detection (4 tests):**
- Parse part with thought=true flag
- Parse part without thought flag (defaults to false)
- Distinguish thought content from regular content
- Detect thought signature in text content

**Function Call Streaming (3 tests):**
- Parse functionCall part
- Generate UUID for function call
- Parse function arguments

**Event Normalization (3 tests):**
- Normalize text part to IK_STREAM_CONTENT_DELTA
- Normalize thought part to IK_STREAM_THINKING_DELTA
- Normalize finishReason to IK_STREAM_DONE

**Error Handling (2 tests):**
- Handle malformed JSON chunk
- Handle stream interruption

## Postconditions

- [ ] 1 test file with 16+ tests
- [ ] 2 fixture files with valid streaming format
- [ ] Thought part detection verified
- [ ] Event normalization verified
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
