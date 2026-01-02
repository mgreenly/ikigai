# Task: Include Thinking Blocks in Response Builder

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** streaming-capture.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns
- `/load style` - For code style conventions

**Source:**
- `src/providers/anthropic/streaming.c` - Response builder (lines 122-166)
- `src/providers/anthropic/streaming.h` - Stream context struct
- `tests/unit/providers/anthropic/streaming_response_builder_test.c` - Test patterns

**Plan:**
- `release/plan/thinking-signatures.md` - Section 6

## Libraries

- `talloc` - Already used for memory management

Do not introduce new dependencies.

## Preconditions

- [ ] Git workspace is clean
- [ ] `streaming-capture.md` task completed (accumulation fields exist)

## Objective

Update `ik_anthropic_stream_build_response` to include thinking and redacted_thinking blocks in the response content.

## Interface

**Function:** `ik_anthropic_stream_build_response`

**Current behavior:** Only includes tool_call block if present.

**New behavior:** Build content_blocks array containing (in order):
1. Thinking block (if `sctx->current_thinking_text` is non-NULL)
2. Redacted thinking block (if `sctx->current_redacted_data` is non-NULL)
3. Tool call block (if `sctx->current_tool_id` is non-NULL)

## Behaviors

- Anthropic returns thinking blocks BEFORE tool_use blocks - preserve this order
- A response may have: just thinking, thinking + tool_use, or just tool_use
- Redacted thinking replaces regular thinking (they don't coexist)
- Count content blocks dynamically based on what's present
- Use `talloc_array` for content_blocks with correct count
- Copy strings with `talloc_strdup` on `resp->content_blocks` context

### Thinking Block Population

```c
block->type = IK_CONTENT_THINKING;
block->data.thinking.text = talloc_strdup(..., sctx->current_thinking_text);
block->data.thinking.signature = talloc_strdup(..., sctx->current_thinking_signature);
```

### Redacted Thinking Block Population

```c
block->type = IK_CONTENT_REDACTED_THINKING;
block->data.redacted_thinking.data = talloc_strdup(..., sctx->current_redacted_data);
```

## Test Scenarios

Add tests to `streaming_response_builder_test.c` or new file:

1. `test_response_with_thinking_block` - Verify thinking block included with signature
2. `test_response_with_redacted_thinking` - Verify redacted_thinking block included
3. `test_response_thinking_and_tool_call` - Verify both present, thinking first
4. `test_response_only_tool_call` - Existing behavior preserved

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(response-builder.md): success - include thinking blocks in response

ik_anthropic_stream_build_response now includes thinking and
redacted_thinking blocks with signatures in response content.
EOF
)"
```

Report status: `/task-done response-builder.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
