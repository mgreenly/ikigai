# Phase 31 — worker boot sweep: requeue orphaned `working` jobs

*Realizes design Decision 14 (boot sweep) and Decision 4 (worker startup
requeues crash orphans). Depends on Phase 30 (the atomic, no-tx integrate that
makes a requeue safe) and Phase 25 (the `JobStore` lifecycle writes).*

D14(e) / D4 step 5 require the worker, on startup, to requeue any job left in
`working` by a crash or restart — `UPDATE jobs SET status='pending' WHERE
status='working'`. The deployed code has **no such sweep anywhere**: `ClaimPending`
only ever selects `status='pending'`, so a job orphaned in `working` (by a process
restart, or by the pre-Phase-30 deadlock) is stranded forever and never
reprocessed. R-MDN8-RAVN is currently unmet despite D4 being marked realized.

Built as the missing startup requeue:

- A `JobStore` sweep method on the **write** handle (D17 routing) issuing
  `UPDATE jobs SET status='pending' WHERE status='working'`.
- Invoked **once at worker startup, before the worker claims its first job**, so
  every orphaned `working` row is returned to `pending` and re-enters the normal
  claim loop. This is safe precisely because Phase 30's atomic integrate
  guarantees an orphaned job wrote no subjects/claims/pages, so its requeued
  re-integration starts clean.

**Done when:** R-MDN8-RAVN is covered by a clearly-named test — a job left in
`working` at startup is reset to `pending` and reprocessed by the worker — and the
suite is green.
