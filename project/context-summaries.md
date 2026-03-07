# Context Summaries — Design Document

**Feature**: Automatic summarization of pruned conversation history
**Status**: Design
**Prerequisite**: rel-13 sliding context window (complete)
**Related**: `project/sliding-context-window.md`, `project/context-management-v2.md`

---

## Overview

The sliding context window (rel-13) prunes old turns from the in-memory message array to stay within a token budget. Pruned messages are gone from the LLM's perspective — it has no memory of them. Context summaries restore partial memory by prepending condensed summaries of pruned history to the LLM request.

Summaries are organized in three tiers:

```
[ active ]              messages within token budget, sent verbatim
[ recent ]              pruned from active, current clear epoch, summarized
[ previous-sessions ]   last N clear epochs, each independently summarized
```

The LLM sees summaries + active messages on every request. Summaries are not messages — they are injected into the request at build time and never stored in the `messages` array.

---

## Definitions

**Clear epoch**: The span of conversation between two `/clear` commands (or between conversation start and the first `/clear`). A clear epoch is the unit of session identity. When the user runs `/clear`, the current epoch ends and a new one begins.

**Recent**: Messages that have been pruned from the active context window but belong to the current clear epoch. This set grows as the sliding window advances. The recent summary is regenerated on every prune.

**Previous session**: A completed clear epoch. Once `/clear` fires, the entire epoch (recent + whatever was still in active context) becomes a previous session. Its summary is generated once and stored permanently.

---

## Three-Tier Model

### Tier 1: Active

The existing sliding context window. Messages from `context_start_index` to `message_count`, sent verbatim to the LLM. No change from current behavior.

### Tier 2: Recent

Messages in the current clear epoch that have fallen out of the active window. Range: from the most recent `/clear` event (or conversation start) to `context_start_index`.

**Properties:**
- In-memory only — stored as a string field on the agent struct
- Regenerated (full re-summarize) on every prune
- Lost on process exit — regenerated on next prune after restart
- NULL until the first prune of the current epoch (no summary needed if nothing has been pruned)

**Lifecycle:**
1. User works, sliding window prunes a turn
2. Identify all messages in the recent range
3. Send them to an LLM for summarization (background)
4. Store the result in `agent->recent_summary`
5. Next LLM request picks up the updated summary
6. On `/clear`: recent summary participates in generating the previous-session summary, then is reset to NULL

### Tier 3: Previous Sessions

Completed clear epochs, each summarized independently. Stored in a database table. Generated once at `/clear` time, never modified.

**Properties:**
- Immutable — generated once, stored permanently
- Capped at 5 per agent (oldest dropped when limit exceeded)
- Survive process restarts (loaded from DB on replay)
- Each summary includes a token count so the request builder can do budget math

**Lifecycle:**
1. User runs `/clear`
2. Generate a summary of the entire ending epoch (recent + remaining active messages)
3. Store in `session_summaries` table
4. If agent now has > 5 session summaries, delete the oldest
5. Reset recent summary to NULL, begin new epoch

---

## System Blocks

### Motivation

Currently, the system prompt is a single concatenated string: base instructions + pinned documents mashed together. This prevents providers from caching stable portions independently and provides no clean injection point for summaries.

The solution is to model system-level content as an **ordered array of blocks**. Each block is a distinct piece of system content with its own identity. Pinned documents, summaries, and the base prompt are all system blocks — the same abstraction, the same injection path.

### Block Ordering

Blocks are ordered most-stable-first, most-volatile-last. This maximizes provider cache hit rates, since all three providers cache from the front of the request:

```
Block 0:   Base system prompt (agent instructions)
Block 1:   Pinned document A
Block 2:   Pinned document B
Block 3:   Previous-session summary 5 (oldest)
Block 4:   Previous-session summary 4
...
Block 7:   Previous-session summary 1 (most recent)
Block 8:   Recent summary (current epoch, pruned portion)
```

- **Base prompt**: almost never changes
- **Pinned documents**: rarely change (user pins/unpins occasionally)
- **Previous-session summaries**: change only on `/clear`
- **Recent summary**: changes on every prune (most volatile)

### Provider Serialization

Each provider serializes system blocks to its native format:

