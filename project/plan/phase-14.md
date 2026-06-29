# Phase 14 — adopt dropbox into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **dropbox**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

dropbox adopts the uniform layout: DB at `state/dropbox.db`, sidecar at
`cache/dropbox.db.generation` via the unit's exported env; binary as
`libexec/dropbox-v<semver>` selected by `bin/run`; boot reconstruction per Phase
06. dropbox's local Dropbox-folder mirror is a durable/rebuildable classification
call — durable content placed under `state/`, anything re-syncable left outside it.
Backup/restore inherited from `opsctl` (D7).

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**dropbox slice**) — a freshly set-up `/opt/dropbox/` (DB under
  `state/`, sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink)
  boots and passes its `health` check, with durable mirror content under `state/`.
