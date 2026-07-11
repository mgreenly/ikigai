# gmail — build loop (gather → build → verify)

This directory holds the **installed** unattended build loop for the gmail
service: the three prompts `ralph` cycles to build the project one phase at a
time, the executable `run` wrapper, and this overview. It is kept beside the
prompts it describes so it can never drift from the loop on disk. It is **not** a
spec artifact — the spec shapes live under `project/product`, `project/design`,
and `project/plan`; `project/README.md` (the workspace map) only points here.

## Running it

```
project/loops/run
```

which is exactly:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` runs from the **service root** (`gmail/`, its working directory), so every
workspace path the prompts reference is service-root-relative (`project/…`). It
re-invokes each prompt in a **fresh, isolated context** and cycles
`gather → build → verify → gather → …` until the loop ends.

## Status contract

Each turn ends by reporting a `status` (and a one-sentence `message`). The
harness supplies the `{status, message}` schema out of band and reads back the
**final** message of the turn:

- `NEXT` — **terminal**: advance to the next prompt (wrapping `verify → gather`).
- `DONE` — **terminal**: stop the loop. **Only `gather` ever reports it**, when it
  finds no `⬜` phase left. `build` and `verify` always report `NEXT` — finishing
  a phase completely is still `NEXT`.
- `CONTINUE` — **non-terminal**: the status a streaming model tags the progress
  messages it emits *before* its terminal message. `ralph` reads only the last
  message, so `CONTINUE` never advances or ends the loop.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips marker |
|---|---|---|---|---|
| **gather** | `STATUS.md`, one `phase-NN.md`, `INDEX.md`, the realized `DNN.md`, `product/README.md` (intent) | the brief's **contract region** (fresh phase only) | no | no |
| **build**  | **only** `project/loops/brief.md` (both regions) | source + co-located id-tagged tests | yes (the increment) | no |
| **verify** | `project/loops/brief.md` + runs the suite | the brief's **feedback region** (on a gap) | yes (the one-line `⬜→✅` flip, on pass) | yes (on pass) |

The toolchain the prompts bake in (from design's *Conventions*): "green" means
`cd gmail && go build ./...`, `cd gmail && go vet ./...`,
`cd gmail && gofmt -l .` (no output), and `cd gmail && go test ./...` all succeed
with zero failures. The next-phase lookup is
`grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`. Tests are co-located
with the code they exercise as `*_test.go` files named for the behavior (e.g.
`cmd/gmail/nginx_test.go`, `cmd/gmail/landing_test.go`), never in a per-phase or
root-level file.

## Brief lifecycle

`project/loops/brief.md` is the ephemeral, single-phase seam between the prompts.
It is **never committed** (listed in the repo-root `.gitignore` via
`*/project/loops/brief.md`) and is **phase-scoped, not per-cycle**:

- `gather` **authors the contract region once** when a phase first becomes the
  active `⬜` phase, and **no-ops** (leaves the brief untouched, opens no big doc)
  while that same phase is still in flight.
- `build` consumes the whole brief — contract **and** verify-feedback regions —
  and closes any open gaps first; it never writes the brief.
- `verify` re-derives truth independently and either **passes** the phase (flip
  `⬜→✅`, commit, delete the brief) or records a **gap** (overwrite the feedback
  region with only the currently-open gaps, keep the brief). The brief therefore
  **persists across cycles** until the phase passes or a stall reset discards it.

## Why it converges (human-free)

`verify` can neither halt the loop nor advance a phase on a gap, so an incomplete
phase just stays `⬜` and is re-attacked next cycle — now with `verify`'s
grounded, command-tied feedback in front of `build`, and without `gather`
re-reading the big docs (it no-ops on the in-flight brief). The persisted feedback
gives `verify` cross-cycle memory: it distinguishes *slow convergence* (the
open-gap id set shrinking/changing) from a *true stall* (the **same** gap ids
unsatisfied for **3** consecutive attempts with **no new build commit**). On a
true stall it does a **trajectory reset** — discards the brief so the next
`gather` rebuilds the contract fresh. The only exit is `gather → DONE`, which
requires zero `⬜` markers, so the run ends only when every phase is verified green
(or a ralph budget rail trips).

## The `project/loops/brief.md` schema

A single-phase file with a **gather-owned contract region** and a
**verify-owned feedback region** (each written by exactly one step, so they never
clobber each other):

```
# Brief — Phase NN: <one-line objective>

phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - project/design/D<nn>.md

## Design prose (copied verbatim from the DNN.md — Verification lists omitted)
<the realized Decision's full Decision. statement + shape/signatures + Rejected.
alternatives, verbatim, minus that Decision's Verification list>

## Ids to cover
R-XXXX-XXXX — <full requirement text copied verbatim from the Decision's Verification list>
# ...one id per line in that exact form, OR: (none — structural phase; see Done bar's named check)

## Files to touch
- gmail/<path>

## Dependency interfaces / required shapes (copied from design — do not open design files)
<the dependency packages' public signatures / required config shapes, copied verbatim>

## Done bar
- every id under "Ids to cover" covered by a genuinely-asserting, reachable
  `// R-XXXX-XXXX`-tagged test co-located with the code it exercises;
- the suite green (build, vet, gofmt -l, go test — all clean);
- any phase-specific named check.

## Verify feedback — attempt N
<empty when gather authors it; verify overwrites it with the current open gaps —
each an R-id + the exact failing command + observed output, plus the observed
build commit and stall streak>
```

The denominator for a phase is `grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}'
project/loops/brief.md` — the matched id substring per line, ignoring the trailing
requirement text — so it yields exactly the phase's id set.
