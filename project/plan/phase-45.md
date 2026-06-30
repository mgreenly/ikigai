# Phase 45 — live-box one-time converter delta

*Realizes design Decision 8 (live-box migration). Depends on Phase 24, 25.*

opsctl's one-time conversion is brought up to the reworked design: per service it moves
`data/<svc>.db*` → `state/<svc>.db*` and the sidecar → `cache/`; converts `releases/`+`current` →
`libexec/`+`bin/run`; converts the old single box-generated `etc/manifest.env` and any loose
`etc/nginx.conf` into the versioned, symlink-selected form `etc/<v>/{nginx.conf,manifest.env}` +
`etc/current → <v>`, plus `share/<v>` + `share/current`; and **retires the dead tiers** — the local
`backups/` dir and the flat `data/` dir are removed once their contents have moved. The converter is
**idempotent** (a half-converted tree completes; a re-run is a no-op) and **non-destructive** (moves
precede removals; a removal fires only once its destination exists). It **drops** on-box manifest
generation in favor of the shipped authored file.

**Done when:**
- `convert_test.go` tagged `R-4MSB-T2SS`, against a temp `OPSCTL_ROOT` seeded with the **old** layout
  (`data/<svc>.db`, `releases/<v>/`, `current`, a box-generated `etc/manifest.env`, a stray `backups/`),
  asserts the result: DB intact under `state/`, live binary reachable via `bin/run`,
  `etc/current`/`share/current` resolving, the authored `etc/<v>/manifest.env` in place, and the `data/`
  and `backups/` tiers **removed** — and that a second run is a no-op.
- `bin/test` exits 0.
