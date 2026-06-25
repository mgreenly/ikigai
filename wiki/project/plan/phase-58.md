# Phase 58 — Merging the two lanes: rank fusion + exact-name pin (`hybridRetriever`, `SearchAnalyzed`)

*Realizes design Decision 33 (merging the two lanes: rank fusion + exact-name pin). Depends on Phase 52 (the D08 seam), Phase 55 (the keyword lane), Phase 56 (the meaning lane), and Phase 57 (the `wiki.QueryAnalysis` type), plus the D25 `wiki.Resolver` (Phase 42) and the `wiki.PageStore`. Consumed by the `ask` rewrite (Phase 60).*

The two lanes become one ranked list: a `hybridRetriever` runs both, fuses by rank, optionally pins an exact-name match on top, and returns the top few pages. It is itself a `retrieve.Retriever`, so `ask` sees one list and never knows two lanes existed.

In `internal/retrieve`:

- `hybridRetriever{keyword, vector retrieve.Retriever, resolver *wiki.Resolver, pages *wiki.PageStore, cfg FusionConfig}` where `FusionConfig{RRFk int (default 60), PerLane int (default 60), FinalK int (default 8)}` — all three are config knobs. Tests drive it with **mock lane retrievers**, so this phase depends only on the lanes' `Retriever` interface, not their internals.
- **Reciprocal Rank Fusion** — each page scored by its **position** in each lane's list (`1/(RRFk + rank)` summed across the lanes it appears in), not by the incompatible raw lane scores; no normalization, no tuning data. A page near the top of either lane scores well; near the top of both scores highest.
- **One entry per page** — a page found by both lanes is merged into a single result keyed on `PageID` (the subject id), never listed twice.
- **Exact-name pin** — before fusing, the whole question is checked against existing subjects by exact normalized name via the shared `Resolver` (alias-honoring); on a match that subject's page is placed at **rank 1**, ahead of the fused list, and removed from the fused tail so it isn't repeated. No exact match → the result is purely the fused list.
- `Search(ctx, query, k)` — the one-string form (both lanes get the same string; the simple/exact path and tests).
- `SearchAnalyzed(ctx, qa wiki.QueryAnalysis, k) (retrieve.Result, error)` — the real entry `ask` calls. It fans each sub-query out with **per-lane routing**: the **meaning lane** gets the full natural-language sub-query sentence; the **keyword lane** gets the OR-joined keywords + aliases (sanitized via `ftsPhrase`). Every resulting lane-list (two lanes × each sub-query) is fused in a **single** RRF pass into one deduped ranked list; then the exact-name pin (on the original whole question) and the `FinalK` cap apply. It returns a `retrieve.Result{Hits, TopDense (highest raw cosine seen across the meaning lane this query), Pinned}` — `TopDense` is the raw signal the D09 honest-empty gate needs, which the fused rank score hides.

**Done when:** the suite is green (per design *Conventions*) and these ids are covered by clearly-named tests (driven with mock/spy lane retrievers so fusion is exercised in isolation):

- **R-79KD-1622** — RRF ordering: a page in **both** lanes' lists ranks above a page that is rank 1 in only one lane, per `1/(RRFk+rank)` summed (k=60) — the fused order follows the formula, not either lane alone.
- **R-7AS9-EXSR** — a page returned by both lanes appears exactly **once** in the fused output (deduped by `PageID`), contributions combined.
- **R-7C05-SPJG** — an exactly-named existing subject (normalized-exact via the resolver, alias-honoring) is returned at **rank 1** and not duplicated lower; a question naming no subject exactly yields the fused list with no pin.
- **R-7D82-6HA5** — `Search` requests `PerLane` candidates per lane and returns at most `FinalK` (default 8); raising/lowering the `FinalK` knob changes the count returned.
- **R-Q8RI-7POG** — `SearchAnalyzed` over a two-sub-query "compare X and Y" analysis fans each sub-query to both lanes and fuses **all** lists in one pass into a single deduped list containing **both** X's and Y's page — a result a single blended query does not produce.
- **R-Q9ZE-LHF5** — per-lane routing: under `SearchAnalyzed` the meaning lane receives the full sub-query **sentence** while the keyword lane receives the OR-joined **keywords + aliases** (confirmed with spy lanes capturing the text each was handed).
