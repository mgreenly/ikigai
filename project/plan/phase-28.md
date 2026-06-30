# Phase 28 — opsctl prune: delete each old version as a complete set

*Realizes design Decision 2 (prune over the versioned bundle). Depends on Phase 24.*

`prune` keeps the newest N versions by D03 precedence and deletes each older version **as a set** —
its `libexec/<svc>-v*` file **and** its `etc/<v>/` (both `nginx.conf` and `manifest.env`) **and** its
`share/<v>/` — never deleting the live version's set or the rollback target's set. This replaces any
release-dir-based pruning with set deletion over the D02 layout.

**Done when:**
- `prune_test.go` tagged `R-1CN2-AZHH`, against a temp `OPSCTL_ROOT` seeded with N+ versions, asserts
  the newest N are kept and each older version's `libexec` file, `etc/<v>/`, and `share/<v>/` are all
  removed, with no orphaned `etc/<v>`/`share/<v>` left behind and the live/rollback sets untouched.
- `bin/test` exits 0.
