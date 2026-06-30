# Suite on-box layout, versioning & backup/restore — Plan

**Authority: construction order and history.** This document and the
`project/plan/` directory it heads own the **order the work is built in** and the
**record of what has been built** — nothing else (the *why* is product's, the
*shape and its proof* is design's). The plan is **append-only**: completed phases
are never rewritten or deleted, so the plan doubles as the construction history.
To extend it: update `project/product/product.md` and `project/design/` **in
place** first, then **append** a new phase — a new `project/plan/phase-NN.md` body
plus a new line in `project/plan/STATUS.md`. Never edit a finished phase except to
flip its status marker in `STATUS.md`.

**One phase = one package = one accumulating context.** Each phase is a single
coherent unit of work — almost always one Go package (or one shell tool) — built
in one accumulating context against product and design, reading only that unit's
design Decisions and the *interfaces* (not internals) of the packages it depends
on. That is what keeps every phase the size of a small standalone tool no matter
how large the suite grows. Where a single Decision is too large for one context it
is split across phases, and each affected phase names the **slice** of that
Decision's Verification ids it carries.

**Done bar.** A phase is **done** when every Verification id (`R-XXXX-XXXX`) of the
design Decision(s) it realizes — or the explicit slice of those ids assigned to it
— is covered by a clearly-named test, and the suite is green. "Green" is defined
by design's *Conventions*: **`bin/test` exits 0** (`bin/check-migrations`, then the
repo-root `bin/*.test.sh`, then `go test ./...` across every workspace module).
"Covered" is defined by each Decision's **Verification** list — the concrete
behavior the id names, proven on the substrate that id specifies (a real
filesystem/S3/eventplane substrate is exercised for real, never only a fake).
Every phase's acceptance bar is expressed as **deterministic exit conditions**:
mechanically-checkable predicates (a green test/suite, an exit code, an exact
match count) that are reproducible on identical repo state and whose passing state
is actually reachable — never a subjective prose judgment, and never a
self-referential or unsatisfiable check.

## Layout

The plan is physically split so the build loop reads only the one unit of work it
needs, never the whole history:

- **`project/plan/STATUS.md`** — the manifest: one line per phase in build order,
  and the **only** home of status markers (`✅` done / `⬜` not started). The loop
  finds its next work with
  `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1` and reads only that
  phase's body file.
- **`project/plan/phase-NN.md`** — one body file per phase (zero-padded; a
  sub-phase keeps its suffix, e.g. `phase-08a.md`). Carries **no** status marker
  of its own.
- **`project/plan/plan.md`** — this file: the static rules above. It lists no
  phases and carries no status, so it never grows with the project.

**Append-only, for this layout:** never rewrite or delete a `phase-NN.md`, never
delete a `STATUS.md` line. The only build-time mutation is flipping one phase's
`⬜ → ✅` in `STATUS.md`.