**Anthropic** — `system` as array of `TextBlockParam` objects. Each block becomes a `{ "type": "text", "text": "..." }` entry. Stable blocks can be marked with `cache_control: { "type": "ephemeral" }` for explicit caching.

```json
{
  "system": [
    { "type": "text", "text": "Base instructions..." },
    { "type": "text", "text": "Pinned doc A content...",
      "cache_control": { "type": "ephemeral" } },
    { "type": "text", "text": "Previous session summary...",
      "cache_control": { "type": "ephemeral" } },
    { "type": "text", "text": "Recent summary..." }
  ]
}
```

**OpenAI Chat Completions** — Each block becomes a `system` or `developer` role message at the start of the messages array. Prefix caching is automatic — stable blocks at the front get cached without explicit markers.

**OpenAI Responses API** — Base prompt goes in the `instructions` field. Additional blocks become `developer` role messages in `input`.

**Google** — `systemInstruction` with multiple `parts`. Each block becomes a `{ "text": "..." }` entry in the `parts` array. Implicit caching is automatic on Gemini 2.0+ models.

```json
{
  "systemInstruction": {
    "parts": [
      { "text": "Base instructions..." },
      { "text": "Pinned doc A content..." },
      { "text": "Previous session summary..." },
      { "text": "Recent summary..." }
    ]
  }
}
```

### Request Struct Changes

`ik_request_t` currently has a single `char *system_prompt` field. This changes to an array of system blocks:

```c
typedef struct {
    char *text;       // Block content
    bool cacheable;   // Hint for provider-specific caching (e.g. Anthropic cache_control)
} ik_system_block_t;

typedef struct {
    // ... existing fields ...
    ik_system_block_t *system_blocks;  // Ordered array of system blocks
    size_t system_block_count;
    // char *system_prompt;            // REMOVED — replaced by system_blocks
} ik_request_t;
```

Each provider serializer iterates `system_blocks` and maps to its native format.

---

## What the LLM Sees

On every request, the payload is constructed as:

```
system blocks:
  [0] Base system prompt
  [1] Pinned document A
  [2] Pinned document B
  [3] Previous-session summary 5     oldest
  [4] Previous-session summary 4
  [5] Previous-session summary 3
  [6] Previous-session summary 2
  [7] Previous-session summary 1     most recent completed epoch
  [8] Recent summary                 current epoch, pruned portion

tool definitions

messages:
  ── context ──
  [active messages]                  verbatim
```

System blocks and summaries are not part of `agent->messages[]` — they are assembled during request building in `ik_request_build_from_conversation()`. Tool definitions remain a separate concern, serialized by each provider in its native position (Anthropic: top-level `tools`, OpenAI: top-level `tools`, Google: top-level `tools`).

---

## Database Schema

### New Table: `session_summaries`

```sql
CREATE TABLE session_summaries (
    id            BIGSERIAL PRIMARY KEY,
    agent_uuid    TEXT NOT NULL REFERENCES agents(uuid) ON DELETE CASCADE,
    summary       TEXT NOT NULL,
    start_msg_id  BIGINT NOT NULL,
    end_msg_id    BIGINT NOT NULL,
    token_count   INTEGER NOT NULL,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX idx_session_summaries_epoch
    ON session_summaries(agent_uuid, start_msg_id, end_msg_id);

CREATE INDEX idx_session_summaries_agent
    ON session_summaries(agent_uuid, created_at DESC);
```

**Columns:**
- `agent_uuid`: The agent this summary belongs to
- `summary`: The generated summary text
- `start_msg_id`: First message ID in the epoch (message after the previous `/clear`, or first message ever)
- `end_msg_id`: Last message ID in the epoch (message before the `/clear` that ended it)
- `token_count`: Token count of the summary text, for budget allocation
- `created_at`: When the summary was generated

**Constraints:**
- One row per clear epoch per agent
- Rows are immutable after creation
- Cap enforced in application code (delete oldest when > 5)

---

## In-Memory State

### New Fields on `ik_agent_ctx_t`

