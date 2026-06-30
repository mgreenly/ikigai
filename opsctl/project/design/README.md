# opsctl — Design

**Authority: shape and its proof.** This directory owns *how* opsctl is built and
*how each behavior is proven*. The product (`project/product/product.md`) owns the
*why* and the user-facing promises; design states the **exact, checkable form** of
those promises and never re-declares the why. Design *uses* the product's
contractual constants by value but does not own them. This is the **single,
current** statement of the architecture: when a decision changes its file is
rewritten in place (never stacked); the history of how it got here lives in the
plan.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and **nowhere else** — there is
  no separate requirements document.
- Design's responsibility for ids ends at **minting them into this doc**. How
  coverage is measured, what counts as covered, and when work is "done" are not
  design's concern and are owned by downstream phases.

## Conventions

Shared facts every Decision leans on.

- **Language / module.** Go (`go 1.26`); module path `opsctl`. opsctl is on-box
  CLI tooling and is **not** release-versioned.
- **Build / typecheck command.** `GOWORK=off go build ./...` from the service
  root (`opsctl/`). The production build always forces `GOWORK=off`; design and
  tests assume the same so behavior matches the deployed binary.
- **Test command.** `GOWORK=off go test ./...` from the service root.
- **"The suite is green"** means: `GOWORK=off go build ./...` succeeds **and**
  `GOWORK=off go test ./...` passes with no failures, from `opsctl/`.
- **Privilege / IO seam.** opsctl runs as root on the box (via `sudo`) and
  performs privileged filesystem and unit operations through a `System` seam
  (e.g. `System.ChownTree(ctx, owner, group, path)`), which is faked in unit
  tests and real on the box. Claims whose correctness depends on the *real*
  service user being able to read/write a path cannot be falsified by the fake
  and carry a real-substrate (live-box) Verification id.
- **Real substrate for live claims.** The live box for end-to-end verification is
  `int.ikigenba.com` (`ssh int`); opsctl there needs the box env loaded:
  `sudo bash -c 'set -a; . /etc/ikigenba/env; opsctl <verb> …'`.

## Layout

The design is **split for addressability** so a build phase reads only the one
Decision it realizes:

- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map. Regenerated whenever a Decision is
  added or its Verification ids change.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded
  filename `D01.md`, `D02.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/README.md` — this spine: static cross-cutting facts only, no
  per-Decision detail.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a new
Decision adds a `DNN.md` and an INDEX entry.
