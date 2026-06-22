# Phase 32 — jobs store: multi-state filter, newest-first order, and CountJobs

*Realizes design Decision 15 (cursor pagination — the jobs newest-first / multi-state set / count additions). Depends on Phase 26 (the `internal/page` codec and the four paginated list seams).*

This phase reshapes the **jobs** store seam in `internal/wiki` to the evolved D15
contract; the other three list seams (subjects, claims, llm_calls) are untouched.

- **`wiki.JobFilter` becomes `Statuses []string`** in place of the scalar
  `Status string` (`Since`/`Until` unchanged). A `nil`/empty `Statuses` matches
  all states; a non-empty slice filters **match-any** (`status IN (?,?,…)`).
  Element validity is enforced at the MCP boundary (Phase 33), not re-checked here.
- **`JobStore.ListJobs` returns newest-first.** The jobs keyset query flips to
  descending — `ORDER BY received_at DESC, id DESC` with the predicate
  `(received_at < ?1 OR (received_at = ?1 AND id < ?2))` — served by a reverse
  scan of the existing `jobs_pending_received (status, received_at, id)` index.
  **No new index, no migration.** The opaque cursor codec is unchanged (it still
  carries the boundary `(received_at, id)`); only the operator and `ORDER BY`
  direction differ. The ordering holds whether or not a status/time filter applies.
- **New `JobStore.CountJobs(ctx, f JobFilter) (int, error)`** runs the *same*
  `WHERE` as `ListJobs` under `SELECT COUNT(*) FROM jobs WHERE <filters>` — no
  keyset predicate, no `ORDER BY`, no `LIMIT` — returning the filtered total
  without materializing rows. An empty filter counts all jobs.
- **Composition-root ripple (sanctioned):** `cmd/wiki/main.go`'s `jobListService`
  adapter is the only consumer of `wiki.JobFilter`; update it to map into the new
  `Statuses` field (wrapping the still-scalar `mcp.JobFilter.Status` into a
  one-element slice) so the build stays green. The full wire reshape and the
  `CountJobs` wiring land in Phase 33 — this is a minimal compile-keeping change.

The existing Phase 26 jobs pagination tests are updated for the flipped
newest-first order and the renamed filter field; subjects/claims/llm_calls tests
are unaffected.

**Done when:** R-XYAZ-V0XE (jobs newest-first, filtered or not), R-XZIW-8SO3
(`ListJobs` multi-state match-any; empty set = all states), and R-Y1YP-0C5H
(`CountJobs` filtered total equal to the `ListJobs` count under the same filter,
no rows) are each covered by clearly-named tests against a real temp SQLite, and
the suite is green per design's *Conventions*.
