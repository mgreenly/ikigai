# eventplane — Plan

**Authority: construction order and history.** This document and the
`project/plan/` directory own the build order and the record of what's built.
The plan is **append-only**: to extend the project, update product and design
in place first, then append a new `phase-NN.md` and its `STATUS.md` line —
never edit a finished phase except to flip its marker. **Coverage invariant:**
every *current* design Verification id is realized in exactly one phase.
Coverage is one-directional — the plan may also carry retired ids inside
frozen phases whose behavior has since left design; a current id minted later
is covered by a newly appended phase, never by rewriting an old one.

**One phase = one package = one build-turn context.** Each phase is a single
coherent unit of work — almost always one package — scoped to its design
Decisions and the *interfaces* of what it depends on, and sized so the build
loop can carry it in one fresh build-turn context. If a Decision is too large
for one context it is split across phases, each naming its slice of the
Decision's Verification ids.

**Done bar.** A phase is done when every Verification id it realizes (or its
explicit slice) is covered by a clearly-named test and the suite is green —
"green" as defined in design's Conventions (`go test ./...` and
`go vet ./...` from `eventplane/`, both exit 0). Every phase's acceptance bar
is deterministic exit conditions — never a prose judgment, never a
self-referential or unsatisfiable check.

## Layout

`STATUS.md` is the manifest and the **only** home of status markers;
`phase-NN.md` is one body file per phase (zero-padded; sub-phases keep their
suffix, e.g. `phase-07a.md`); this README is the static rules. Append-only in
the layout too: never rewrite or delete a `phase-NN.md`, never delete a
`STATUS.md` line; the only build-time mutation is flipping one phase's
`⬜ → ✅` in `STATUS.md`.
