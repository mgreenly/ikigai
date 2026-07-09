# Phase 13 — remove the orphaned served-tree seams and dead nginx fragment

*Realizes design Decision 7 (`project/design/D07.md`, id `R-3LHT-WHAO` — the orphan-sweep
verification). Depends on phase 12 (which removed the last callers of the `web`-group and
setgid seams). Touches `internal/opsctl/seam.go` (drop the now-unused `Chmod`,
`EnsureSystemGroup`, `AddUserToGroup` interface methods + `RealSystem` impls),
`internal/opsctl/setup.go` (delete the dead `stateWWWFragment` alias generator), the fake
`System` in the test support, and any test that referenced those symbols.*

With phase 12 gone in, three `System` seam methods (`Chmod`, `EnsureSystemGroup`,
`AddUserToGroup`) have no remaining caller, and `stateWWWFragment` — the old `alias`-to-disk
nginx fragment generator — is dead (setup emits the committed proxy-pass fragment via
`opts.Fragment`, never this). Remove all of it so no orphaned served-tree code survives the
model change. The observable end state:

- **seam.** `System` no longer declares `Chmod`, `EnsureSystemGroup`, or `AddUserToGroup`;
  their `RealSystem` implementations and the fake's recorders/methods are gone. Every other
  seam method (`ChownTree`, `EnsureSystemUser`, `InstallPackages`, unit ops, …) is untouched.
- **fragment.** `stateWWWFragment` and its test are deleted; the served-tree nginx routing is
  owned entirely by the committed `sites/etc/nginx.conf` staged via `opts.Fragment`.
- No behavior changes — this phase only deletes code that nothing calls.

Non-goals: no change to any remaining seam method, to setup/deploy/restore behavior (phase 12),
or to the fragment actually shipped for sites.

**Done when** the suite is green — `GOWORK=off go build ./...` succeeds and
`GOWORK=off go test ./...` passes from `opsctl/` — and this id is covered:

- **R-3LHT-WHAO** — a `project/`-excluded grep over `opsctl/internal/` finds **none** of
  `ensureWWWPerms`, `stateWWWFragment`, `EnsureSystemGroup`, `AddUserToGroup`, a `Chmod(`
  seam method on `System`, or the `"web"` group literal, and `internal/opsctl/www.go` does
  not exist; the build and full test suite are green with those symbols removed. Fails today:
  each symbol is present. *(check: scoped grep + green build/test)*
