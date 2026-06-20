# Phase 2 — The phase-1 data model: schema, normalize, and the domain stores

*Realizes design Decision 3 (the phase-1 data model). Depends on Phase 1.*

Lay down the five-table schema and the domain core that reads and writes it, so
every later stage has a real, constraint-enforcing persistence seam to build on.

**What gets built (the observable end state):**

- `internal/db/`: the ordered migrations creating `jobs`, `subjects`, `claims`,
  `pages`, and the `pages_fts` external-content FTS5 virtual table — with the
  DB-enforced constraints `UNIQUE(subjects.norm_name)`,
  `CHECK(subjects.type IN ('entity','event','concept'))`, and
  `CHECK(length(pages.body) <= 12000)`, plus the documented (comment-only, no
  `FOREIGN KEY`) relationships and the `jobs_status`/`claims_subject`/`claims_job`
  indexes. Migrations are created with `bin/new-migration wiki <name>` (never a
  hand-authored version), embedded via `//go:embed` as `db.FS`, and applied by
  the appkit runner.
- `internal/wiki/`: the domain types (`Subject`, `Claim`, `Page`, `Job`), the
  pure `normalize(name) string` function (NFKC → casefold → trim → collapse
  internal whitespace → strip diacritics), and the stores that read/write the
  five tables. The page-write path performs the explicit in-tx `pages_fts` sync
  (delete-by-OLD-value before insert-new) described in D8 — built here as part of
  the store, exercised end-to-end in Phase 6.

**Done when:**

- R-7SNG-0G9A — inserting a second `subjects` row whose `norm_name` equals an
  existing row's is rejected by `UNIQUE(norm_name)`.
- R-7TVC-E7ZZ — `normalize` is deterministic and applies the full pipeline:
  `normalize("  Café ") == normalize("cafe")`, and distinct names normalize
  distinctly.
- R-7V38-RZQO — a `pages` write with `body` longer than 12,000 characters is
  rejected by `CHECK(length(body) <= 12000)`.
- R-7WB5-5RHD — a `subjects` write whose `type` is outside `{entity,event,concept}`
  is rejected by the `CHECK` constraint.
- Tests run against a real temp `modernc.org/sqlite` DB migrated by the appkit
  runner; the suite is green.
