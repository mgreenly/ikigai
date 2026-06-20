# Phase 7 — The ingest pipeline: fire-and-return Ingest + the single integrate/commit worker

*Realizes design Decision 4 (the ingest pipeline and worker). Depends on Phase 2 (stores), Phase 4 (extract), Phase 5 (compile), and Phase 6 (the in-tx FTS sync exercised in commit).*

Wire the stages into the running service: a fire-and-return front door and one
background worker that claims, integrates, and atomically commits ingest jobs.
This phase replaces Phase 1's placeholder worker with the real `worker.Run`.

**What gets built (the observable end state):**

- `wiki.Service.Ingest(ctx, owner, text, title, tags) (jobID, err)` — computes
  `sha256(text)`, INSERTs one `pending` `jobs` row, nudges the worker, returns the
  job id. No LLM/extract on the request path.
- `wiki.Service.JobStatus(ctx, jobID)` — `{status, received_at, started_at?,
  finished_at?, error?}` plus the subject ids the job produced.
- `internal/worker/`: `Run(ctx, svc)` — the single, concurrency-1 `Spec.Workers`
  goroutine. Loop: claim the oldest `pending` job (short tx →
  `status='working'`), integrate with **no db tx held** (`extract` → resolve each
  subject by `normalize(name)` → for each affected subject gather its full
  persisted claim set + this job's new claims → `compile`), then commit in one
  all-or-nothing tx (upsert subjects first-writer-wins `type`/`occurred_at`,
  insert this job's claims, upsert pages `version++` + explicit `pages_fts` sync,
  mark job `done`). Any error → mark `failed` with a non-empty `error`, writing no
  subjects/claims/pages. Doorbell via `sync.Cond` + poll-timeout fallback; exit
  only on `ctx.Done()`. **Boot sweep** on startup requeues `working → pending`.
  The integrate orchestration passes `compile` only the claim set — never page
  text (the anti-poisoning invariant, structurally enforced by the signature).

**Done when:**

- R-M8RN-87WV — `Ingest` returns a job id from a single INSERT, with no
  LLM/extract call on the request path.
- R-M9ZJ-LZNK — a `pending` job is claimed and advances `pending → working →
  done`, producing its subjects, claims, and page.
- R-MB7F-ZRE9 — when integration errors mid-run, the job ends `failed` with a
  non-empty `error` and **zero** subjects/claims/pages are written (atomic commit).
- R-MCFC-DJ4Y — a second job naming an existing subject (same `normalize(name)`)
  appends its claims and bumps that subject's page `version`, creating no
  duplicate subject.
- R-MDN8-RAVN — on worker startup, jobs left in `working` are reset to `pending`
  and reprocessed.
- R-MG31-IUD1 — `compile` is invoked with the subject's claim set and never
  receives page-body input (asserted at the integrate orchestration seam).
- Integration tests wire the worker + real DB + mock provider end-to-end; the
  suite is green.
