# dashboard — Plan (web pages restructure)

**Authority: construction order and history.** This document and the
`project/plan/` directory it heads own the **build order** of the dashboard's
three-page web restructure and the **record of what has been built**. The plan is
**append-only**: completed phases are never rewritten or deleted, so the plan
doubles as the construction history. To extend the work, update the product
(`project/product/product.md`) and design (`project/design/design.md` +
`project/design/`) **in place** to stay authoritative for the current state, then
**append** a new phase here — a new `project/plan/phase-NN.md` body file plus a new
line in `project/plan/STATUS.md`. Never edit a finished phase except to flip its
status marker in `STATUS.md`.

**One phase = one coherent increment = one accumulating context.** Each phase is a
single coherent unit sized for one subagent, built in one accumulating context
against the product and design. A phase reads only the design Decision(s) it
realizes (resolved through `STATUS.md` → `phase-NN.md` → the brief). For this
change every phase touches the `internal/server` package and the `ui/` templates —
the composition is a few files (`routes.go`, a new `profile.go`, the templates,
the view builders), assembled incrementally as the three pages come online; that
is growth of a shared wiring surface, not a rewrite of a finished phase. The
phases are **sequential**: the profile page must exist before token/grant
management can move onto it, and the landing must be cleared of that management
before the AGENTS.md truth is rewritten.

**Done bar.** A phase is **done** when every Verification item — the
`R-XXXX-XXXX` ids — in the design Decision(s) it realizes is covered by a
clearly-named, genuinely-asserting test and the suite is green. "Green" is defined
concretely in design's *Conventions*: `cd dashboard && go build ./...`,
`go vet ./...`, `gofmt -l .` (no output), `go test ./...`, and
`bin/check-migrations dashboard` all succeed with zero failures. "Covered" means
each listed id has a genuine test exercising the behavior that Decision's
Verification list describes — see each `project/design/DNN.md` Verification section
for what the id requires. The doc-truth phase (D6) is verified by a text check on
`AGENTS.md`, not a Go test.

## Layout

The plan is physically split so the build loop reads only what it needs:

- `project/plan/STATUS.md` — the manifest: one line per phase in build order, and
  the **only** home of status markers (`✅` done / `⬜` not started).
- `project/plan/phase-NN.md` — one body file per phase (zero-padded:
  `phase-01.md`, …). A phase body carries **no** status token — status lives only
  in `STATUS.md`.
- `project/plan/plan.md` — this file: the static, invariant rules above. It lists
  no phases and carries no status, so it never grows with the project.

**Append-only, restated for this layout:** never rewrite or delete a
`phase-NN.md`; never delete a line in `STATUS.md`. The only build-time mutation to
either is flipping a single phase's `⬜ → ✅` in `STATUS.md` when it lands.
