# Phase 43 — The merge work item & execution

*Realizes design Decision 26 (the merge work item & execution). Depends on Phase 42 (D25 `AliasStore` / `Resolver` / no-chain invariant), Phase 05 (D7 `Compile`), Phase 25/30 (D14 atomic integrate + the per-job cancel + status guard), Phase 31 (D4/D14 boot sweep).*

A merge runs as a work item on the **same single worker goroutine** as `integrate` — that shared serialization *is* the merge⇄integrate race fix — executed as one all-or-nothing transaction that reuses the existing 3-arg D7 `Compile` (no new prompt).

In `internal/wiki`:

- A new timestamped migration extends `jobs`: `ADD COLUMN kind TEXT NOT NULL DEFAULT 'ingest' CHECK (kind IN ('ingest','merge'))`, plus FK-free carrier columns `merge_winner_id`/`merge_loser_id` (the loser is deleted, so a FK there would block the merge). Authored via `bin/new-migration wiki merge_jobs`. A merge job reuses the `jobs` row with empty `source_text`/`sha256` and the two carriers populated.
- Net-new store methods: `SubjectStore.Delete(id)` and `ClaimStore.RepointSubject(from, to)`.
- `mergeSubjects(ctx, job)` in three phases mirroring `integrate`: **A** resolve winner+loser by PK (`Get`, not `GetByPath`), validate both exist and differ, end `failed` with a clear reason on a stale/already-folded side; **B** union both claim sets and `Compile(ctx, winner, combined)` with **no tx held**; **C** one tx on the write handle with tx-bound stores, in strict order — repoint claims → upsert winner page → delete loser page → **repoint the loser's inbound aliases onto the winner** (before the delete, so the `ON DELETE RESTRICT` FK permits it) → delete loser subject → insert the `normalize(loser.name) → winner` alias → guarded `status='done' WHERE status='working'` → commit; any error rolls back entirely.

In `internal/worker`: `ProcessNext` branches on `job.Kind` after claiming and registering the per-job cancel — `'merge'` → `svc.mergeSubjects`, else `integrate`. The kind-agnostic boot sweep already requeues an orphaned merge; atomicity makes the re-run clean.

(The `jobs`/`jobs_count` `kind` filter that keeps merges out of the default ingest list is **not** built here — it is owned by D16 and lands in Phase 44.)

**Done when:** R-NEFH-U8IO, R-NFNE-809D, R-NGVA-LS02, R-NI36-ZJQR, R-NJB3-DBHG, R-NKIZ-R385, R-NLQW-4UYU, R-NMYS-IMPJ, R-NPEL-A66X are each covered by a clearly-named test on the worker seam against a real temp SQLite (`foreign_keys=ON`) with a scripted mock compiler — the migration/CHECK, dispatch, happy-path fold, atomic rollback on error, the FK-driven repoint-before-delete ordering, the abort-race guard, boot-recovery re-run, the stale-pair no-op, and the serialization proof — and the suite is green.
