# Phase 53 — Page-embedding storage: the `page_embeddings` table, `EmbeddingStore`, and the float32↔blob codec

*Realizes design Decision 30 (where page embeddings are stored). Depends on the D17 split read/write handles and the D3 store pattern (the `sqlStore`/`SubjectStore`/`PageStore` family in `internal/wiki`) — no new-phase dependency. Consumed later by the meaning lane (Phase 56) and the background lifecycle (Phase 59).*

The wiki gains a place to keep each page's vector, separate from the hot `pages` row, plus the small store and codec the rest of the search machinery reads and writes it through.

In `internal/wiki`:

- A new **forward migration** (generated with `bin/new-migration wiki page_embeddings`; **never** hand-name the version) creates `page_embeddings(subject_id TEXT PRIMARY KEY, model TEXT NOT NULL, dims INTEGER NOT NULL, vec BLOB NOT NULL, content_hash TEXT NOT NULL, updated_at INTEGER NOT NULL)` — epoch-ms `updated_at`, no `FOREIGN KEY` (suite style; the comment records that `subject_id == pages.subject == subjects.id`, and the PK guarantees at most one vector per page).
- An `Embedding` value type `{SubjectID, Model string, Dims int, Vec []float32, ContentHash string, UpdatedAt int64}` (the vector L2-normalized as handed back by the embedder).
- `EmbeddingStore` over the same read/write handles (D17) as the sibling stores, with `Upsert(ctx, Embedding) error` (insert-or-replace the single row for a subject) and `LoadAll(ctx) ([]Embedding, error)` (every row, vectors intact — the source the meaning-lane cache hydrates from).
- A pure, package-local codec: `encodeVec([]float32) []byte` (little-endian pack) and `decodeVec([]byte) ([]float32, error)` that **errors** on a byte length not a multiple of 4 (fail loud, never silently drop trailing bytes).

The composition root constructs the `EmbeddingStore` alongside the other stores; nothing else changes yet (no page write computes a vector — that arrives in Phase 59).

**Done when:** the suite is green (per design *Conventions*, including `bin/check-migrations wiki` accepting the new migration and rejecting edits to committed ones) and these ids are covered by clearly-named tests — the round-trip/codec test pure, the store tests against a **real** temp SQLite migrated by the appkit runner:

- **R-9OCK-FJK1** — encode→decode round-trips a float32 vector element-for-element (same length and order, little-endian); a blob whose length isn't a multiple of 4 returns an error.
- **R-9PKG-TBAQ** — a second `Upsert` for an existing subject **replaces** the row (new model/dims/vec/content_hash/updated_at) and leaves exactly one row — never a duplicate.
- **R-9QSD-731F** — `Upsert` then read round-trips every column against real SQLite: the vector read back is identical, and model/dims/content_hash match.
- **R-9S09-KUS4** — `LoadAll` returns every stored embedding with its vector intact, so the in-memory cache can be filled from it.
