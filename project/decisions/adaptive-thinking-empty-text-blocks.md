# Adaptive Thinking: Empty Text Blocks Before Thinking

## Context

Claude Opus 4.6 uses adaptive thinking (`{"thinking": {"type": "adaptive"}}`) instead of budget-based thinking. With adaptive thinking, interleaved thinking is automatically enabled — thinking blocks can appear between text blocks in the response.

## Finding

The Anthropic API returns an empty text block (`"\n\n"`) before the thinking block in adaptive thinking responses:

```json
{"content": [
  {"type": "text", "text": "\n\n"},
  {"type": "thinking", "thinking": "...", "signature": "..."},
  {"type": "text", "text": "Actual response here"}
]}
```

This causes a visible empty `●` in the UI when the first LLM turn includes thinking + tool_use. The sequence looks like:

1. LLM responds with: empty text `"\n\n"` → thinking → tool_use
2. UI shows empty `●` with token count (e.g., "91 out" = thinking tokens)
3. Tool executes
4. LLM responds with actual text — displays correctly

## Root Cause

The text delta handler in `apps/ikigai/providers/anthropic/streaming_events.c:144-158` passes all text deltas through unchanged. There is no filtering for whitespace-only text content:

```c
static void process_text_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *delta_obj, int32_t index)
{
    yyjson_val *text_val = yyjson_obj_get(delta_obj, "text");
    if (text_val == NULL) return;
    const char *text = yyjson_get_str(text_val);
    if (text == NULL) return;
    // Emits event unconditionally — no whitespace check
    ik_stream_event_t event = { ... };
    sctx->stream_cb(&event, sctx->stream_ctx);
}
```

## Impact

Cosmetic only. The empty `●` appears briefly before the tool call executes. Responses complete correctly.

## Options

1. **Filter whitespace-only text deltas** in `process_text_delta()` — suppress text that is only `\n`, `\r`, or spaces. Simple but could mask legitimate whitespace in edge cases.
2. **Filter at the UI layer** — don't render a response indicator until non-whitespace text has been received. Keeps the streaming layer faithful to the API.
3. **Leave as-is** — functional behavior is correct; the empty state is brief.

## Related

- Adaptive thinking docs: https://platform.claude.com/docs/en/build-with-claude/adaptive-thinking
- Effort parameter docs: https://platform.claude.com/docs/en/build-with-claude/effort
- GitHub issue #224 — beta header support for 1M context window
