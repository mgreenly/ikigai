# Suite on-box layout, versioning & backup/restore — Design

**Authority: shape and its proof.** This directory owns *how* the on-box install
tree, the versioned-binary deploy mechanism, the SemVer 2.0 version contract, and
the per-service backup/restore are built, and *how each behavior is proven*. The
product owns the *why* and the user-facing promises (no `project/product/product.md`
exists yet — the converged intent is summarized in each Decision's prose, to be
back-filled into a product doc later). Design uses the suite's contractual
constants (service names, the `IKIGENBA_*` env names, the event-protocol epoch
rules) **by value** but does not own them. This is the **single, current**
statement of the architecture: when a decision changes, its `DNN.md` is rewritten
in place — decisions are never stacked. History of how it got here lives in the
plan.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in those Verification lists and **nowhere else** — there is
  no separate requirements document.
- Design's responsibility for ids ends at **minting** them here. How coverage is
  measured, what counts as covered, and when the work is "done" are downstream
  phases' concern and are deliberately not specified in this directory.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain.** Go 1.26. Modules are wired for local dev by the
  repo-root `go.work`; the **production build forces `GOWORK=off`** and produces
  one static `linux/amd64` binary per service. Relevant module paths: `opsctl`,
  `appkit`, `eventplane`, and one per service (`crm`, `ledger`, …).
- **Build / typecheck command.** `go build ./...` within a module (workspace
  mode locally). A release artifact is built static for `linux/amd64` with
  `GOWORK=off`.
- **Test command — the green gate.** `bin/test` from the repo root. It runs, fail-fast:
  (1) `bin/check-migrations`, (2) the repo-root shell tests `bin/*.test.sh`,
  (3) `go test ./...` across every workspace module. **"The suite is green"
  means `bin/test` exits 0.** Shell tooling (`bump`, `ship`) is tested by a
  sibling `bin/<name>.test.sh`; opsctl/appkit behavior is tested by `go test`.
- **New dependency.** `golang.org/x/mod/semver` (added to `opsctl/go.mod`) is the
  sole SemVer 2.0 authority — no hand-rolled version parsing survives this design
  (`x/*` deps are in policy; the suite already vendors `golang.org/x/text`).
- **Testability seams.** opsctl roots every filesystem op at a configurable base
  (`OPSCTL_ROOT`, default `/opt`) and a parallel `SysRoot` (default `/`), so the
  whole layout is exercised against a temp dir with no real box. Box-only effects
  sit behind seams the tests stub: `System` (systemd start/stop/is-active),
  `AppRunner` (subprocess invocation of a service binary's verbs), and — added by
  this design (D07) — `ObjectStore` (the S3 object operations). A claim that
  hinges on a **real** external contract (S3 accepting an upload, the event plane
  rejecting a stale cursor, a real filesystem honoring an atomic rename) is
  verified by an id that names that real substrate, never only a fake.

## Layout

The design is **split for addressability** so a build phase reads only the one
Decision it realizes:

- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map. Regenerated whenever a Decision is
  added or its Verification ids change.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded
  `D01.md`, `D02.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/design.md` — this spine: cross-cutting facts only, no
  per-Decision detail.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a new
Decision adds a `DNN.md` and an INDEX entry.

## Resolved design questions (rationale trail)

Three questions were debated during authoring and are now settled inside the
Decisions; recorded here so the *why* survives:

- **Apex TLS cert → backed up/restored** (D07 cert stream). Reissue does not scale
  for mass recovery: Let's Encrypt rate limits are per **registered domain** and
  the whole fleet shares `ikigenba.com`, so a fleet rebuild that reissued would
  exhaust the quota; restoring from S3 consumes none.
- **Served web content → uniform `state/www/{public,private}`** under `state/`
  (D01/D05). Backed up as plain `state/`, no special root, no `sites`-casing.
  nginx serves it via the `web` group + `0711` traverse-only `state/` (DB stays
  private); `public/` is direct, `private/` is introspection-gated.
- **Backup automation → scheduled, restore interactive** (D09). One 3 AM
  America/Chicago systemd timer drives the `opsctl backup --all` box sweep
  (per-service stop·snapshot·start, fail-soft); restore is never scheduled.

No open decisions remain.
