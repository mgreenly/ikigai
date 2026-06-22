# Phase 33 — MCP surface: multi-state `jobs` + the `jobs_count` verb (thirteen verbs)

*Realizes design Decision 16 (MCP surface — the `jobs` reshape and the new `jobs_count` verb) and Decision 10 (the exact verb membership, R-MUQ4). Depends on Phase 32 (the `wiki.JobStore` multi-state filter + `CountJobs`) and Phase 27 (the existing twelve-verb surface).*

This phase exposes Phase 32's seam on the MCP surface in `internal/mcp`, taking
the surface from twelve to **thirteen** verbs.

- **`mcp.JobFilter` becomes `Statuses []string`** in place of the scalar
  `Status`, matching the store filter.
- **The `jobs` verb's `status` input becomes an enum-constrained array.** Its
  `inputSchema` carries `items.enum = [pending, working, done, failed, aborted]`
  and the tool description names the five, so the vocabulary is discoverable from
  `tools/list`. An empty/omitted array means all states; a non-empty array
  filters match-any. The page comes back **newest-first** (Phase 32 passthrough).
- **Every `status` element is validated** against the closed five-state set; any
  value outside it returns a clean tool error whose message **names the valid
  set** — never a silent empty page. The same rule governs `jobs_count`.
- **New `jobs_count` verb** — input `{status?: [state…], since?, until?}` (no
  `limit`/`cursor`), result `{count: <int>}`. It shares the `{status, since,
  until}` → `JobFilter` parse helper with `jobs`. New `jobsCountFunc` interface +
  `WithJobsCountService` Option, registered in `tools()` when wired; `jobs_count`
  is a no-required verb whose `inputSchema` omits `required` (never `null`).
- **Composition-root wiring:** update `cmd/wiki/main.go` — the `jobListService`
  adapter passes `Statuses` straight through, and a new count adapter wires
  `mcp.WithJobsCountService(...)` over `wiki.JobStore.CountJobs`. Both Specs
  expose the identical thirteen-verb surface.

**Done when:** R-Y36L-E3W6 (`jobs_count` returns `{count}` equal to the matching
`jobs` total, same filter inputs, no items/cursor) and R-Y4EH-RVMV (the
`jobs`/`jobs_count` `status` schema publishes the five-state `enum`, and an
unrecognized value is a clean tool error naming the valid set) are covered by
clearly-named handler-level tests; the reshaped R-37NS-BRXR (`jobs` `status`
array restricts match-any), the narrowed R-3EZ6-MEDX (malformed `since`/`until`
or cursor → clean error), the extended R-3G73-064M (`jobs`/`jobs_count`/`llm_calls`
omit empty `required`), and R-MUQ4-K1JS (`tools/list` returns exactly the
thirteen verbs including `jobs_count`) remain green; and the full suite is green
per design's *Conventions*.
