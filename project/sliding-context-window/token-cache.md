# Token Cache Module — Design Document

**Feature**: Token counting cache for sliding context window
**Status**: Core integration complete (goals #262, #280, #274). Standalone module, IDLE transition integration, and `/model` invalidation are wired and tested. Remaining: `/clear` reset, `/rewind` trim, horizontal rule in scrollback.
**Related**: `project/sliding-context-window.md` (parent design), `anthropic-token-api.md`, `openai-token-api.md`, `google-token-api.md`

**Implementation files**:
- `apps/ikigai/token_cache.h` — Public interface
- `apps/ikigai/token_cache.c` — Implementation
- `tests/unit/apps/ikigai/token_cache_test.c` — Unit tests (25 tests, all passing)

---

## Overview

The token cache is a small module that tracks the token cost of each component in the LLM request context. All token count reads go through a single getter interface. If a cached value exists, it's returned immediately. If not, the module calls the current provider's countTokens API, caches the result, and returns it. Invalidation clears cached values so the next read recomputes them.

This design makes countTokens the primary path for all token counting. The bytes-based fallback estimator (`apps/ikigai/token_count.c`) is retained as an internal fallback inside the cache getters — used when provider APIs fail due to rate limits, network errors, or offline conditions. See OQ-2 for the resolution that kept the bytes estimator as an internal detail rather than eliminating it.

---

## Cached Components

The request context decomposes into three kinds of components, each cached independently:

| Component | Source | How cost is determined |
|-----------|--------|----------------------|
| System prompt | Pinned docs, config fallback | countTokens API (on first access or after invalidation) |
| Tool definitions | Tool registry | countTokens API (on first access or after invalidation) |
| Turns | `agent->messages[]` grouped by user message boundaries | `response_input_tokens` delta during normal operation; countTokens API after invalidation |

**Total context tokens** = system prompt tokens + tool tokens + sum of all turn tokens.

The small per-request fixed overhead (reply priming, formatting) is ignored. On a 100K budget this is noise. If desired, a small constant can be added as a conservative buffer.

---

## Turn Identification

A turn starts at each `IK_ROLE_USER` message in `agent->messages[]` and includes all subsequent messages up to (but not including) the next `IK_ROLE_USER` message. This is a structural invariant: the user cannot send a message while the agent is working, so every user message is a turn boundary.

The messages array contains only conversation-kind messages (`IK_ROLE_USER`, `IK_ROLE_ASSISTANT`, `IK_ROLE_TOOL`). System messages and metadata kinds are never in the array. System prompt is a separate field on the request.

---

## API

```c
typedef struct ik_token_cache ik_token_cache_t;

// Lifecycle — cache holds a reference to the agent for message access and provider dispatch
ik_token_cache_t *ik_token_cache_create(TALLOC_CTX *ctx, ik_agent_t *agent);
ik_token_cache_t *ik_token_cache_clone(TALLOC_CTX *ctx, const ik_token_cache_t *src, ik_agent_t *new_agent);

// Getters — return cached value or compute via countTokens (bytes fallback on failure)
// All turn_index parameters assert turn_index < turn_count (PANIC on out-of-range).
int32_t  ik_token_cache_get_total(ik_token_cache_t *cache);
int32_t  ik_token_cache_get_system_tokens(ik_token_cache_t *cache);
int32_t  ik_token_cache_get_tool_tokens(ik_token_cache_t *cache);
int32_t  ik_token_cache_get_turn_tokens(ik_token_cache_t *cache, size_t turn_index);

// Record — store a known token count (called after turn completion)
// Asserts turn_index < turn_count.
void     ik_token_cache_record_turn(ik_token_cache_t *cache, size_t turn_index, int32_t tokens);

// Invalidate — clear cached values so next getter call recomputes
void     ik_token_cache_invalidate_all(ik_token_cache_t *cache);
void     ik_token_cache_invalidate_system(ik_token_cache_t *cache);
void     ik_token_cache_invalidate_tools(ik_token_cache_t *cache);

// Structural — update cache when turns are added or removed
void     ik_token_cache_prune_oldest_turn(ik_token_cache_t *cache);
void     ik_token_cache_add_turn(ik_token_cache_t *cache);
```

**The getter is the only read path.** All callers access token counts through the getter. The getter checks the cache, computes if missing via countTokens API, stores the result, and returns it. No caller can bypass the cache or trigger redundant computation.

**Two-layer caching.** Per-component values (system, tools, individual turns) are the first layer. `get_total()` is the second layer — a cached sum of whatever the per-component getters returned. Per-component values are cached only when they come from the countTokens API; uncached entries use the sentinel value `-1`. If the API fails, the getter returns a bytes estimate to the caller but does not cache it (entry stays `-1`) — the next call retries the API. `get_total()` caches the sum regardless of how individual components were computed, and is invalidated when any component changes (turn added, turn pruned, `/model`, pin change, etc.).

**Invalidation functions are the only write path that clears values.** `invalidate_all()` clears everything (system, tools, all turns, cached total). `invalidate_system()` and `invalidate_tools()` clear only their component and the cached total. The next getter call recomputes.

---

## Normal Operation Flow

1. User sends a message, LLM responds (possibly with tool calls), turn completes
2. At IDLE transition, `response_input_tokens` is available from the provider response (free, included in every response)
3. **Delta** = `response_input_tokens` - previous total
4. If delta ≤ 0, use bytes estimate for the new turn's cost instead
5. Call `ik_token_cache_record_turn()` with the turn cost
6. Update the stored total
7. **Budget check**: if total ≤ budget, done — no per-turn counts needed
8. If over budget, prune (see below)

No API calls during normal operation. The provider already told us the total with `response_input_tokens`, and the delta gives us the per-turn cost exactly. The `delta ≤ 0` guard handles provider errors (returning 0 or -1) and edge cases where the reported total is less than the previous total (rounding, cached responses).

### After `/model`

`/model` invalidates all cached values and immediately runs the prune loop. The first `get_total()` call walks all turns, recomputing each against the new provider (countTokens API where possible, bytes estimate where the API fails). Per-component API results are cached; bytes estimates are not (they'll retry on the next call). The total is cached as the sum. `prune_oldest_turn()` subtracts the pruned turn's cost from the cached total — subsequent loop iterations are pure arithmetic with no API calls or re-walking.

---

## Pruning Flow

```
while (ik_token_cache_get_total(cache) > budget && turn_count > 1) {
    ik_token_cache_prune_oldest_turn(cache);
}
```

**Pruning is atomic.** `prune_oldest_turn()` removes the oldest turn from both the agent's message array and the cache entry in a single operation. The cache holds an agent reference (passed at creation), so it can reach the messages directly. No caller needs to coordinate two separate calls — the cache and agent are always consistent.

**`get_total()` is computed once per prune session.** The first call walks all turns and caches the sum. `prune_oldest_turn()` subtracts the pruned turn's cost from the cached total. Subsequent `get_total()` calls in the loop return the cached sum — no re-walking turns, no re-requesting from the API. The loop is pure arithmetic after the first `get_total()`.

**Per-component values may be API or estimated.** When `get_total()` walks turns to compute the sum, each `get_turn_tokens()` tries the countTokens API first. If the API succeeds, the value is cached (persists across calls). If the API fails (rate limit, network error), a bytes estimate is returned but not cached — the next call retries the API. This means some turns may have exact counts and others approximate counts within a single prune session. The bytes estimate is conservative (overcounts slightly), so pruning errs toward freeing more space, which is the safe direction.

After pruning, `agent->context_start_index` is updated to the oldest message still in context. The scrollback renderer draws a horizontal rule (`── context ──`) before this message when it's within the visible viewport. Pruned messages remain in the scrollback for the user to scroll back through — they're just no longer sent to the LLM. Moving the boundary is a single integer assignment, not a buffer operation.

---

## Invalidation Events

| Event | Action | Rationale |
|-------|--------|-----------|
| `/model` | `invalidate_all()` + prune | Different tokenizer, all counts meaningless; re-prune immediately |
| `/clear` | Reset cache (zero turns, zero total) | Messages array is empty |
| `/rewind` | Remove invalidated turn entries | Turns after the mark are gone; remaining turns' counts are unchanged |
| Pin/unpin document | `invalidate_system()` | System prompt content changed; turn and tool counts unaffected |
| Tool registry change | `invalidate_tools()` | Tool definitions changed; turn and system counts unaffected |

**`/clear` and `/rewind` do not require API calls.** `/clear` resets to empty. `/rewind` removes turn entries and their counts — remaining cached counts are still valid.

**`/model` invalidation is immediate.** All per-component counts are cleared and the prune loop runs right away. The first `get_total()` walks all turns, recomputing each against the new provider's countTokens API (bytes estimate fallback on failure). API results are cached; bytes estimates are not. The cached total is the sum. Subsequent prune iterations subtract from the cached total — pure arithmetic, no re-walking. Turns that were bytes-estimated retry the API on the next IDLE transition.

---

## Provider Vtable Entry

Token counting is dispatched through the existing provider vtable. A new `count_tokens` entry is added alongside the existing `start_stream`, `start_request`, etc.:

```c
struct ik_provider_vtable {
    // ... existing methods (fdset, perform, timeout, info_read,
    //     start_request, start_stream, cleanup, cancel) ...

    /**
     * count_tokens - Count input tokens for a request
     *
     * @param ctx             Provider context (opaque)
     * @param req             Request to count (same format as start_request/start_stream)
     * @param token_count_out Output: number of input tokens
     * @return                OK(NULL) on success, ERR(...) on failure
     *
     * Synchronous blocking call. Only called at IDLE transition when
     * nothing else is in flight. Each provider hits its own endpoint:
     *   - Anthropic: POST /v1/messages/count_tokens
     *   - OpenAI:    POST /v1/responses/input_tokens
     *   - Google:    POST models/{model}:countTokens
     */
    res_t (*count_tokens)(void *ctx,
                          const ik_request_t *req,
                          int32_t *token_count_out);
};
```

Each provider implements `count_tokens` against its own endpoint. The token cache module calls it through `provider->vt->count_tokens()` without knowing which provider it's talking to.

**Synchronous, not async.** The existing async pattern (fdset/perform/info_read) is for the streaming event loop during active LLM requests. `count_tokens` runs at IDLE transition — the user is waiting for the prompt, nothing else is in flight. A simple blocking HTTP call is appropriate. The server-side latency is single-digit milliseconds; network round-trip dominates.

**Request reuse.** The `ik_request_t` passed to `count_tokens` is the same structure used for inference calls. For counting a single turn's messages, a minimal request is built containing just that turn's messages. For counting the full context, the existing `ik_request_build_from_conversation()` output can be used directly.

---

## Computing a Turn's Token Count

When a turn has no cached count and the getter is called, the module builds a minimal `ik_request_t` containing just that turn's messages (user message plus all assistant/tool messages until the next user message) and calls `provider->vt->count_tokens()`.

The resulting count captures the token cost of those messages including per-message formatting overhead. The small inaccuracy from not including request-level fixed overhead (reply priming) is acceptable — it amounts to a few tokens and we're working against a 100K budget ceiling.

---

## Integration Points

| File | Integration |
|------|-------------|
| `providers/provider_vtable.h` | `count_tokens` entry added (**done**, goal #256) |
| `providers/anthropic/count_tokens.c` | `count_tokens` → `POST /v1/messages/count_tokens` (**done**, goal #257) |
| `providers/openai/count_tokens.c` | `count_tokens` → `POST /v1/responses/input_tokens` (**done**, goal #258) |
| `providers/google/count_tokens.c` | `count_tokens` → `POST models/{model}:countTokens` (**done**, goal #259) |
| `agent_state.c` | `WAITING_FOR_LLM → IDLE` transition: record turn cost from `response_input_tokens` delta, run prune loop (**done**, goal #280) |
| `repl_response_helpers.c` | Response metadata storage — where `response_input_tokens` is populated |
| `commands_basic.c` | `/clear`: reset cache |
| `commands_mark.c` | `/rewind`: remove pruned turn entries from cache |
| `commands_model.c` | `/model`: call `invalidate_all()`, run prune loop |
| `agent.h` | Agent struct holds `ik_token_cache_t *` pointer (**done**, goal #280) |
| `config.h` | `sliding_context_tokens` budget field (**done**, goal #280) |

---

## What This Replaces

The original design doc (`project/sliding-context-window.md`) specified:

- A bytes-based fallback estimator (~4 bytes/token)
- Provider countTokens APIs
- The pruning loop serializing the full request and counting on every iteration

This design makes countTokens the primary path and the bytes-based estimator an internal fallback (not exposed to callers). The bytes estimator is **not eliminated** — it is retained inside the cache getter as a circuit breaker when provider APIs are unavailable (OQ-2 resolution). The pruning loop becomes arithmetic over cached values in the common case.

---

## Dependencies

The token cache module needs:

- **Provider vtable** — `provider->vt->count_tokens()` for lazy recomputation
- **`ik_request_t` construction** — to build minimal requests for individual turn counting (reuses existing request infrastructure)
- **`response_input_tokens`** — available on `agent` after each LLM response, used to derive per-turn costs during normal operation

The module does NOT need:

- A tokenizer library or vocabulary files
- Offline token counting capability
- Knowledge of which provider is active (the vtable handles dispatch)
- Per-component breakdown from the countTokens API (it only returns totals, which is fine — we count individual turns by sending just their messages)

---

## Open Questions

Gaps identified by codebase audit. Must be resolved before implementation.

### Critical

**OQ-1: Error propagation — getters return int32_t but countTokens can fail.**
The vtable `count_tokens()` returns `res_t`. The cache getters return bare `int32_t`. When the API fails (network, rate limit, auth), the getter has no way to signal failure.

*Proposed resolution*: Keep `int32_t` getters. The cache absorbs errors internally by falling back to a shared bytes-based estimator (~4 bytes/token). Callers never see errors. The cache is the trust boundary — it validates at the external API edge and presents clean internal data. A log message fires when fallback activates so operational problems are visible.

**OQ-2: Shared bytes estimation module required.**
The bytes-based fallback resolves OQ-1 (error absorption), OQ-5 (thundering herd), and the `/model` first-turn problem. This is a new dependency not in the original design — a shared module that all three providers' countTokens fallback paths can use. The ~4 bytes/token ratio the provider API docs reference, applied to serialized message content. Conservative (overcounts slightly), which is the safe direction for budget enforcement.

*Impact*: The "What This Replaces" section claims the bytes estimator is eliminated entirely. That's no longer true — it becomes an internal fallback behind the cache getter, not a primary interface.

**OQ-3: ~~System prompt and tool token counts have no API surface.~~ RESOLVED.**
Added `get_system_tokens()`, `get_tool_tokens()`, `invalidate_system()`, `invalidate_tools()` to the API. Invalidation events table updated with granular functions.

**OQ-4: ~~Provider reference missing from cache API.~~ RESOLVED.**
`ik_token_cache_create()` now takes an `ik_agent_t *agent` parameter. The cache holds this reference for the duration of its lifetime, giving it access to both `agent->messages[]` and the provider vtable via `agent->provider->vt->count_tokens()`. This also resolves OQ-8 (atomic pruning).

### High

**OQ-5: ~~Lazy recomputation is a thundering herd after `/model`.~~ RESOLVED.**
`/model` now invalidates all values and immediately runs the prune loop. The first `get_total()` walks turns once, trying countTokens API for each (bytes fallback on failure). API results are cached; bytes estimates are not. The cached total is the sum. `prune_oldest_turn()` subtracts from the cached total — subsequent loop iterations are pure arithmetic with no re-walking or re-requesting. Rate-limited turns retry on the next IDLE transition.

**OQ-6: ~~Mixed-tokenizer delta after `/model` is broken.~~ RESOLVED.**
`/model` no longer defers to the next turn's delta. It invalidates and re-prunes immediately, recomputing all turn costs against the new provider. The cached total is fresh for the new tokenizer. The next turn's delta computes correctly because `previous_total` reflects the new tokenizer's counts.

**OQ-7: ~~Delta calculation has no validation.~~ RESOLVED.**
One rule: if `delta <= 0`, use bytes estimate for the turn cost. Handles provider errors (0, -1) and edge cases where reported total is less than previous total. Normal Operation Flow updated with the guard.

**OQ-8: ~~Non-atomic pruning can desync cache and agent.~~ RESOLVED.**
Replaced `remove_oldest_turn(cache)` + `remove_oldest_turn(agent)` with a single `ik_token_cache_prune_oldest_turn(cache)` that atomically removes from both the message array and the cache. The cache holds an agent reference (OQ-4), so it owns the entire prune operation.

### Medium

**OQ-9: ~~Sentinel value ambiguity.~~ RESOLVED.**
Sentinel value is `-1`. Uncached entries store `-1`; the getter checks for this and recomputes. Two-layer caching section updated.

**OQ-10: ~~`repl_tool_completion.c` is misleading as integration point.~~ RESOLVED.**
Integration table updated: `agent_state.c` for the `WAITING_FOR_LLM → IDLE` transition, `repl_response_helpers.c` for response metadata. `repl_tool_completion.c` removed from table.

**OQ-11: ~~Partial invalidation path unclear.~~ RESOLVED.**
Already resolved by OQ-3 — `invalidate_system()` and `invalidate_tools()` provide granular invalidation. `invalidate_all()` used only for `/model`.

**OQ-12: ~~No bounds checking on turn_index.~~ RESOLVED.**
All functions taking `turn_index` assert `turn_index < turn_count` and PANIC on out-of-range. Out-of-range is a caller bug, not a runtime condition.

**OQ-13: ~~Horizontal rule after pruning doesn't exist.~~ RESOLVED.**
Store `context_start_index` on the agent — the oldest message still in context. The scrollback renderer draws an HR (`── context ──`) before that message when it's in the viewport. Pruned messages stay in scrollback for user review. Moving the boundary is one integer assignment. No event insertion, no mark manipulation, no buffer operations.

**OQ-14: ~~Fork behavior unspecified.~~ RESOLVED.**
Child agents always inherit parent messages. Two cases, both clone the cache via `ik_token_cache_clone()`: pure fork clones as-is (all values valid); prompted child clones then calls `invalidate_system()` (per-turn counts still valid, only system prompt changed).

**OQ-15: Threading contract implicit.**
Ikigai has tool threads (`pthread_mutex_t tool_thread_mutex`). The design doesn't state that the cache is single-threaded and IDLE-only. Agent fields are main-thread-only by convention (`agent.h` comment), but the cache should state this explicitly.

---

## Implementation Notes (goal #262)

### Deviations from design

**`prune_oldest_turn` does not modify `agent->messages[]`.**
The design says pruning is atomic and the module removes from both message array and cache. However, it also says "pruned messages remain in the scrollback". For this standalone implementation, `prune_oldest_turn` advances an internal `context_start_index` field (stored in the cache struct, not on the agent) to track which messages are still in context. The agent's `context_start_index` field referenced in OQ-13 is not yet on the agent struct — that integration is deferred to a follow-up goal.

**`get_tool_tokens` counts without tool definitions.**
The cache has no access to the tool registry (not in `ik_agent_ctx_t`). `get_tool_tokens()` calls `count_tokens` with an empty request (model-only). This gives an accurate per-turn count but underestimates tool overhead. Full tool-definition counting requires the tool registry to be accessible from the cache — deferred to integration work.

**`ik_agent_get_effective_system_prompt` always returns a non-NULL default.**
Priority 4 in the function returns `IK_DEFAULT_OPENAI_SYSTEM_MESSAGE` ("You are a personal agent...") as a hardcoded fallback. The bytes estimate for this (~21 tokens) appears in system token counts for test agents without provider instances.

**Threading contract** (OQ-15) is documented in `token_cache.h` header comment: single-threaded, main-thread-only.
