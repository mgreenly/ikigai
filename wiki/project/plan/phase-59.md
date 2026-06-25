# Phase 59 — Keeping page vectors current, in the background

*Realizes design Decision 35 (keeping page vectors current, in the background). Depends on Phase 53 (D30 — `EmbeddingStore`), Phase 54 (D34 — the recording embedder), and Phase 56 (D32 — the `vectorCache`), and touches the D04 ingest worker and the D26 merge path (Phases 07/43). It supplies the vectors the meaning lane (Phase 56) searches.*

Page embedding always happens **after** a page is safely committed — never inside the ingest transaction — and the meaning lane keeps using a page's existing vector until a fresh one replaces it. Two cooperating paths share one helper.

- **`embedAndStore` (`internal/wiki`, on `Service`).** `embedAndStore(ctx, p wiki.Page) error` computes the current page fingerprint (the existing `hashText` over title+body), embeds the body in **document** role (D34), and overwrites the stored vector (`EmbeddingStore.Upsert`) **and** the in-memory cache entry (`vectorCache.Upsert`) in place. Never run inside a transaction.
- **After-commit path (`internal/worker`).** Once the ingest/merge worker's atomic commit succeeds, it embeds the pages that job just wrote, running with the job id on the context (`WithJobID`) so the `embed-page` records tie to that ingest/merge job. If the embed then fails, the **job still completes `done`** with its page intact (the commit is not rolled back) — the page just temporarily lacks a fresh vector, left for catch-up.
- **Catch-up sweep (a new `Spec.Workers` entry).** A background loop independent of the ingest worker that drains the "needs embedding" set, then waits on a doorbell (nudged after each commit) with a poll-timeout fallback, exiting only on `ctx.Done()`. Its embeds are maintenance, so they carry **no** job id. A page **needs** a (re-)embed when it has no `page_embeddings` row, **or** its stored `content_hash` ≠ the current title+body fingerprint, **or** its stored `model`/`dims` ≠ the configured embed site (D34); a page current on all three is **skipped**. On first boot every page is "missing," so the sweep backfills the whole corpus with no new verb or migration.
- **Old vector stays live.** When a page is rewritten, its existing `page_embeddings` row and cache entry are **left in place** — still searchable on the slightly-stale vector — until the new vector **overwrites** it. The only vector-less pages are brand-new ones awaiting their first embed; those ride the keyword lane until it lands.

The composition root wires the catch-up worker into `Spec.Workers` and the after-commit nudge into the existing worker.

**Done when:** the suite is green (per design *Conventions*) and these ids are covered by clearly-named tests against a **real** temp SQLite with mock embedder/recorder (no live call):

- **R-6XNX-FNXO** — embedding runs **outside** the commit: when the after-commit embed errors, the job still ends `done` with subjects, claims, and page intact (commit not rolled back), and the page is left for catch-up.
- **R-6YVT-TFOD** — after a job integrates, its page is embedded and stored/cached, and an `embed-page` `CallRecord` carries that job's id (the after-commit embed is attributed to the ingest job).
- **R-703Q-77F2** — catch-up selection embeds exactly the pages that are missing a vector, whose `content_hash` no longer matches, or whose `model`/`dims` differ — and **skips** a page current on all three (no redundant call).
- **R-71BM-KZ5R** — a rewritten previously-embedded page keeps its prior vector present and searchable (store and cache) until the new vector overwrites it in place — never zero vectors.
- **R-72JI-YQWG** — first-boot backfill: pre-existing pages with no `page_embeddings` rows are all embedded by the catch-up worker with no new verb or migration, and reads/searches stay answerable throughout (keyword lane covers the not-yet-embedded pages).
- **R-73RF-CIN5** — a page whose after-commit embed failed is re-selected by a later catch-up sweep and successfully embedded (a failed embed is retried, not lost).
