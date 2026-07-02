# Phase 2 — Stage unpacks on the OPSCTL_ROOT filesystem (no cross-device rename)

*Realizes design Decision 2 (stage unpacks into a temp dir on the OPSCTL_ROOT
filesystem), id `R-65MT-7QEK` — a partial-Decision split: D2's second id
`R-66UP-LI59` is a real-substrate (live-box) check verified by the operator
out-of-loop, not in this phase. Depends on no earlier phase.*

## What gets built

Package `internal/opsctl`, the `stage` path in `deploy.go`.

Today `Stage` creates its unpack scratch directory with
`os.MkdirTemp("", "opsctl-stage-*")` (`deploy.go:40`) — the empty first argument
resolves to the OS temp dir (`$TMPDIR`/`/tmp`). It then places each unpacked tier
into the app tree under `OPSCTL_ROOT` via `replacePath` → `os.Rename`. On a box
where `/tmp` and `OPSCTL_ROOT` (`/opt`) are separate filesystems, that rename
fails with `EXDEV` and every `stage` aborts.

Introduce the seam from D2: a `Layout.stagingParent()` helper returning
`Layout.Root`, and have `Stage` create its scratch dir with
`os.MkdirTemp(l.stagingParent(), "opsctl-stage-*")`. The scratch tree then lives
on the same filesystem as every rename destination, so tier placement is always
intra-device. `Layout.Root` is non-empty by construction (`NewLayoutSys` defaults
`""` to `/opt`). The `defer os.RemoveAll(tmp)` cleanup and every other stage step
(unpack, preflight, SHA collision guard, tier placement) are unchanged.

Observable end state: the stage scratch directory is created under `OPSCTL_ROOT`,
never under the OS temp dir; a stage against a layout whose root is a test dir
still places all three tiers correctly.

## Done when

All of the following hold on identical repo state, from the service root
(`opsctl/`):

- `GOWORK=off go build ./...` exits 0.
- `GOWORK=off go test ./...` exits 0 (suite green).
- Id `R-65MT-7QEK` is covered by a named test:
  `grep -rE 'R-65MT-7QEK' internal/ --include='*_test.go'` returns ≥ 1 matching
  line, and that test asserts that the stage staging parent for a layout rooted at
  a test dir equals `Layout.Root` (that test dir) and is **not** `""` and **not**
  `os.TempDir()` — a `MkdirTemp("", …)` (OS-temp) implementation must fail it.
