# appkit — Design (manifest read-path: one indirection through `current`)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the manifest read-path is shaped and *how each
behavior is proven*. The product (`project/product/product.md`) owns the *why* and
the user-facing promises; design states the **exact, checkable form** of those
promises and never re-declares the why. (No `product.md` exists yet for appkit;
the why is carried by reference: services deployed to the box were vanishing from
the dashboard's service list because manifest readers read a *sibling* file next
to the deploy `current` symlink instead of reading *through* it, so a freshly
set-up service — crm — that never got the unmaintained sibling was silently
dropped.) This design is the **single current** statement, rewritten in place
(stale decisions removed, not stacked); the history of how it got here lives in
the plan.

This design is scoped narrowly: move every manifest reader onto the single
per-app `etc/current` deploy symlink (D1–D2), make the local dev runtime layout
mirror the box so one code path serves both (D3), and retire the now-dead stable
sibling path plus its hand-placed artifacts (D4). It deliberately makes **no**
`opsctl` change: the box already publishes `etc/current/manifest.env`, so reading
through `current` makes prod correct with no deploy-tool edit.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is **no
  separate requirements document**.
- Design's responsibility for ids ends at minting them into this doc. How coverage
  is measured and when the work is "done" are **not** design's concern —
  downstream phases own that.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain:** Go **1.26**, single module `module appkit` rooted at
  `appkit/`. The manifest reader is `appkit/inventory` over `appkit/manifest`
  (the shared `KEY=value` parser); both are consumed by the dashboard via a
  committed `replace appkit => ../appkit`.
- **Build / typecheck command:** `cd appkit && go build ./...` and `go vet ./...`.
  The isolated-module check (mirroring the production build) adds `GOWORK=off`.
- **Test command:** `cd appkit && go test ./...`. **"The suite is green"** means
  `go build ./...`, `go vet ./...`, `gofmt -l .` (no output), and `go test ./...`
  all succeed with zero failures, from `appkit/`.
- **Cross-module collaborators (outside `appkit/`).** Two readers and the local
  launcher are not Go and not under `appkit/`: the repo-root shell scripts
  `bin/registry` and `bin/start`. They conform to the same contract and are
  verified by `bin/registry.test.sh` and a live `bin/start` smoke, **not** by the
  appkit Go suite. Phases that touch them (D2, D3, D4) are called out in the plan
  as crossing the `appkit/` boundary; they are part of this one layout-parity fix,
  not appkit-package work.
- **This change touches no schema and no `opsctl` code.** The box's deploy layout
  is unchanged; only how readers address it changes.

## Layout

The design is **split for addressability** so the build loop reads only the one
Decision a phase realizes:

- `project/design/INDEX.md` — the manifest: each Decision → its `DNN.md`, plus a
  sorted `R-id → Decision/file` reverse map (the grep target for id lookup).
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded
  `D01.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/design.md` — this spine: static cross-cutting facts only.

Design is rewritten in place, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a new
Decision adds a `DNN.md` and an INDEX entry.
