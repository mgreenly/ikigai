# Phase 22 — live-box one-time converter

*Realizes design Decision 8 (per-service adoption & live-box migration) — the R-4MSB converter slice. Depends on Phase 04 (the Layout/tree it converts into); the per-service adoptions (Phases 10–21) precede it so every service's code already expects the new layout.*

opsctl gains a one-time conversion that migrates an existing `/opt/<svc>/` from the
old layout to the new one: per service it moves `data/<svc>.db*` → `state/`, moves
the generation sidecar → `cache/`, converts `releases/`+`current` →
`libexec/`+`bin/run`, and repoints the unit's env. It is **idempotent** (safe to
re-run; a half-converted tree completes) and **non-destructive** (no data dropped),
following the existing OLD→new conversion precedent in opsctl. This is the tooling
that migrates the live `int` box — run only on an explicit deploy request, never
from this build loop.

**Done when:** `bin/test` exits 0 and:
- R-4MSB-T2SS — run against a fixture of the **old** layout (`data/<svc>.db`,
  `releases/<v>/`, `current`) under a temp `OPSCTL_ROOT`, the converter produces
  the new layout with the DB intact under `state/` and the live binary reachable
  via `bin/run`, and re-running it is a no-op (idempotent) — opsctl unit test.
