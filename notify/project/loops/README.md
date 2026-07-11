# notify — build loop (gather → build → verify)

This directory holds the **installed unattended build loop** for the notify
service: three prompts an autonomous harness (`ralph`) re-invokes with a fresh
context each turn to build the service one phase at a time, plus the executable
`run` wrapper and the ephemeral `brief.md` seam. This README describes the loop
**as installed here** — it lives beside the prompts so it can never drift from
them. The workspace map in `project/README.md` only points here.

## Running it

```
project/loops/run
```

which is exactly:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` runs from the **service root** (`notify/`) as its working directory, so
every path the prompts use is service-root-relative (`project/…`). It cycles the
three prompts `gather → build → verify → gather → …`, each in a fresh isolated
context, until `gather` finds no unfinished phase.

## The status contract

Each turn ends with a `{status, message}` the harness supplies out of band
(codex via `--output-schema`, claude via a synthetic `StructuredOutput` tool) and
reads back itself — the prompts never hard-code a transport. The terminal (last)
message of a turn drives the loop:

- **`NEXT`** — terminal: advance to the next prompt, wrapping `verify → gather`.
- **`DONE`** — terminal: stop the loop. **Only `gather` ever reports it**, when no
  `⬜` phase remains. `build` and `verify` always report `NEXT`.
- **`CONTINUE`** — the non-terminal status a streaming model tags its mid-turn
  progress messages with; `ralph` reads only the last message, so `CONTINUE`
  never advances or ends the loop.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips marker |
|---|---|---|---|---|
| **gather** | `STATUS.md`, one `phase-NN.md`, `design/INDEX.md` + realized `DNN.md`, (product for intent) | `brief.md` **contract region** (fresh phase only; no-ops on an in-flight brief) | no | no |
| **build** | `brief.md` only (contract + feedback) | service source + co-located id-tagged tests | yes (the code increment) | no |
| **verify** | `brief.md` (contract + own prior feedback), the suite, the source under test | on pass: nothing; on gap: `brief.md` **feedback region** | on pass: the one `⬜→✅` flip | yes (pass only) |

Only `gather` reads the big docs; only `verify` flips a marker or deletes the
brief; only `build` writes service code.

## The brief lifecycle

`project/loops/brief.md` is the single-phase seam that keeps `build` and `verify`
scoped to one phase without opening design or plan. It is **never committed**
(listed in the repo-root `.gitignore` as `*/project/loops/brief.md`) and
**phase-scoped, not per-cycle**:

- **gather** authors the contract region **once**, when a phase first becomes the
  active `⬜` phase, and **no-ops** (leaves it untouched) on every later cycle
  while that phase is still `⬜`.
- **build** consumes the whole brief — contract plus any `## Verify feedback` —
  and closes the listed open gaps first; it never writes the brief.
- **verify** on a **pass** flips the marker and **deletes** the brief; on a **gap**
  it **overwrites** the feedback region with only the currently-open gaps (each
  tied to an `R-id` and the exact failing command/output) and **keeps** the brief,
  so it persists across cycles until the phase passes or a stall reset fires.

## Why it converges (human-free)

`verify` can neither halt the loop nor advance a phase on a gap, so an incomplete
phase just stays `⬜` and is re-attacked next cycle — now with `verify`'s grounded
feedback in front of `build`, and without `gather` re-reading the big docs (it
no-ops on the in-flight brief). The persisted feedback gives `verify` cross-cycle
memory: it distinguishes *slow convergence* (the open-gap id set shrinking) from a
*true stall* (the **same** gap ids unsatisfied for **3** consecutive attempts with
**no new build commit**). On a true stall it does a **trajectory reset** —
discards the brief, logs the stall to `~/.ralph/verify.log`, leaves `⬜` — so the
next `gather` rebuilds the contract fresh. The only exit is `gather → DONE`, which
requires zero `⬜` markers, so the run ends only when every phase is verified green
(or a ralph budget rail trips).

## The `project/loops/brief.md` schema

Two single-writer regions that never clobber each other:

```
# Brief — Phase NN: <one-line objective>

## Contract                       ← gather writes once; verify never touches it
phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - project/design/D0n.md

### Design prose (verbatim, Verification lists excluded)
<full design prose of each realized Decision — Decision statement, shape/
signatures, and Rejected alternatives — copied verbatim from its DNN.md, with
that Decision's Verification list omitted>

### Ids to cover
R-XXXX-XXXX — <full requirement text, copied verbatim from the Verification list>
<...one line per phase-listed id; or "(none — structural phase; see the Done bar's named check)">

### Files to touch
- notify/<path>

### Dependency interfaces / required shapes (copied from design)
<public signatures / required config or doc snippets, verbatim>

### Done bar
<deterministic exit conditions: every id covered by a genuinely-asserting
`// R-XXXX-XXXX` test co-located with the code it exercises (landing/nginx/
composition tests in cmd/notify/*_test.go; domain tests beside their package),
and the suite green — go build ./..., go vet ./..., gofmt -l . (no output),
go test ./... all clean from the notify service root — plus any named check>

## Verify feedback              ← verify owns this; gather writes it empty
## Verify feedback — attempt N
build commit: <sha>   stall streak: <k>
- R-XXXX-XXXX — <exact failing command> → <observed output> (file:line)
<...only the currently-open gaps>
```

The id lines stay grep-able: `grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}'
project/loops/brief.md` yields exactly the active phase's id set.

## Toolchain baked into the prompts (from design's Conventions)

- **Build / typecheck:** `go build ./...` and `go vet ./...` (from the notify
  service root, which is ralph's cwd).
- **Test:** `go test ./...`.
- **"Suite is green":** `go build ./...`, `go vet ./...`, `gofmt -l .` (no
  output), and `go test ./...` all succeed with zero failures.
- **Next-phase lookup:** `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`
  (STATUS.md phase lines are bare `Phase NN ⬜/✅ …`, not Markdown bullets).
- **Id resolution:** a Decision → its `DNN.md` via `project/design/INDEX.md`; a
  single id via `grep -n R-XXXX-XXXX project/design/INDEX.md`.
