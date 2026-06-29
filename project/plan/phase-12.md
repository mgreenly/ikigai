# Phase 12 — adopt ledger into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **ledger**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

ledger adopts the uniform layout: DB at `state/ledger.db`, sidecar at
`cache/ledger.db.generation` via the unit's exported env; binary as
`libexec/ledger-v<semver>` selected by `bin/run`; boot reconstruction per Phase 06.
Backup/restore is inherited from `opsctl` (D7).

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**ledger slice**) — a freshly set-up `/opt/ledger/` (DB under
  `state/`, sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink)
  boots and passes its `health` check.
