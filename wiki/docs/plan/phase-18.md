# Phase 18 — remove the keyword retrieval lane

*Realizes design Decision 8 (no retrieval lane this release) and its knock-on write-side edits in Decision 3 (`internal/wiki`) and Decision 4 (the ingest commit path). Depends on Phase 17 (after which `ask` no longer imports `internal/retrieve`).*

D8 has been re-decided: this release ships **no retrieval lane**. With Phase 17,
`ask` finds pages by subject extraction, so the FTS5 keyword machinery has no
consumer and is removed. This phase deletes it and drops the `pages_fts` table.
Because committed migrations are immutable, the table is removed by a **new**
forward migration — the original create migration stays frozen on disk. Removing
the in-tx FTS sync and `PageStore.Search` must land together with (or before) the
drop, so the commit path never references a table that no longer exists.

**What gets built (the observable end state):**

- `internal/retrieve` is **deleted** — the `Hit`/`Retriever`/`Service`/
  `keywordRetriever` seam, `ftsPhrase`, the bm25 lane, and the registry-first pin.
  Nothing imports it (Phase 17 removed the last consumer).
- `internal/wiki`:
  - `PageStore.Upsert` no longer syncs `pages_fts` — the commit tx upserts
    subjects, claims, and pages and flips the job to `done` with **no reference to
    any FTS table**.
  - `PageStore.Search` (the FTS-backed page-search method) is **deleted**.
  - The `search default/cap` config knobs leave `wiki.Config`, and their
    `ManifestExtras` entries leave `cmd/wiki/main.go`.
- `internal/db/migrations`: a new forward migration created via
  `bin/new-migration wiki drop_pages_fts` issues `DROP TABLE pages_fts;`. The
  original `CREATE VIRTUAL TABLE pages_fts` migration is unchanged.
- Tests that referenced `pages_fts` or `PageStore.Search` (the `internal/db` and
  `internal/wiki` page-search/FTS-sync tests) are removed or rewritten to the
  FTS-free schema.

**Done when:**

- R-PH8Z-YHNX — a test asserts that after migrations run, the schema contains **no
  `pages_fts`** table (the new drop migration is applied and recorded in
  `schema_migrations`), and the original create migration remains unmodified on
  disk.
- R-PIGW-C9EM — a test asserts an ingest integrates and commits end-to-end against
  the FTS-free schema with **no reference to any `pages_fts` table** in the commit
  path (the page upsert and `done` transition succeed with no FTS sync step).
- `internal/retrieve` no longer exists and nothing imports it; the D8-related
  retired ids (the old keyword-lane tests) are gone with the package.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki` — including the new drop
  migration).