```c
typedef struct {
    // ... existing fields ...

    // Context summaries
    char *recent_summary;              // Current epoch's pruned-history summary (NULL = none)
    int32_t recent_summary_tokens;     // Token count of recent_summary (0 = none)
    uint32_t recent_summary_generation; // Incremented on every prune; background tasks compare against this

    // Previous-session summaries (loaded from DB at startup, updated on /clear)
    ik_session_summary_t *session_summaries;
    size_t session_summary_count;
} ik_agent_ctx_t;
```

### Previous Session Summaries

Previous-session summaries are loaded from the database at agent startup (during replay) and cached in memory for request building.

```c
typedef struct {
    char *summary;       // Summary text
    int32_t token_count; // Token count for budget math
} ik_session_summary_t;
```

The `session_summaries` array on `ik_agent_ctx_t` holds up to 5 entries, ordered oldest-first. It is populated during replay and updated in-place when `/clear` generates a new session summary.

---

## Summary Generation

### Input

The summarization LLM call receives:

- The messages to summarize (conversation kinds only — metadata kinds like `mark`, `clear`, `rewind` are excluded, matching `ik_msg_is_conversation_kind()`)
- A system prompt instructing the LLM to produce a concise summary

### Output

A plain text summary. No structured format — just natural language describing what happened, what was decided, and what the current state of work is.

### Which LLM?

The summary is generated using the same provider and model configured on the agent. This avoids needing separate API credentials or provider routing for summarization.

### Prompt

The summarization prompt should instruct the LLM to:

1. Capture key decisions and conclusions
2. Note any unresolved questions or open threads
3. Preserve important technical details (file names, function names, error messages)
4. Omit conversational filler and repeated attempts
5. Be concise — the summary will consume context budget

---

## Token Budget Allocation — DECIDED

Rather than dynamically partitioning the token budget, each summary type has a fixed maximum token size and a fixed maximum count. These are defined as constants in code, intended to become per-agent configuration values in a future release (rel-15).

### Limits

```c
#define IK_SUMMARY_RECENT_MAX_TOKENS           4000
#define IK_SUMMARY_PREVIOUS_SESSION_MAX_TOKENS  2000
#define IK_SUMMARY_PREVIOUS_SESSION_MAX_COUNT   5
```

| Type | Max tokens | Max count | Worst-case total |
|------|-----------|-----------|-----------------|
| Recent summary | 4,000 | 1 | 4,000 |
| Previous-session summary | 2,000 | 5 | 10,000 |
| **Total** | | | **14,000** |

At the default 100K token budget, summaries consume at most 14% of the context window.

### Enforcement

The summarization prompt instructs the LLM to stay within the token limit for that summary type. If the generated summary exceeds the limit, it is truncated using byte-based estimation (bytes / 4) at the last sentence boundary before the limit. Truncation is a safety net — the prompt is the primary mechanism.

### Independent Budgets

The active window and summaries have separate, independent budgets. The active window budget is the full `sliding_context_tokens` value — summaries do not reduce it. Summary token limits are enforced independently via the fixed caps above. The two budgets are additive: the total tokens sent to the provider is active window tokens + summary tokens.

---

## Pruning and Summarization Flow

### On IDLE Transition (after every LLM response)

Current flow:
1. Record turn token cost
2. Prune oldest turns until total <= budget

New flow:
1. Record turn token cost
2. Prune oldest turns until total <= budget
3. **If messages were pruned**: trigger recent summary regeneration (background)

### On `/clear MARK` (Rewind)

`/clear MARK` removes messages after a mark but does not end the epoch. Summary behavior:

1. **No previous-session summary** — the epoch is not ending
2. **Reset `recent_summary` to NULL** — it may cover messages that no longer exist. Regenerated naturally on the next prune.
3. **Token cache adjustment** — if `context_start_index` exceeds the new `message_count`, clamp it. Handled by the token cache reset that `/clear MARK` triggers.

### On `/clear`

Current flow:
1. Clear scrollback, messages, token cache, marks
2. Insert `clear` event to DB
3. Re-insert system prompt

New flow:
1. **Query all raw messages** in the current epoch from the database
2. **Generate previous-session summary** via blocking LLM call (skip if epoch is empty)
3. **Store** in `session_summaries` table
3. **Enforce cap**: delete oldest if > 5 for this agent
4. Clear scrollback, messages, token cache, marks
5. **Reset** `recent_summary = NULL`
6. Insert `clear` event to DB
7. Re-insert system prompt

