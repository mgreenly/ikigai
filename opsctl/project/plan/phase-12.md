# Phase 12 — replace the web-group served-tree model with service-user ownership

*Realizes design Decision 7 (`project/design/D07.md`, id `R-3K9X-IPJZ`), Decision 8
(`project/design/D08.md`, id `R-3MPQ-A91D`), and Decision 9 (`project/design/D09.md`,
id `R-3NXM-O0S2`). Depends on phase 11 (the two-tier `public`/`private` shape).
Atomic across the three paths because they share the `ensureWWWPerms` helper being
removed. Touches `internal/opsctl/initbox.go` (drop the `web` group + membership
lines), `internal/opsctl/www.go` (delete the helper file), `internal/opsctl/setup.go`
(served-tree branch chowns state to `app:app`), `internal/opsctl/deploy.go` (drop the
`ensureWWWPerms` call), `internal/opsctl/backup.go` (restore reconstitutes `state/www`
to `app:app`), and the affected tests.*

Retire the `web`-group/setgid served-tree model — dead since sites moved to
proxy-pass serving (D7) — and replace it with plain service-user (`app:app`)
ownership on each path. The observable end state:

- **init-box.** `initbox.go` no longer creates the `web` group or adds `nginx` to
  it (the two `EnsureSystemGroup("web")` / `AddUserToGroup("nginx","web")` calls are
  gone). nginx install, apex block, and cert flow are unchanged.
- **setup.** The served-tree branch (`len(opts.WWWDirs) > 0`) creates `public/` and
  `private/` at `0750`, then chowns the state tree to the service user with
  `ChownTree(app, app, l.StateDir())` (covering `state/www`) — replacing the
  `ensureWWWPerms` call. No `web` chown, no setgid.
- **deploy.** The `ensureWWWPerms` call after the state-ownership chown is removed;
  the existing recursive `ChownTree(app, app, l.StateDir())` already leaves
  `state/www` owned `app:app`.
- **restore.** The `ensureWWWPerms` call is replaced by a guarded
  `ChownTree(app, app, l.WWWDir())` after `replaceStateFromArchive`, alongside the
  existing `cache/` reconstitution (D01), before the deferred restart.
- **helper.** `internal/opsctl/www.go` (`ensureWWWPerms`) is deleted; no path calls
  it. (The now-unused `Chmod`/`EnsureSystemGroup`/`AddUserToGroup` seam methods are
  removed in phase 13.)
- Tests for the retired ids (`R-AVIE-SOYW`, `R-AWQB-6GPL`, `R-AZ63-Y06Z`, `R-QEPF-HJ11`,
  `R-AQMT-9M04`) are removed with their behavior; the new-ownership tests replace them.

Non-goals: no removal of the seam methods or the dead `stateWWWFragment` yet (phase 13),
no change to the DEFAULT/worker branches, no world-readable www mode.

**Done when** the suite is green — `GOWORK=off go build ./...` succeeds and
`GOWORK=off go test ./...` passes from `opsctl/` — and these ids are each covered by a
clearly-named test (temp `OPSCTL_ROOT` + fake `System`):

- **R-3K9X-IPJZ** — after `Setup` of a served-tree app (`sites`), the fake records
  `ChownTree(app, app, StateDir())` (covering `WWWDir()`) and records **no**
  `ChownTree(_, "web", _)` and **no** `Chmod` of a www path; `public/`+`private/` exist
  at `0750`. Fails against the prior helper (recorded `ChownTree(app,"web",WWWDir())` +
  `Chmod(02750)`).
- **R-3MPQ-A91D** — after `Deploy` of a served-tree app, the fake records the recursive
  `ChownTree(app, app, StateDir())` and records **no** `ChownTree(_, "web", _)` and **no**
  www `Chmod`. Fails against the prior `deploy.go` (recorded a `web` re-chown after the
  sweep).
- **R-3NXM-O0S2** — after `Restore` of a served-tree app, the fake records
  `ChownTree(app, app, WWWDir())` ordered after the state replacement and before the
  deferred restart, plus the existing `cache/` chown, and records **no**
  `ChownTree(_, "web", _)` and **no** `Chmod`. Fails against the prior restore (recorded a
  `web` re-chown + setgid).

Operator-verified out-of-loop (not loop-driven), re-exercising the live-box ids after a
real deploy/restore on `int.ikigenba.com`: **R-AXY7-K8GA** and **R-B0E0-BRXO** — an
anonymous `GET …/srv/sites/public/<site>/` returns 200.
