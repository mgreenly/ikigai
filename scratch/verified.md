# Verified Gaps

Gaps that have been reviewed and fixed. Do not re-investigate these.

## Fixed

### 2024-12-22: SSE Parser API Mismatch (CRITICAL)

**Location:** `scratch/tasks/sse-parser.md`

**Problem:** SSE parser task defined a pull-based API (`ik_sse_parser_next()`) but streaming tasks (anthropic-streaming.md, google-streaming.md, openai-streaming-*.md) incorrectly assumed a push-based/callback API where the parser would invoke callbacks.

**Fix:** Added "Callback Integration Pattern" section to sse-parser.md clarifying:
- API is PULL-based, not callback-based
- Parser accumulates data and provides events on demand
- Caller is responsible for the extraction loop
- Includes complete code example of curl write callback integration

**Impact:** Streaming implementations now have clear guidance on how to integrate with the SSE parser using the correct pull-based pattern.
