# Phase 30 — atomic, no-tx-held integrate (D14(b) conformance)

*Realizes design Decision 14 (atomic, idempotent integrate that holds no DB
transaction across the LLM calls) and Decision 17 (the write connection is idle
during a job's LLM phase); preserves Decision 4's anti-poisoning seam. Depends on
Phase 25 (the D14 abort/re-run lifecycle and the integrate transaction this phase
rewrites), Phase 24 (the D13 recorder write path), and Phase 29 (the D17
`Conns{Read,Write}` split — the single write connection).*

This phase brings `Service.integrate` (`internal/wiki/service.go`) back into
conformance with the shape D14(b) already specifies. The deployed code opens
`s.write.BeginTx` and holds that transaction **across every `compile` LLM call**,
then writes the terminal status (`FinishWorking`) *outside* the transaction. On
the D17 single-connection write handle this is the exact deadlock D17(b) names:
each eager recorder write (D13) for a `compile` round-trip starves waiting for the
one write connection the open integrate transaction holds, so the single worker
wedges in `working` for the whole LLM phase and never commits. It also blocks all
other writes (status, `abort`, `rerun`, new `ingest`) for the duration, violating
D17(c).

Rebuilt to the D14(b) sequence — **no transaction held across the LLM calls**, one
short conditional commit:

- *(reads, no tx)* Read this job's existing claims → the subjects it previously
  touched (`oldSubjects`; empty on a first run), via the read handle.
- *(no tx)* `extract(source_text)` → new subjects + claims; resolve existing /
  mint new subject ids in memory; `affected = oldSubjects ∪ newSubjects`. For each
  affected subject compute its **post-replacement** claim set in memory —
  `(persisted claims − this job's old claims) + this job's new claims` — and
  `compile` its page. `compile` still receives only subject identity + claims and
  no page body (the anti-poisoning seam stays structural). Every LLM call here is
  recorded eagerly per D13, now acquiring the idle write connection between
  round-trips with nothing held.
- *(one commit tx on `Write`, conditional)* `DELETE FROM claims WHERE job_id=?` →
  upsert affected subjects (first-writer-wins `type`, D3) → insert this job's new
  claims → for each affected subject upsert its recompiled page **or delete the
  page when its final claim set is empty** (subject row retained) →
  `UPDATE jobs SET status='done', finished_at=now WHERE id=? AND status='working'`.
  The terminal-status write lives **inside** this tx and is guarded on
  `status='working'`: if it touches **0 rows** (a concurrent abort flipped the job
  to `aborted`) → **rollback**, so nothing lands. Any step-2 error → the same
  guarded `UPDATE … status='failed', error=… WHERE id=? AND status='working'`,
  writing no subjects/claims/pages. The eager `llm_calls` rows are outside this tx
  and survive a rollback (intended).

**Done when:** R-MB7F-ZRE9 (integration error ends the job `failed` with zero
subjects/claims/pages — atomic), R-0TKT-MXFO (aborting a `working` job cancels its
in-flight LLM call and commits zero subjects/claims/pages via the conditional
rollback), R-0YGF-60EG (after a re-run the job's claims are exactly its new
extraction's, none duplicated, affected pages recompiled), R-0ZOB-JS55 (a re-run
that no longer names a previously-touched subject recompiles it from the reduced
set and deletes the page when the final set is empty), and R-FWS5-ACM0 (with the
write connection idle during the simulated LLM phase, a read and a concurrent
control write both complete promptly — a slow job wedges neither reads nor control
writes) are each covered by a clearly-named test; R-MG31-IUD1 (compile receives no
page-body input) remains covered; and the suite is green.
