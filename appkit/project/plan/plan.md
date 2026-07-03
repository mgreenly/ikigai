# appkit — Plan (manifest read-path through `current`)

**Authority: construction order and history.** This document and the
`project/plan/` directory it heads own the **build order** of the manifest
read-path fix and the **record of what has been built**. The plan is
**append-only**: completed phases are never rewritten or deleted, so the plan
doubles as the construction history. To extend the work, update the design
(`project/design/design.md` + `project/design/`) **in place**, then **append** a
new phase here — a new `project/plan/phase-NN.md` plus a new line in
`project/plan/STATUS.md`. Never edit a finished phase except to flip its status
marker in `STATUS.md`.

**One phase = one coherent increment.** Each phase is a single coherent unit sized
for one subagent, built against the design Decision(s) it realizes (resolved
through `STATUS.md` → `phase-NN.md` → the brief). The phases are **sequential and
dependency-ordered**: the reader must move onto `current` (D1) before the local
launcher is re-shaped to feed it (D3), and both must be true before the dead
sibling path and its box artifacts are retired (D4).

**Boundary note.** Only D1 is `appkit/`-package work. D2 (`bin/registry`), D3
(`bin/start`), and D4 (source-wide cleanup + a live-box action) deliberately cross
the `appkit/` boundary into repo-root operator scripts and one production step —
they are part of this one layout-parity fix, not appkit-package code, and are
verified by shell tests / live smokes rather than the appkit Go suite. A phase
that touches the live box (D4) is an explicit operator step, not an unattended
loop build.

**Done bar.** A phase is **done** when every Verification id in the design
Decision(s) it realizes is covered by a clearly-named, genuinely-asserting test (or
the named live check) and the relevant suite is green. For appkit "green" is
defined in design's *Conventions*: from `appkit/`, `go build ./...`,
`go vet ./...`, `gofmt -l .` (no output), and `go test ./...` all succeed. For the
shell collaborators, the named script (`bin/registry.test.sh`) passes; for the
integration/box ids, the named live check passes.

## Layout

The plan is physically split so the build loop reads only what it needs:

- `project/plan/STATUS.md` — the manifest: one line per phase in build order, and
  the **only** home of status markers (`✅` done / `⬜` not started).
- `project/plan/phase-NN.md` — one body file per phase (zero-padded). A phase body
  carries **no** status token — status lives only in `STATUS.md`.

## Operator steps (outside the unattended loop)

Some Verification lives on the **live `int` box** and cannot be a `STATUS.md`
phase: the unattended loop is forbidden to `ssh int` / `opsctl` / mutate the box,
so an id whose only pass-predicate is a live-box command could never flip ⬜→✅ and
would make the loop non-convergent. Such ids are recorded here as explicit operator
steps, run **only on explicit instruction to deploy**, and are deliberately **absent
from `STATUS.md`** so `gather`/`verify` never treat them as loop work.

**R-YU3O-6CQP — box end-state stands on `current` alone (D4, operator-verified).**
Prerequisite: Phase 01 (the `current` reader) is merged and the dashboard rebuilt.
Sequenced so crm never drops out mid-change:

1. Deploy the Phase 01 dashboard to the box: `bin/ship dashboard` → `opsctl stage`
   → `opsctl deploy` (dashboard last, per `deploy.md`).
2. Enumerate stale siblings — `ssh int 'ls /opt/*/etc/manifest.env'` — then remove
   the hand-placed bridge and leftovers, e.g.
   `ssh int 'sudo rm -f /opt/crm/etc/manifest.env /opt/ledger/etc/manifest.env'`.
3. Restart the dashboard so its OAuth-AS resource list re-derives with crm present
   (`deriveResources` runs at startup): `ssh int 'sudo systemctl restart dashboard'`.
4. Verify: `curl -s https://int.ikigenba.com/services` still lists `crm` (resolved
   through `/opt/crm/etc/current/manifest.env`, no sibling present), and a
   `/srv/crm/mcp` request is now token-mintable (crm is in the AS resource set).

Until this runs, the mid-investigation bridge symlink `/opt/crm/etc/manifest.env`
stays on the box on purpose — removing it before the `current` reader ships would
re-hide crm.