**Crash safety:** The ordering (generate summary → store → clear → insert clear event) is intentional. If the process crashes mid-flow, orphaned summary rows are harmless — they don't corrupt state. If the crash happens before the clear event is written, the user simply runs `/clear` again after restart. No transactional guarantees are needed.

---

## Background LLM Calls

Summarization requires an LLM call, but the main event loop is single-threaded and the user shouldn't wait for summaries before they can continue working.

### Failure Handling

- **Background (recent):** On LLM call failure, log the error and keep the previous `recent_summary`. A stale summary is better than no summary. No retry — the next prune triggers a fresh attempt.
- **Blocking (`/clear`):** On LLM call failure, proceed with the clear and store no summary for the epoch. Log a warning so the user knows. The epoch's raw messages remain in the DB.

### Requirements

- Summary generation must not block the main event loop
- The user can send messages while a summary is being generated
- If a new prune happens before the previous summary completes, the in-progress summary should be cancelled or its result discarded (it's already stale)
- A brief staleness gap is acceptable — between prune and summary completion, the LLM sees slightly less context than it could

### Implementation Considerations

The agent already has infrastructure for background work:
- `tool_thread` / `tool_thread_mutex` / `tool_thread_complete` for tool execution
- Per-agent `worker_db_ctx` for database access without contention
- `CURLM` multi-handle for async HTTP

A summary generation task could reuse or parallel the tool execution pattern — a background thread that makes an HTTP request to the LLM provider, parses the response, and signals completion. The main loop picks up the result on the next tick.

---

## Replay and Restart

### What Happens on Process Restart

1. `ik_agent_replay_history()` reconstructs the message array from the database
2. Previous-session summaries are loaded from `session_summaries` table
3. `recent_summary` starts as NULL and `recent_summary_generation` resets to 0 — both reset together, so no stale summary can conflict
4. On the first interaction that triggers pruning, `recent_summary` is regenerated from the pruned messages

This means there's a brief window after restart where the LLM has no recent summary, even if the previous process had one. This is acceptable — the summary will be regenerated on the first prune.

### Fork Inheritance

When an agent forks:
- Child does **not** inherit `recent_summary` — the child's epoch starts at fork time
- Child does **not** inherit previous-session summaries — those are the parent's long-term memory
- Child starts with zero summaries and builds its own through its own `/clear` cycles
- The child's inherited conversation (from `ik_agent_copy_conversation()`) is its starting context

---

## `/summary` Command

Outputs current summary state to the scrollback buffer. Display only — not added to message history or sent to the LLM. Similar to `/agents`.

**Output format:**

```
── Recent Summary ──
<recent_summary text or "(none)">

── Previous Sessions (N/5) ──
[1] <oldest session summary>
[2] ...
[N] <newest session summary>
```

Shows all tiers so the user can inspect what the LLM is seeing as context.

---

## Implementation Touchpoints

### New Files

| File | Purpose |
|------|---------|
| `apps/ikigai/summary.h` | Summary generation interface (LLM call, prompt, response parsing) |
| `apps/ikigai/summary.c` | Summary generation implementation |
| `apps/ikigai/summary_worker.h` | Background summary thread interface |
| `apps/ikigai/summary_worker.c` | Background summary thread implementation |
| `apps/ikigai/db/session_summary.h` | DB queries for session_summaries table |
| `apps/ikigai/db/session_summary.c` | DB query implementations |
| `share/migrations/009-session-summaries.sql` | Schema migration |

### Modified Files

| File | Change |
|------|--------|
| `apps/ikigai/agent.h` | Add `recent_summary`, `recent_summary_tokens`, `recent_summary_generation`, `session_summaries`, and `summary_thread` fields |
| `apps/ikigai/agent.c` | Refactor `ik_agent_get_effective_system_prompt()` to return individual system blocks instead of a concatenated string |
| `apps/ikigai/commands_basic.c` | Add `/summary` command handler (display only, no message history) |
| `apps/ikigai/agent_state.c` | Trigger summary regeneration after prune |
| `apps/ikigai/commands_basic.c` | Generate previous-session summary on `/clear` |
| `apps/ikigai/providers/provider.h` | Replace `char *system_prompt` with `ik_system_block_t` array on `ik_request_t` |
| `apps/ikigai/providers/request_tools.c` | Build system blocks (base prompt, pinned docs, summaries) in `ik_request_build_from_conversation()` |
| `apps/ikigai/providers/anthropic/request.c` | Serialize system blocks as array of `TextBlockParam` with `cache_control` |
| `apps/ikigai/providers/openai/request_chat.c` | Serialize system blocks as multiple `system`/`developer` role messages |
| `apps/ikigai/providers/openai/request_responses.c` | Serialize base block as `instructions`, rest as `developer` messages |
| `apps/ikigai/providers/google/request.c` | Serialize system blocks as `systemInstruction.parts` array |
| `apps/ikigai/commands_fork.c` | No summary inheritance — child starts clean |
| `apps/ikigai/db/agent_replay.c` | Load previous-session summaries on replay |

---

## Decomposition

The feature decomposes into independent pieces that can be built and tested separately:

### Piece 0: System Blocks Refactor

Replace the single `char *system_prompt` on `ik_request_t` with an ordered array of `ik_system_block_t`. Refactor all four provider serializers to iterate the array. Move pinned document handling from string concatenation to individual blocks. No summarization logic — just the new abstraction.

This is a prerequisite for all other pieces. It can be built and tested independently with existing functionality (base prompt + pinned docs) before any summary code exists. The token cache's `system_tokens` remains a single cached total across all blocks — no per-block granularity needed.

### Piece 1: Summary Boundaries

Pure logic. Given the message array, identify the "recent" range: messages from index 0 to `context_start_index`. The epoch always starts at index 0 because `/clear` resets the message array. No LLM, no background work, no DB.

**Inputs**: `agent->messages[]`, `context_start_index`
**Output**: start index (always 0) and end index (`context_start_index`) of the recent range

### Piece 2: Summary Generation

Given a range of messages, produce a summary via LLM call. A function that takes messages in, returns text out. Doesn't care about where it runs or when it's triggered.

**Inputs**: array of `ik_message_t`, provider/model config
**Output**: summary text string

### Piece 3: Summary Storage

Database operations for `session_summaries` table. Insert, query by agent, enforce cap.

### Piece 4: Summary Injection

Modify `ik_request_build_from_conversation()` to prepend summaries before active messages. Reads `recent_summary` from agent struct and previous-session summaries from a cached query result.

### Piece 5: Background Execution

The machinery to run an LLM call outside the main request-response loop. This is the hardest piece and the biggest new capability.

Uses a dedicated thread separate from the existing `tool_thread` infrastructure, allowing summary generation to run concurrently with tool execution. New fields on `ik_agent_ctx_t`:

```c
pthread_t summary_thread;
pthread_mutex_t summary_thread_mutex;
bool summary_thread_running;
bool summary_thread_complete;
uint32_t summary_thread_generation;  // Generation at time of dispatch
char *summary_thread_result;         // Result text (read by main loop on completion)
```

**Flow**: On prune, increment `recent_summary_generation`, spawn `summary_thread` with the current generation. The thread makes the LLM call via `summary.c`, writes the result to `summary_thread_result`, and sets `summary_thread_complete`. The main loop picks up the result on the next tick: if `summary_thread_generation == recent_summary_generation`, accept it; otherwise discard (a newer prune has already superseded it).

**Files**: `apps/ikigai/summary_worker.h` / `summary_worker.c` for the thread lifecycle. `apps/ikigai/agent.h` for the new fields.

### Piece 6: Integration

Wire prune -> background summarize -> update `recent_summary` -> next request picks it up. Wire `/clear` -> generate previous-session summary -> store in DB -> reset.

Pieces 1-4 can be built and tested without background execution. Piece 5 is independent infrastructure. Piece 6 is glue.

---

## Open Questions

### Q1: Summary Injection — DECIDED

Summaries are injected as **system blocks** — the same mechanism used for the base system prompt and pinned documents. See the "System Blocks" section for full details.

All three providers support multi-block system content natively:
- Anthropic: `system` as array of `TextBlockParam` with per-block `cache_control`
- OpenAI Chat Completions: multiple `system`/`developer` role messages
- OpenAI Responses API: `instructions` field + `developer` messages in `input`
- Google: `systemInstruction` with multiple `parts`

This approach is portable, supports per-block caching, and avoids polluting the conversation message flow with synthetic user messages.

### Q2: Summary Budget — DECIDED

Fixed limits per summary type, no dynamic budget partitioning. See "Token Budget Allocation" section. Recent: 4,000 tokens max. Previous-session: 2,000 tokens max, 5 max count. Total worst case: 14K tokens (14% of default 100K budget). These are code constants that will become per-agent config values in rel-15.

### Q3: Previous-Session Summary at `/clear` Time — DECIDED

Summarize from **raw messages in the database**, not from `recent_summary` + active. At `/clear` time, query all messages in the current epoch (from the previous `/clear` to now) and summarize them directly. This produces an authoritative summary derived from the actual conversation with no compression-of-compression loss.

If the epoch's messages exceed the LLM's context window, truncate from the front — the oldest messages in the epoch are least valuable for capturing the epoch's outcomes and final state. In practice, most epochs will fit comfortably.

### Q4: Background Summary Staleness — DECIDED

**Accept unless superseded.** Keep a generation counter on the agent, incremented on every prune. Each background summary records the generation it was started at. When it completes, accept the result unless a *newer* summary has already landed (i.e., `agent->recent_summary_generation > this_summary_generation`). A stale summary that covers most of the pruned history is better than no summary at all — only discard it if something newer has already replaced it.

### Q5: Summarization During `/clear` — DECIDED

**Block.** `/clear` is infrequent and explicit — the user can wait a few seconds. Blocking eliminates race conditions between the old epoch's summary generation and the new epoch's state. The input is raw DB messages for the entire epoch (per Q3), which could be large, but epochs that exceed the LLM's context window are truncated from the front (also per Q3), bounding the actual call size. Can revisit if it becomes a UX problem.

### Q6: What If the Agent Has Never Pruned? — DECIDED

**Always generate a previous-session summary on `/clear`**, regardless of whether pruning occurred. The summary is generated from all raw messages in the epoch (queried from the DB), not from `recent_summary`. Whether the sliding window pruned anything is irrelevant — the summary captures what happened in the epoch.

Skip only if the epoch is empty (no messages between the two `/clear` events).

### Q7: Fork and Previous-Session Summaries — DECIDED

**Don't inherit.** The child starts with zero previous-session summaries. Its history begins at the fork point — previous clear epochs are the parent's long-term memory, not the child's. The child inherits the parent's active conversation (via `ik_agent_copy_conversation()`), and that's sufficient context. `recent_summary` is also not copied — the child's epoch starts at fork time, so there's nothing "recent" to carry over. If the child lives long enough to `/clear`, it builds its own session summaries from that point forward.

### Q8: Summary Prompt Tuning — DECIDED

**Hardcoded in C.** The summarization prompt is a string constant, tuned during development. Can be externalized later if needed.

---

## Future Considerations

### Time-Based Partitioning

The three-tier model (active, recent, previous-sessions) could later be extended with time-based aggregation:

```
[ active ]
[ recent ]
[ previous-sessions ]    last 5 clear epochs
[ previous-days ]        daily aggregates
[ previous-weeks ]       weekly aggregates
```

Daily and weekly summaries would be generated by re-summarizing groups of session summaries. The `session_summaries` table already provides the raw material — no architectural changes needed, just an additional aggregation layer.

### Summary Quality Metrics

Once summaries are in use, measuring their quality becomes important. Possible signals:
- Does the LLM reference summarized information accurately?
- Does the user correct the LLM about things that were in the summary?
- Are summaries too long (wasting budget) or too short (losing information)?

### Incremental Summarization

The current design fully re-summarizes the recent range on every prune. If this becomes expensive, a future optimization could fold newly pruned messages into the existing summary incrementally. This trades accuracy for cost.

---

## Related Documents

- `project/sliding-context-window.md` — Sliding window design (prerequisite)
- `project/context-management-v2.md` — `/clear`, `/mark`, `/fork` commands
- `project/tokens/usage-strategy.md` — Token display and estimation strategy
- `project/README.md` — Roadmap
