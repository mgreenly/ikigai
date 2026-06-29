# Phase 11 — adopt crm into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **crm**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

crm adopts the uniform layout: DB at `state/crm.db`, sidecar at
`cache/crm.db.generation` via the unit's exported env; binary as
`libexec/crm-v<semver>` selected by `bin/run`; boot reconstruction per Phase 06.
Backup/restore is inherited from `opsctl` (D7); crm's former `bin/backup`/
`bin/restore` script is retired in Phase 08.

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**crm slice**) — a freshly set-up `/opt/crm/` (DB under `state/`,
  sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink) boots and
  passes its `health` check.
