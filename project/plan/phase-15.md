# Phase 15 — adopt prompts into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **prompts**. Depends on Phase 04 (setup/tree), Phase 06 (config + boot reconstruction).*

prompts adopts the uniform layout: DB at `state/prompts.db`, sidecar at
`cache/prompts.db.generation` via the unit's exported env; binary as
`libexec/prompts-v<semver>` selected by `bin/run`; boot reconstruction per Phase
06. prompts makes the **conscious durable-data classification** the model demands:
its `sandboxes/` durable content is placed under `state/`; its `runs/`-style
operational logs are left in the non-state region (rebuilt/empty on boot).
Backup/restore inherited from `opsctl` (D7); prompts' former
`bin/backup`/`bin/restore` script (which forgot the epoch re-mint) is retired in
Phase 08.

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**prompts slice**) — a freshly set-up `/opt/prompts/` (DB under
  `state/`, durable `sandboxes/` under `state/`, sidecar under `cache/`, binary
  under `libexec/`, `bin/run` symlink) boots and passes its `health` check, with
  non-state `runs/` recreated on boot.
