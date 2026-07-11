# prompts — build loop (gather → build → verify)

The installed unattended build loop for the prompts service. `ralph` re-invokes
three prompts in a **fresh, isolated context** each turn, cycling
`gather → build → verify → gather → …`, to build the project one phase at a time
from the sealed `project/` spec. This README describes the loop **as installed**;
it lives beside the prompts it documents so it can never drift from them.

## Running it

From the service root (`prompts/`):

```
project/loops/run
```

`run` is the executable operator wrapper; its entire body is:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` runs from the service root, so every workspace path the prompts reference
is service-root-relative (`project/…`). The loop stops on its own when `gather`
finds no `⬜` phase left in `project/plan/STATUS.md` (or a ralph budget rail
trips).

## The status contract

Each turn ends with a `{status, message}` the harness supplies out of band
(`ralph` injects the schema per backend — codex via `--output-schema`, claude via
`--json-schema`); the prompt never hard-codes a transport. `ralph` reads only the
**terminal** (last) message of a turn and drives the loop off its status:

- `NEXT` — **terminal**: advance to the next prompt (wrapping `verify → gather`).
- `DONE` — **terminal**: stop the loop. **Only `gather` ever reports it**, and
  only when no `⬜` phase remains. `build` and `verify` always report `NEXT` —
  finishing a phase completely is still `NEXT`.
- `CONTINUE` — **non-terminal**: the status a streaming backend tags each
  mid-turn progress message with (codex coerces *every* streamed message into the
  schema). It never terminates a turn and never drives the loop.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips marker | terminal status |
|---|---|---|---|---|---|
| **gather** | `STATUS.md`, the one `phase-NN.md`, `INDEX.md`, the realized `DNN.md`, `product/README.md` | the brief's **contract region** (only when authoring fresh) | no | no | `NEXT`, or `DONE` if no `⬜` phase |
| **build** | `brief.md` only (contract + feedback) | source + co-located `*_test.go` | yes (code increment) | no | always `NEXT` |
| **verify** | `brief.md` + the suite | brief's **feedback region** on a gap; `STATUS.md` on a pass | yes (the one-line `⬜→✅` flip, on pass) | yes (pass only) | always `NEXT` |

## Brief lifecycle (`project/loops/brief.md`)

The brief is the single seam that keeps `build`'s and `verify`'s context scoped to
one phase — self-contained, so neither opens design or plan. It is **never
committed** (gitignored), **single-phase**, and **phase-scoped, not per-cycle**:

- **gather** authors the contract region **once**, when a phase first becomes the
  active `⬜` phase. On later cycles, while that phase is still `⬜`, gather sees a
  brief whose header names the current phase and **no-ops** — it does not reopen a
  big doc and does not touch either region.
- **build** consumes the whole brief (prioritizing any open feedback gaps) and
  never writes it.
- **verify** owns the feedback region. On a **pass** it flips the marker and
  **deletes** the brief; on a **gap** it **overwrites** the feedback region with
  only the currently-open gaps and leaves the brief in place, so the brief
  persists across cycles until the phase passes or a stall reset discards it.

## Why it converges without a human

`verify` can neither halt the loop nor advance a phase on a gap, so an incomplete
phase just stays `⬜` and is re-attacked next cycle — now with `verify`'s grounded,
command-tied feedback in front of `build`, and without `gather` re-reading the big
docs (it no-ops on the in-flight brief). The persisted feedback also gives
`verify` cross-cycle memory: it distinguishes *slow convergence* (the open-gap id
set shrinking or changing) from a *true stall* (the **same** gap ids unsatisfied
for **3** consecutive attempts with **no new build commit**). On a true stall it
does a **trajectory reset** — discards the brief so the next `gather` rebuilds the
contract fresh — which stays inside the "verify never halts / never advances on a
gap" invariant. The only exit is `gather → DONE`, which requires zero `⬜`
markers, so the run ends only when every phase is verified green.

## The `project/loops/brief.md` schema

Two **region-owned** halves, each with a single writer so they never clobber each
other.

**Contract region** (gather-owned; written once per phase):

```
# Brief — Phase NN: <one-line objective>

phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - project/design/D0n.md

## Design (full prose of each realized Decision — Verification lists omitted)
<Decision statement + shape/signatures + Rejected, verbatim from each realized
 DNN.md, with that Decision's Verification list removed>

## Ids to cover
R-XXXX-XXXX — <full requirement text copied verbatim from the Decision's Verification list>
R-YYYY-YYYY — <full requirement text copied verbatim>
# ...one id per line (id at line-start, em-dash, full requirement prose on the
# same line), OR the single line: (none — structural phase)

## Files to touch
- prompts/<path>

## Dependency interfaces (copied from design — do not open design files)
```go
// package <dep>  (from D0k)
<copied type / func / const signatures>
```

## Done bar
- Every id under "Ids to cover" is covered by a genuinely-asserting
  `// R-XXXX-XXXX` test, co-located with the code it exercises and named for the
  behavior (structural phase: green build + the named smoke).
- The suite is green (from `prompts/`): go build ./... ; go vet ./... ;
  gofmt -l . (prints nothing) ; go test ./...
- <any phase-specific check the phase's Done-when names>
```

The id-line format keeps the denominator grep-able:
`grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md` yields exactly this
phase's id set (the `-o` match ignores the trailing requirement text).

**Verify-feedback region** (verify-owned; gather writes it empty, build reads but
never writes, verify overwrites on each gap):

```
## Verify feedback — attempt N
build commit observed: <sha>
stall streak: <n>
- R-XXXX-XXXX — <exact failing command> → <observed output> (file:line)
# ...only the currently-open gaps; empty/"(none yet)" when the phase is fresh
```

`project/loops/brief.md` is listed in the repo-root `.gitignore` and is never
committed.
