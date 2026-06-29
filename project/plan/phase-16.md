# Phase 16 — adopt wiki into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **wiki**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

wiki adopts the uniform layout: DB at `state/wiki.db`, sidecar at
`cache/wiki.db.generation` via the unit's exported env; binary as
`libexec/wiki-v<semver>` selected by `bin/run`; boot reconstruction per Phase 06.
Any RAG/index artifacts that are rebuildable from the DB stay in the non-state
region (regenerated on boot); only durable source content lives under `state/`.
Backup/restore inherited from `opsctl` (D7).

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**wiki slice**) — a freshly set-up `/opt/wiki/` (DB under `state/`,
  sidecar under `cache/`, binary under `libexec/`, `bin/run` symlink) boots and
  passes its `health` check, with rebuildable index data recreated on boot.
