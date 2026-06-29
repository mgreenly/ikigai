# Phase 10 — adopt dashboard into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **dashboard**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

dashboard adopts the uniform layout: DB at `state/dashboard.db`, generation sidecar
at `cache/dashboard.db.generation` via `<APP>_DB_PATH`/`<APP>_GENERATION_PATH` in
the unit's exported env; binary placed as `libexec/dashboard-v<semver>` selected by
`bin/run`; boot reconstruction per Phase 06. Backup/restore is inherited from
`opsctl` (D7) — dashboard carries no backup script and no in-binary S3 path (those
are retired in Phase 08). The apex TLS cert is handled by opsctl's cert stream
(Phase 08a), not by this binary.

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**dashboard slice**) — a freshly set-up `/opt/dashboard/` (DB under
  `state/`, sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink)
  boots and passes its `health` check (per-service boot smoke).
