# cron — Plan (landing page)

**Authority: construction order and history.** This document and the
`project/plan/` directory it heads own the **build order** of the cron landing
page and the **record of what has been built**. The plan is **append-only**:
completed phases are never rewritten or deleted, so the plan doubles as the
construction history. To extend the work, update the product
(`project/product/product.md`) and design (`project/design/design.md` +
`project/design/`) **in place** to stay authoritative for the current state, then
**append** a new phase here — a new `project/plan/phase-NN.md` body file plus a
new line in `project/plan/STATUS.md`. Never edit a finished phase except to flip
its status marker in `STATUS.md`.

**One phase = one coherent unit = one accumulating context.** Each phase is a
single coherent unit — almost always one Go package (`internal/<pkg>` or
`cmd/cron`), or one shipped artifact (the nginx fragment, the doctrine doc) —
built in one accumulating context against the product and design. A phase reads
only the design Decisions it realizes and the *interfaces* (not the internals) of
the packages it depends on. This keeps every phase the size of a small standalone
task. The composition root (`cmd/cron/main.go`) is the one shared file
legitimately touched by more than one phase — it is assembled incrementally as
surfaces come online; that is not a rewrite of a finished phase, only growth of a
wiring file.

**Done bar.** A phase is **done** when every Verification item — the
`R-XXXX-XXXX` ids — in the design Decisions it realizes (or the slice of those
ids assigned to it) is covered by a clearly-named, genuinely-asserting test and
the suite is green. "Green" is defined concretely in design's *Conventions*:
`cd cron && go build ./...`, `cd cron && go vet ./...`, `cd cron && gofmt -l .`
(no output), `cd cron && go test ./...`, and `bin/check-migrations cron` all
succeed with zero failures. "Covered" means each listed id has a genuine test
exercising the behavior that Decision's Verification list describes — see each
`project/design/DNN.md` Verification section for what the id requires. A
**structural** phase (no ids, e.g. the docs truth-statement) is done when its
named content check passes and the suite stays green.

## Layout

The plan is physically split so the build loop reads only what it needs:

- `project/plan/STATUS.md` — the manifest: one line per phase in build order, and
  the **only** home of status markers (`✅` done / `⬜` not started).
- `project/plan/phase-NN.md` — one body file per phase (zero-padded: `phase-01.md`,
  `phase-02.md`, …). A phase body carries **no** status token — status lives only
  in `STATUS.md`.
- `project/plan/plan.md` — this file: the static, invariant rules above. It lists
  no phases and carries no status, so it never grows with the project.

**Append-only, restated for this layout:** never rewrite or delete a
`phase-NN.md`; never delete a line in `STATUS.md`. The only build-time mutation to
either is flipping a single phase's `⬜ → ✅` in `STATUS.md` when it lands.
