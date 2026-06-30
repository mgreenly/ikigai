# Phase 1 — Restore recreates `cache/` owned by the service user

*Realizes design Decision 1 (restore reconstructs `cache/`), ids `R-WP3M-PO1V` and
`R-WQBJ-3FSK` — a partial-Decision split: D1's third id `R-WRJF-H7J9` is a
real-substrate (live-box) check verified by the operator out-of-loop, not in this
phase. Depends on no earlier phase.*

## What gets built

Package `internal/opsctl`, the restore path in `backup.go`.

After `state/` is replaced during a restore, opsctl reconstructs `cache/` as an
empty directory owned by `<app>:<app>` (mode `0o755`), so the unprivileged service
user can write to it the moment the unit restarts. Today restore wipes `cache/`
(`replaceStateFromArchive`, backup.go:278) and never recreates it, leaving the
service to `mkdir` under a root-owned `AppDir` — which it cannot, so it
crash-loops.

The chown is privileged and uses the existing `System` seam
(`System.ChownTree(ctx, app, app, l.CacheDir())`, as in `setup.go`). Because the
current filesystem step `replaceStateFromArchive(ctx, l Layout, archive string)`
is a free function with no `System`, the cache recreate+chown step moves onto the
`(*Opsctl).Restore` method (which holds the `Opsctl` receiver and its `System`),
running after `replaceStateFromArchive` succeeds and before the deferred unit
restart. `cache/` is **not** restored from a backup — snapshots archive only
`state/`; it is recreated empty and the service rebuilds its contents.

Observable end state: after `Restore`, `CacheDir()` exists as an empty directory
and a `ChownTree` to owner/group `<app>` was issued for it; the rest of the restore
contract (wipe+replace `state/`, remove `.generation`, restart unit) is unchanged.

## Done when

All of the following hold on identical repo state, from the service root
(`opsctl/`):

- `GOWORK=off go build ./...` exits 0.
- `GOWORK=off go test ./...` exits 0 (suite green).
- Both ids are covered by named tests:
  `grep -rE 'R-WP3M-PO1V|R-WQBJ-3FSK' internal/ --include='*_test.go'` returns ≥ 2
  matching lines, and those tests assert, via the fake `System`:
  - `R-WP3M-PO1V` — after `Restore`, `CacheDir()` exists as an empty directory
    (not absent).
  - `R-WQBJ-3FSK` — `Restore` issued a `ChownTree` for `CacheDir()` with
    owner == group == the app name (not `root`); a root-owned or un-chowned cache
    must fail this assertion.
