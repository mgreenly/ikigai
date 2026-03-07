# Context Summaries — Design Gaps

Gaps identified by review of `project/context-summaries.md` against the codebase and prerequisite design docs.

---

## High — Contradictions or Missing Design

### 1. `active_budget` reference is wrong

**Location**: "Pruning and Summarization Flow" > "On IDLE Transition", step 2

The flow says "Prune oldest turns until total <= `active_budget`" but the "Token Budget Allocation" section explicitly says there is no `active_budget` vs `summary_budget` split. Should say "budget" or `sliding_context_tokens`.

### 2. Q5 rationale contradicts Q3

**Location**: Q5 (Summarization During `/clear`)

Q5 justifies blocking with "The input is bounded (`recent_summary` + active messages), so generation should be fast." But Q3 decided to summarize from **raw DB messages**, not `recent_summary` + active. The input could be the entire epoch's message history from the database. The Q5 rationale is stale and needs updating.

### 3. Missing: generation counter field in struct

**Location**: "In-Memory State" > "New Fields on `ik_agent_ctx_t`"

Q4 decides on a generation counter incremented on every prune, but the struct definition only lists `recent_summary` and `recent_summary_tokens`. Missing field: something like `uint32_t recent_summary_generation`.

### 4. Missing: previous-session summary cache on agent struct

**Location**: "In-Memory State" > "Previous Session Summaries"

The doc says previous-session summaries are "loaded from the database at agent startup (during replay) and cached in memory for request building" but defines no field on `ik_agent_ctx_t` to hold them. The request builder (Piece 4) needs to read them from somewhere. Need a struct and array field, or a decision to re-query from DB on every request.

### 5. Missing: how Piece 1 finds the epoch boundary

**Location**: "Decomposition" > "Piece 1: Summary Boundaries"

Piece 1 says "identify the recent range: messages from last `/clear` (or conversation start) to `context_start_index`." But `/clear` events are metadata kinds — they are NOT in `agent->messages[]` (which only contains conversation kinds). The doc doesn't specify how the epoch start is determined. Options: track it as an index on the agent struct, query the DB for the most recent `clear` event, or record the message index at `/clear` time.

### 6. Missing: summary generation module file

**Location**: "Implementation Touchpoints" > "New Files"

Piece 2 (Summary Generation) describes a function that takes messages and returns summary text via an LLM call. No file is listed for this module. The LLM call, summarization prompt, request building, and response parsing need a home. Candidate: `apps/ikigai/summary.h` / `summary.c`.

### 7. Missing: background execution files

**Location**: "Implementation Touchpoints" > "New Files"

Piece 5 (Background Execution) is described as "the hardest piece and the biggest new capability" but has zero implementation mapping — no new files, no modified files. Needs at minimum a file listing and a description of how it relates to existing background infrastructure (`tool_thread`, `CURLM`).

---

## Medium — Unaddressed Edge Cases

### 8. `/rewind` (`/clear MARK`) interaction with summaries

**Location**: Not mentioned anywhere in the doc

`/clear MARK` removes messages after a mark but does NOT end the epoch. The doc's `/clear` flow does not distinguish between `/clear` (nuclear, ends epoch) and `/clear MARK` (rewind, keeps epoch). Questions:
- Does `/clear MARK` trigger a previous-session summary? (Probably not — epoch isn't ending.)
- If `recent_summary` covers messages that are now rewound, it becomes inaccurate. Is it reset to NULL? Regenerated?
- If `context_start_index` pointed into removed messages, does it need adjustment?

### 9. LLM call failure handling

**Location**: "Background LLM Calls" and "Pruning and Summarization Flow"

No strategy for failure in either path:
- **Background (recent)**: If the LLM call fails, does the agent retain the previous `recent_summary`? Retry? Log and move on?
- **Blocking (`/clear`)**: If the summary LLM call fails, does `/clear` abort (leaving epoch intact) or proceed without a summary (epoch history lost from LLM's perspective permanently)? Should the user be prompted?

### 10. `/model` switch cascade — RESOLVED (no change needed)

Already handled by existing token cache invalidation. Model switch invalidates cached token counts, triggers re-estimation, and any resulting prune naturally dispatches re-summarization. No special handling required.

### 11. Crash during blocking `/clear` summary

**Location**: "Pruning and Summarization Flow" > "On `/clear`"

The flow is: query messages (1) -> generate summary (2) -> store in DB (3) -> clear state (4-5) -> insert clear event (6). If the process crashes between steps 3 and 6, the DB has a session summary for an epoch that was never closed. If it crashes during step 2, the `/clear` never happened — user doesn't know. Consider: should the clear event be written first (guaranteeing clear happens) with summary generation as best-effort?

### 12. Truncation strategy for oversized summaries

**Location**: "Token Budget Allocation" > "Enforcement"

The doc says "If the generated summary exceeds the limit, it is truncated to fit" but doesn't specify how. Token-level truncation requires tokenization. Character-level truncation could cut mid-sentence. Needs a decision: truncate by estimated token count (bytes / 4), or ask the LLM to respect the limit in the prompt and accept whatever comes back.

### 13. Pinned doc assembly function not in modified files

**Location**: "Implementation Touchpoints" > "Modified Files"

The current pinned document assembly is in `ik_agent_get_effective_system_prompt()` at `apps/ikigai/agent.c:276-341`. This function concatenates pinned docs into a single string. The system blocks refactor (Piece 0) must change this function, but `agent.c` is not listed in the modified files table.

---

## Low — Minor or Implementation Details

### 14. Piece 0 doesn't mention token cache impact

The system prompt changes from a single string to an array of blocks. The token cache counts `system_tokens` as a single cached value. With multiple blocks, should each block be counted separately? Or is the total system token count still one value? Piece 0's description should note the token cache interaction.

### 15. Short epochs trigger blocking LLM call

Q6 says skip only if the epoch is empty. A single trivial exchange ("hi" / "hello") still triggers a blocking LLM call and DB row. Consider a minimum message count or token threshold below which the epoch is skipped or stored verbatim.

### 16. Concurrent agent rate limits

Multiple agents sharing the same API key could interfere if both are generating summaries simultaneously. No coordination or queuing is described. Likely acceptable for now but worth noting.

### 17. Generation counter resets on restart

The generation counter is in-memory and resets to 0 on restart. Since `recent_summary` also resets to NULL, this is safe — no stale summary can conflict. Should be stated explicitly for implementers.

### 18. Marks in summarization input

When summarizing an epoch's raw messages from the DB, should `mark` events be included in the transcript? They could provide useful structure ("checkpoint BEFORE_REFACTOR created here"). Needs a one-line decision: include or exclude metadata kinds from summarization input.

### 19. `session_summaries` table lacks unique constraint

No unique constraint prevents duplicate summaries for the same epoch (same `start_msg_id`/`end_msg_id`). Consider adding one, or accept that application-level enforcement is sufficient.
