# Phase 05 — libexec/ + bin/run symlink swap

*Realizes design Decision 2 (versioned binaries via libexec/ + bin/run symlink swap). Depends on Phase 01 (SemVer ordering for "newest N"), Phase 03 (Layout path scheme), and Phase 04 (the tree the binaries live in).*

opsctl's deploy/rollback/prune are retargeted from the two-level
`bin/run → current → releases/<v>/` indirection to the flat
`bin/run → ../libexec/<svc>-v<semver>` scheme. Deploy places the new
`libexec/<svc>-v<new>` file then repoints `bin/run` with the existing `atomicSwap`
(rename over the symlink); rollback repoints to the still-present prior
`libexec/<svc>-v<old>`; prune keeps the newest N by D3 precedence, never deleting
the live or rollback target. The collision guard keys on `LibexecBinary(v)`. This
is the phase that migrates the **last** consumers of the old `Layout` methods, so
those methods (`ReleasesDir`, `ReleaseDir`, `ReleaseBinary`, `CurrentLink`,
`CurrentBinary`, `DataDir`, `WWWServedDir`) are removed here.

**Done when:** `bin/test` exits 0 and:
- R-3TIQ-ML04 — after deploy of `v<new>`, `bin/run` resolves to
  `libexec/<svc>-v<new>` (the just-placed binary) and `releases/`/`current` do not
  exist (real fs, temp root).
- R-3UQN-0CQT — rollback repoints `bin/run` to `libexec/<svc>-v<old>`, which is
  still present (rollback never deleted it).
- R-3VYJ-E4HI — `bin/run` resolves to a runnable file before and immediately after
  the atomic repoint (no observable dangling state).
- A scoped grep over `opsctl/` (excluding `project/`) for the removed method names
  returns **0** matches — the old indirection is gone, not merely unused.
