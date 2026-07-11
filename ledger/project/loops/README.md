# ledger — build loop (gather → build → verify)

This directory holds the **installed** unattended build loop for the ledger
service. `ralph` re-invokes three prompts in a **fresh context** each turn,
cycling `gather → build → verify → gather → …` and building the project one phase
at a time until every phase in `project/plan/STATUS.md` is `✅`. This README
describes the loop **as it is on disk**; the workspace map (`project/README.md`)
only points here.

## Running it

```
project/loops/run
```

which is exactly:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` runs from the **service root** (`ledger/`), so every path the prompts
reference is service-root-relative (`project/…`, `cmd/ledger/…`). The wrapper is
executable.

## The status contract

Each turn ends with a `{status, message}` the harness reads out of band (`ralph`
injects the schema per backend — codex via `--output-schema`, claude via
`--json-schema`). The prompts describe only the contract, never a transport.

- **`NEXT`** — *terminal*: this turn's work is done; advance to the next prompt
  (wrapping `verify → gather`). `build` and `verify` **always** end on `NEXT`.
- **`DONE`** — *terminal*: the whole job is complete; the loop stops. **Only
  `gather` ever reports `DONE`**, and only when its `STATUS.md` grep finds no `⬜`
  phase left.
- **`CONTINUE`** — *non-terminal*: the status a streaming model tags the progress
  messages it emits **before** its terminal message. `ralph` reads only the last
  message, so `CONTINUE` never advances or ends the loop.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips marker |
|---|---|---|---|---|
| **gather** | `STATUS.md`, one `phase-NN.md`, `INDEX.md`, realized `DNN.md`, (product for intent) | `project/loops/brief.md` **contract region** (only when no brief exists for the active phase) | no | no |
| **build** | `project/loops/brief.md` only | package source + co-located id-tagged tests | yes (the code increment) | no |
| **verify** | `project/loops/brief.md` + runs the suite | `STATUS.md` (one flip) **or** the brief's `## Verify feedback` region | yes (the one-line flip, on pass) | **yes**, on pass only |

The next-phase lookup is `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`
(STATUS.md phase lines are bare `Phase NN …` lines). "Green" is
`cd ledger && go build ./...`, `go vet ./...`, `gofmt -l .` (no output), and
`go test ./...`, all with zero failures. Tests are **co-located** with the code
they exercise (post-D10, landing/route/nginx assertions live in `cmd/ledger`,
`package main`; domain/MCP tests in `internal/ledger`, `internal/mcp`,
`internal/db`, `internal/ids`), never a root-level or `phaseNN_test.go` file.

## The brief lifecycle

`project/loops/brief.md` is the ephemeral seam between the three prompts — **never
committed** (it is `.gitignore`d via the repo-root `*/project/loops/brief.md`) and
**single-phase** (it only ever describes one phase).

- **gather** authors the brief's contract region **once**, when a phase first
  becomes the active `⬜` phase, and **no-ops** while that same phase stays in
  flight (it leaves the brief — contract *and* feedback — untouched, opening no
  big doc).
- **build** consumes the whole brief (contract + feedback) and, if the feedback
  lists open gaps, closes those first. It reads the brief but never writes it.
- **verify** re-derives truth independently. **Pass** → flip `⬜→✅`, commit, and
  delete the brief. **Gap** → leave `⬜`, change no source, and **overwrite** the
  feedback region with only the currently-open gaps (each tied to its `R-id` and
  the exact failing command/output). The brief **persists across cycles** until
  the phase passes or a stall reset discards it.

## Why it converges (human-free)

`verify` can neither halt nor advance a phase on a gap, so an incomplete phase just
stays `⬜` and the loop re-attacks it — now with `verify`'s grounded feedback in
front of `build`, and without `gather` re-reading the big docs (it no-ops on the
in-flight brief). The persisted feedback also gives `verify` cross-cycle memory: it
distinguishes *slow convergence* (the open-gap id set shrinking/changing) from a
*true stall* (the **same** gap ids unsatisfied for ≥3 consecutive attempts with **no
new build commit**). On a true stall it does a **trajectory reset** — discards the
brief (logs to `~/.ralph/verify.log`, leaves `⬜`) so the next `gather` rebuilds the
contract fresh from spec. The only exit is `gather → DONE`, which requires zero `⬜`
markers — so the run ends only when every phase is verified green (or a ralph budget
rail trips).

## The `project/loops/brief.md` schema

A single-phase file with two single-writer regions:

- **Contract region** (gather-owned, written once per phase):
  - `# Brief — Phase NN: <objective>` header, `phase:`, `realizes:`,
    `decision_files:`.
  - `## Design prose` — the full Decision statement, shape/signatures, and Rejected
    alternatives of each realized Decision, copied verbatim from its `DNN.md` **with
    that Decision's Verification list omitted**.
  - `## Ids to cover` — one id per line, `R-XXXX-XXXX — <full requirement text
    copied verbatim>`, listing **only** the phase's own ids (or the single line
    `(none — structural phase; …)`). Grep-able as
    `grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md`.
  - `## Files to touch`, `## Dependency interfaces / required shapes` (copied
    signatures/config snippets), and `## Done bar`.
- **Verify-feedback region** (verify-owned): a `## Verify feedback — attempt N`
  heading carrying the attempt counter, the build commit `verify` observed, the
  stall-streak counter, and a checklist of **only** the open gaps — each line an
  `R-id` + the exact failing command/output. `gather` seeds it empty; `verify`
  overwrites it each gap cycle; `build` reads but never writes it.
