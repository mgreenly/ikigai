# Phase 21 — adopt webhooks into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **webhooks**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

webhooks adopts the uniform layout: DB at `state/webhooks.db`, sidecar at
`cache/webhooks.db.generation` via the unit's exported env; binary as
`libexec/webhooks-v<semver>` selected by `bin/run`; boot reconstruction per Phase
06. Backup/restore inherited from `opsctl` (D7). (webhooks is the twelfth
deployable service — present in `go.work` with a `VERSION`, though absent from the
CLAUDE.md service table — and adopts identically to the rest.)

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**webhooks slice**) — a freshly set-up `/opt/webhooks/` (DB under
  `state/`, sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink)
  boots and passes its `health` check.
