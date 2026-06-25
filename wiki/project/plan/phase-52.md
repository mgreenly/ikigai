# Phase 52 — The search seam (`internal/retrieve`): `Hit`, `Retriever`, `Result`, `SearchLimits`

*Realizes design Decision 8 (Search returns: hybrid retrieval over pages, behind one seam). Depends on no earlier phase — this is the foundational boundary the lane and fusion phases (55, 56, 58) and the `ask` rewrite (60) all plug into.*

A new package `internal/retrieve` exists holding **only** the contract every search component agrees on — no lane, no fusion, no DB. It is the seam D08 owns: `ask` and the lanes depend on these types and nothing of each other's internals.

In `internal/retrieve`:

- `Hit` — `{SubjectID, PageID, Score float64, Snippet, Title string}`; `PageID == SubjectID` is the stable key used both to merge/dedupe hits and to cite the page in the final answer.
- `Retriever` — the one-method interface `Search(ctx context.Context, query string, k int) ([]Hit, error)`. Every lane and the fused retriever satisfy it; `ask` depends on this and nothing more.
- `Result` — `{Hits []Hit, TopDense float64, Pinned bool}`, the richer return the fusion entry (D33) hands back so the honest-empty gate (D09) can judge relevance on the raw meaning-lane cosine (`TopDense`) rather than the fused rank score, and know whether an exact-name page was pinned.
- `SearchLimits` — `{Default, Cap int}` with `Resolve(k int) int`: `0 → Default`, otherwise clamp into `[1, Cap]`.

This package is pure types plus one small pure function (`SearchLimits.Resolve`); it imports nothing from the rest of the service, so the lanes can import it without a cycle.

**Done when:** the suite is green (per design *Conventions*). D08 is a structural boundary and mints **no** `R-XXXX-XXXX` ids of its own — its correctness is proven by the lane and `ask` phases that sit behind it. The only acceptance bar here is that the package compiles, `go vet`/`gofmt` are clean, and `SearchLimits.Resolve` is exercised by a clearly-named table test covering the `0→Default` and `[1,Cap]` clamp behavior (the lone behavior the seam carries, even though it has no minted id).
