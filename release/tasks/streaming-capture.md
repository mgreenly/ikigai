# Task: Capture Thinking Signatures in Streaming

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns and error handling
- `/load style` - For code style conventions

**Source:**
- `src/providers/anthropic/streaming.h` - Stream context struct (lines 23-35)
- `src/providers/anthropic/streaming.c` - Context creation (lines 21-52)
- `src/providers/anthropic/streaming_events.c` - Event handlers (full file)
- `tests/unit/providers/anthropic/streaming_events_coverage_3_test.c` - Test patterns

**Plan:**
- `release/plan/thinking-signatures.md` - Sections 3, 5

## Libraries

- `yyjson` - Already used for JSON parsing
- `talloc` - Already used for memory management

Do not introduce new dependencies.

## Preconditions

- [ ] Git workspace is clean
- [ ] `types.md` task completed (signature field exists)

## Objective

1. Add thinking accumulation fields to stream context
2. Handle `signature_delta` events in content_block_delta
3. Handle `redacted_thinking` blocks in content_block_start

## Interface

### Stream Context Fields (streaming.h)

Add to `ik_anthropic_stream_ctx_t`:

| Field | Type | Purpose |
|-------|------|---------|
| `current_thinking_text` | `char *` | Accumulated thinking from thinking_delta |
| `current_thinking_signature` | `char *` | Signature from signature_delta |
| `current_redacted_data` | `char *` | Data from redacted_thinking block |

### Context Creation (streaming.c)

Initialize new fields to NULL in `ik_anthropic_stream_ctx_create`.

### Event Handlers (streaming_events.c)

**In `ik_anthropic_process_content_block_start`:**
- Handle `"redacted_thinking"` type
- Set `current_block_type = IK_CONTENT_REDACTED_THINKING`
- Extract and store `data` field to `sctx->current_redacted_data`

**In `ik_anthropic_process_content_block_delta`:**
- Add case for `"signature_delta"` delta type
- Extract `signature` field, store in `sctx->current_thinking_signature`
- Accumulate thinking_delta text in `sctx->current_thinking_text` (currently only emits event, doesn't accumulate)

**In `ik_anthropic_process_content_block_stop`:**
- No changes needed (thinking blocks don't emit DONE events currently)

## Behaviors

- `signature_delta` arrives BEFORE `content_block_stop` for thinking blocks
- `thinking_delta` events should accumulate text (like tool args accumulate)
- `redacted_thinking` data comes in `content_block_start`, not via deltas
- Memory: All strings allocated on `sctx` context

## Test Scenarios

Add tests in new file `tests/unit/providers/anthropic/streaming_signature_test.c`:

1. `test_signature_delta_captured` - Verify signature stored in context
2. `test_thinking_text_accumulated` - Verify thinking text accumulated
3. `test_redacted_thinking_captured` - Verify redacted data stored
4. `test_signature_delta_no_field` - Handle missing signature field gracefully

Follow patterns from `streaming_events_coverage_3_test.c`.

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(streaming-capture.md): success - capture thinking signatures in streaming

Added thinking accumulation fields to stream context.
Handle signature_delta and redacted_thinking in streaming events.
Added streaming_signature_test.c with coverage tests.
EOF
)"
```

Report status: `/task-done streaming-capture.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] New test file added and passing
- [ ] All changes committed
- [ ] Git workspace is clean
