# Phase 13 — adopt notify into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **notify**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

notify adopts the uniform layout: DB at `state/notify.db`, sidecar at
`cache/notify.db.generation` via the unit's exported env; binary as
`libexec/notify-v<semver>` selected by `bin/run`; boot reconstruction per Phase 06.
As an event-plane consumer, notify rebuilds its cursor state on boot from the
non-state region per the reconstruction invariant. Backup/restore inherited from
`opsctl` (D7).

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**notify slice**) — a freshly set-up `/opt/notify/` (DB under
  `state/`, sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink)
  boots and passes its `health` check.
