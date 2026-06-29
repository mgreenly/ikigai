# Phase 17 — adopt cron into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **cron**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

cron adopts the uniform layout: DB at `state/cron.db`, sidecar at
`cache/cron.db.generation` via the unit's exported env; binary as
`libexec/cron-v<semver>` selected by `bin/run`; boot reconstruction per Phase 06.
Backup/restore inherited from `opsctl` (D7).

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**cron slice**) — a freshly set-up `/opt/cron/` (DB under `state/`,
  sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink) boots and
  passes its `health` check.
