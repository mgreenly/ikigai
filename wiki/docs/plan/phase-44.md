# Phase 44 — Merge MCP surface + the `kind` job filter

*Realizes design Decision 27 (merge MCP surface) and the `kind`-filter slice of Decision 16. Depends on Phase 43 (D26 merge job + execution), Phase 42 (D25 `AliasStore`), Phase 33 (D16 `jobs`/`jobs_count` verbs).*

The merge feature reaches the MCP product surface, taking it from thirteen to **fifteen** verbs, and the existing job-list verbs learn to filter by `kind`.

In `internal/mcp` (cloning the D16 idiom — `Handler` func field, `WithXxxService` Option, `handleToolCall` case, `handleXxxCall`, conditional `tools()` append, hand-coded `inputSchema`):

- **`merge`** (write, fire-and-return): `objectSchema({winner, loser}, [winner, loser])`. Resolves both `type/slug` paths to ids once via `GetByPath` (a no-subject path → clean **not-found**, no job enqueued; `ErrAmbiguousPath` → **tool error** naming the collision; same resolved id → **tool error**), enqueues a `kind='merge'` job (Phase 43) with `owner` from `appkit.IdentityFrom(ctx)`, and returns `{job_id, status:"queued"}` — never `"merged"`.
- **`merges`** (read, paginated): `listSchema({})` (omits `required`). Keyset-paginates the `aliases` table newest-first, each item `{merged_at, winner (type/slug path), loser_name, alias_norm_name, created_by}`, the winner rendered by resolving `subject_id → Path`. Backed by a net-new `AliasStore.ListMerges(ctx, page.Params)` in `internal/wiki`.
- **The `kind` filter on `jobs`/`jobs_count`**: `JobFilter` gains a `Kind` field and `ListJobs`/`CountJobs` filter on it with an **omitted-means-`ingest`** default (so a merge job never appears in the default list); the verbs accept a `kind` array over the closed `{ingest, merge}` set, validated fail-loud (an unknown value → tool error naming the set), `kind:["merge"]` listing merge jobs.

The composition root wires `WithMergeService`/`WithMergesService` (and the `Resolver`/stores they need) into the Spec(s), and the dashboard is restarted after deploy to re-read the manifest (operational note, not built here).

**Done when:** R-DWDM-RVA7, R-DYTF-JERL, R-E01B-X6IA, R-E198-AY8Z, R-E2H4-OPZO, R-E3P1-2HQD (D27) and R-E4WX-G9H2 (D16) are each covered by a clearly-named test driving the JSON-RPC handler in-process with a stubbed identity against the Phase 43 service on a real temp SQLite + mock compiler — the fifteen-verb list, `merge` enqueue/fire-and-return, its two error channels, identity-gated owner attribution, the `merges` audit page, and the `kind` filter's default-ingest / merge-listing / fail-loud behaviors — and the suite is green.
