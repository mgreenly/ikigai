# webhooks — Plan

**Authority: construction order and history.** This document — and the
`project/plan/` directory it heads — owns the order in which the webhooks service
is built and the record of what has been built. It does **not** restate *why*
(that is `project/product/README.md`) or *how/proof* (that is
`project/design/`); it orders design's Decisions into dependency-respecting
phases and tracks which are done. The plan is **append-only**: a completed phase
is never rewritten or deleted, so this directory doubles as the construction
history. To extend the plan, first update product and design *in place*, then
**append** a new phase — a new `project/plan/phase-NN.md` body plus a new line in
`project/plan/STATUS.md`. Never edit a finished phase except to flip its status
marker in `STATUS.md`.

## One phase = one package = one accumulating context

Each phase is a single coherent unit of work — almost always one Go package —
built in one accumulating context against product and design. A phase reads only
the design Decisions it realizes and the *interfaces* (not the internals) of the
packages earlier phases produced. That bound is what keeps every phase the size
of a small standalone tool no matter how large the service grows: the inner
packages (`internal/db`, the `webhooks` domain package, `internal/mcp`) compile
and test on their own, and the composition root that wires them is itself a late,
self-contained phase. Where a phase must establish a structural seam that design
assigns to a different Decision (e.g. the `Service`/`Clock` types defined in
design D1 but first needed by the secret lifecycle), the seam is born in the
phase that first needs it; the *verification ids* of the owning Decision are
realized by the phase named in `STATUS.md`.

## Done bar

A phase is **done** when every Verification item — each `R-XXXX-XXXX` id — in the
design Decision(s) it realizes (or the slice of those ids assigned to it) is
covered by a clearly-named test and the suite is green. "Green" is defined in
design's *Conventions* (`project/design/README.md`): `cd webhooks` and then
`go build ./...`, `go vet ./...`, and `go test ./...` all exit 0 with no
failures, with tests run against real temp-file SQLite and a deterministic
injected clock — never a mocked store or outbox. "Covered" means what each
Decision's Verification list says it means: a genuine test exercising the named
behavior against the substrate that Decision specifies. For the D7 end-to-end
ids, design's verification-gate-honesty rule applies — an all-skipped end-to-end
layer (because `:8080` was unreachable) is a **gap, not a pass**; the gate must
bring the suite up and run those ids for real.

## Layout

The plan is **split for addressability** so the build loop reads only the one
phase it is working on, never the whole history:

- `project/plan/STATUS.md` — the manifest: one line per phase in build order, and
  the **only** home of a phase's status marker (`⬜` not started / `✅` done).
- `project/plan/phase-NN.md` — one body file per phase, zero-padded (`phase-01.md`,
  `phase-02.md`, …; a sub-phase keeps its suffix, e.g. `phase-07a.md`). The body
  carries **no** status token.
- `project/plan/README.md` — this file: the static, invariant rules. It lists no
  phases and carries no status, so it never grows with the project.

Append-only, restated for this layout: never rewrite or delete a `phase-NN.md`,
never delete a `STATUS.md` line. The only build-time mutation to the plan is
flipping one phase's `⬜ → ✅` in `STATUS.md`.
