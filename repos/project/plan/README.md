# repos — Plan

**Authority: construction order and history.** This document and the
`project/plan/` directory own the build order and the record of what's built.
The plan is **append-only**: to extend the project, update product and design
in place first, then append a new `phase-NN.md` and its `STATUS.md` line —
never edit a finished phase except to flip its one marker. The **coverage
invariant**: every *current* design Verification id is realized in exactly one
phase; coverage is one-directional, so the plan may also carry retired ids
from frozen phases whose behavior has since left design; later current ids
need a newly appended phase.

**One phase = one package = one build-turn context.** Each phase is a single
coherent unit of work — almost always one package — scoped to that unit's
design Decisions and the *interfaces* (not internals) of the packages it
depends on, and sized so the build loop can carry it in one fresh build-turn
context. If a Decision is too large for one context it is split across
phases, each naming the slice of Verification ids it carries.

**Done bar.** A phase is **done** when every Verification id it realizes (or
its explicit slice) is covered by a clearly-named test and the suite is green
— see design's Conventions for what "green" concretely means (`go build
./...`, `go vet ./...`, `go test ./...` clean and `gofmt -l .` empty, from
`repos/`). Every phase's acceptance bar is deterministic exit conditions,
never a subjective judgment, never a self-referential or unsatisfiable check.

## Layout

`STATUS.md` is the manifest and the **only** home of status markers;
`phase-NN.md` is one body file per phase (zero-padded; sub-phases keep their
suffix, e.g. `phase-07a.md`); this README is the static rules. Append-only in
the layout too: never rewrite or delete a `phase-NN.md`, never delete a
`STATUS.md` line; the only build-time mutation is flipping one phase's
`⬜ → ✅` in `STATUS.md`.
