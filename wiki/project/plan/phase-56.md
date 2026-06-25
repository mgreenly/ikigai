# Phase 56 — The meaning lane: in-memory vector search

*Realizes design Decision 32 (the meaning lane: in-memory vector search). Depends on Phase 52 (the D08 seam), Phase 53 (D30 — `EmbeddingStore.LoadAll` hydrates the cache) and Phase 54 (D34 — the query-side embed). The cache built here is the structure the background lifecycle (Phase 59) keeps current.*

The "same idea, different words" lane: hold every page's vector in RAM, embed the question, and return the closest pages by cosine — wrapped behind the search seam.

In `internal/retrieve`:

- `vectorCache` — every page's vector in memory behind a `sync.RWMutex` (many searches read while the background updater occasionally writes one entry): a slice of `vectorEntry{SubjectID, Title string, Vec []float32}` with `Replace(all []vectorEntry)` (startup hydrate), `Upsert(e vectorEntry)` (after a re-embed), and `nearest(q []float32, k int) []retrieve.Hit` (top-k by cosine).
- Scoring is a **dot product**: page vectors and the query vector both come back L2-normalized, so cosine is just their dot product. `nearest` computes it against every cached entry, keeps the top `k`, and returns them as `Hit`s (`PageID` = subject id, `Score` = the cosine, plus title). A plain in-RAM scan — no ANN index (D30 rejected the CGO extension that would need one).
- `vectorRetriever` satisfying `retrieve.Retriever`: holds an injected `embed func(ctx, text) ([]float32, error)` (the D34 query-side embed) and the `*vectorCache`; `Search` embeds the question with the **query** input role (not the document role — pages were embedded in document mode, D34), then scans the cache.

The composition root hydrates the cache once at startup from `EmbeddingStore.LoadAll` and injects the query-side embed func built in Phase 54; keeping the cache current after writes is Phase 59's job.

**Done when:** the suite is green (per design *Conventions*) and these ids are covered by clearly-named tests, the concurrency one exercised under the race detector:

- **R-3WOB-6U4Q** — given a cache of page vectors and a query vector, `Search` returns pages ordered by descending cosine, limited to `k` — the nearest vector ranks first, a near-orthogonal page ranks last or is excluded.
- **R-3XW7-KLVF** — `Search` embeds the question with the **query** input role (confirmed via a capturing fake embedder), so the two sides (query here, document in D34) are encoded for each other.
- **R-3Z43-YDM4** — the cache hydrates from `EmbeddingStore.LoadAll` at startup, and after a re-embed `Upsert` a later `Search` ranks by the updated vector (the in-memory copy stays consistent with the store).
- **R-40C0-C5CT** — concurrent `Search` reads while `Upsert` writes run race-free under the race detector with concurrent goroutines (the lock discipline holds).
