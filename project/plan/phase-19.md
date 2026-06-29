# Phase 19 — adopt scripts into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **scripts**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

scripts adopts the uniform layout: DB at `state/scripts.db`, sidecar at
`cache/scripts.db.generation` via the unit's exported env; binary as
`libexec/scripts-v<semver>` selected by `bin/run`; boot reconstruction per Phase
06. scripts makes the **conscious durable-data classification**: durable
script/output content that must survive goes under `state/`; its `runs/` execution
trees that are rebuildable are left in the non-state region (recreated on boot).
Backup/restore inherited from `opsctl` (D7); scripts' former `bin/backup`/
`bin/restore` script (which forgot the epoch re-mint) is retired in Phase 08.

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**scripts slice**) — a freshly set-up `/opt/scripts/` (DB under
  `state/`, durable content under `state/`, sidecar under `cache/`, binary under
  `libexec/`, `bin/run` symlink) boots and passes its `health` check, with
  non-state `runs/` recreated on boot.
