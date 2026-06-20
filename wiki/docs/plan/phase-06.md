# Phase 6 — The retrieval seam: FTS5 keyword lane + registry-first pin

*Realizes design Decision 8 (the retrieval seam `internal/retrieve`). Depends on Phase 2 (the schema, the stores, normalize, and the in-tx FTS sync).*

Phase-1 retrieval is FTS5 keyword only, behind a flat `Hit`/`Retriever` seam so a
vector/hybrid lane drops in later without touching the read path or `ask`.

**What gets built (the observable end state):**

- `internal/retrieve/`: the lane-agnostic `Hit` struct (with the `PageID` fusion
  key, `== SubjectID` in phase 1), the `Retriever` interface
  (`Search(ctx, query, k) ([]Hit, error)`), the `keywordRetriever` lane
  (`NewKeyword(db)`: `WHERE pages_fts MATCH ftsPhrase(query) ORDER BY bm25(pages_fts) LIMIT k`,
  joined back to `pages`), the ported-verbatim `ftsPhrase` (wraps user text as a
  single quoted FTS5 phrase literal so operator characters are literal), and
  `SearchLimits.Resolve` (`k<=0 → Default`, `k>Cap → Cap`, else `k`).
- The `Service` (`NewService(db, r, limits)` + `Search`) composing the
  **registry-first pin** over the retriever: resolve `normalize(query)` against
  `subjects`; if it names a subject with a page, pin that page at rank 1, then the
  retriever fills the remainder deduped by `PageID` up to the resolved limit.
  Empty is clean — no lexical match and no registry hit → empty `[]Hit`, no error.

**Done when:**

- R-CLF2-TMI8 — `SearchLimits.Resolve`: `k<=0 → Default`, `k>Cap → Cap`, in-range
  `k` unchanged.
- R-CMMZ-7E8X — a query containing FTS5 operator characters (`"`, `*`, `OR`, …)
  matches literally via `ftsPhrase` without error.
- R-CNUV-L5ZM — the keyword lane returns pages matching the query, ranked
  best-first by `bm25()` and capped at `k`.
- R-CP2R-YXQB — `Service.Search` pins an exact normalized-name match's page at
  rank 1 ahead of purely lexical hits.
- R-CQAO-CPH0 — after a page is updated, search matches the new body and no longer
  matches text present only in the old body (external-content FTS sync correctness).
- Tests run against a real temp SQLite DB; the suite is green.
