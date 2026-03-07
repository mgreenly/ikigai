# Context Summaries — Test Coverage Gaps

Identified during post-implementation review (2026-03-06). These are edge cases with missing test coverage, not missing functionality.

---

## 1. End-to-end request building with all block types

No test exercises the full block chain: base prompt + pinned docs + previous-session summaries + recent summary, all the way through to provider serialization. Individual pieces are tested but the combined path is not.

## 2. Token accounting with recent_summary injection

`recent_summary_tokens` is stored on the agent struct but no test verifies it's consumed by the request builder or token cache when computing totals.

## 3. Concurrent prune events during worker execution

The generation counter staleness logic is unit-tested (matching vs stale), but no test simulates a prune arriving while the worker thread is actively running — verifying the dispatch skip and eventual discard on poll.

## 4. /clear MARK (rewind) specific tests

Rewind resets `recent_summary` to NULL (tested), but no test exercises the full `/clear MARK` command path end-to-end, including context_start_index clamping when it exceeds the new message_count after rewind.

## 5. Cosmetic deviations to consider normalizing

- `token_count` field is `int` in `ik_session_summary_t` — design specifies `int32_t`
- `SESSION_SUMMARY_CAP` (5) hardcoded in SQL query — could be a shared constant with `IK_SUMMARY_PREVIOUS_SESSION_MAX_COUNT`
