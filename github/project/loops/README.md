# github — build loop

The installed **gather → build → verify** autonomous build loop for the `github`
service. This README describes the loop **as installed here**, beside the prompts
it documents. The workspace map (`project/README.md`) only points here; the spec
shapes (product / design / plan) live under their own directories and are not
restated in this file.

## Running it

From the service root (`github/`):

```sh
project/loops/run
```

`run` is the executable operator wrapper. Its entire content is:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` runs from the service root (`github/`) and re-invokes the three prompts in
a **fresh, isolated context** each turn, cycling `gather → build → verify → gather
→ …`. Every workspace path the prompts use is service-root-relative (`project/…`).
The loop stops only when `gather` finds no `⬜` phase left in
`project/plan/STATUS.md` (or a ralph budget rail trips).

## The status contract

Each turn ends by reporting a `status` and a one-sentence `message`. `ralph`
reads **only the turn's final (terminal) message** and drives the loop from its
status. The harness supplies the `{status, message}` schema out of band (codex via
`--output-schema`, claude via `--json-schema`); the prompts describe only the
contract, never a transport.

- **`CONTINUE`** — **non-terminal**. The status a streaming model tags onto any
  progress message it emits *before* its final message. It never advances the
  loop; `ralph` ignores all but the terminal message.
- **`NEXT`** — **terminal**. This turn's work is done; advance to the next prompt
  (wrapping `verify → gather`). `build` and `verify` **always** end on `NEXT`.
- **`DONE`** — **terminal**. The whole job is complete; the loop stops. **Only
  `gather` ever reports it**, and only when its STATUS grep finds no `⬜` phase.
  Finishing a phase — even the last one, green suite and all gaps closed — is
  still `NEXT` for `build` and `verify`; ending the run is never theirs.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips a marker |
|---|---|---|---|---|
| **gather** | `STATUS.md`; the one `phase-NN.md`; `INDEX.md` + the realized `DNN.md`; dependency interface signatures | the brief's **contract region** (only when authoring a fresh brief) | no | no |
| **build** | **only** `project/loops/brief.md` (contract + feedback) | service source + package-local id-tagged tests | yes (the code increment) | no |
| **verify** | the brief (contract + its own prior feedback); the suite; the tests | on pass: deletes the brief; on gap: **overwrites** the brief's feedback region | on pass: the one-line `STATUS.md` flip | **yes** (`⬜→✅`, on pass only) |

`gather` is the only prompt that opens the big design/plan docs. `build` and
`verify` read **only** the brief. `verify` is the only prompt that flips a marker
or deletes the brief, and it writes no production code.

## The brief lifecycle

`project/loops/brief.md` is the ephemeral, single-phase seam between the three
prompts. It is **never committed** (ignored via the repo-root `.gitignore` entry
`*/project/loops/brief.md`) and describes exactly one phase at a time.

- **gather authors the contract once per phase.** When a phase first becomes the
  active `⬜` phase and no brief for it exists, `gather` writes the contract region
  and an empty feedback region. While that phase stays `⬜`, `gather` **no-ops** on
  the in-flight brief (it re-reads the `# Brief — Phase NN` header and leaves the
  file untouched) — it does **not** re-open a big doc each cycle.
- **build consumes it.** Each cycle `build` reads the whole brief, prioritizes any
  open `## Verify feedback` gaps, does as much of the phase as cleanly fits one
  fresh context, and commits — leaving the marker `⬜` and the brief untouched.
- **verify passes → delete, or gaps → feedback.** On a green suite with full
  coverage, `verify` flips the marker and deletes the brief. On a gap it leaves
  `⬜`, changes no source, and **overwrites** the feedback region with only the
  currently-open gaps (each tied to an `R-id` and the exact failing
  command/output). The brief therefore **persists across cycles** until the phase
  passes or a stall reset discards it.

## Why it converges (human-free)

`verify` can neither halt the loop nor advance a phase on a gap — an incomplete
phase simply stays `⬜` and is re-attacked next cycle, now with `verify`'s
grounded feedback in front of `build`, and without `gather` re-reading the big
docs (it no-ops on the in-flight brief). The persisted feedback gives `verify`
cross-cycle memory: it distinguishes *slow convergence* (the open-gap id set
shrinking/changing) from a *true stall* (the same gap ids unsatisfied for **3**
consecutive attempts with **no new build commit**). On a true stall `verify` does
a **trajectory reset** — logs it to `~/.ralph/verify.log`, deletes the brief, and
leaves `⬜` so the next `gather` rebuilds the contract fresh from spec. The only
exit is `gather → DONE`, which needs zero `⬜` markers, so the run ends only when
every phase is verified green (or a ralph budget rail trips).

## The `project/loops/brief.md` schema

A **gather-owned contract region** (written once when the phase becomes active;
`verify` never writes here) and a **verify-owned feedback region** (`gather`
writes it empty and never touches an in-flight brief; `verify` overwrites it with
open gaps). `build` reads both and writes neither:

```
# Brief — Phase NN — <one-line objective>

## Realizes
- D<n> — <title> — project/design/DNN.md
  (one line per realized Decision)

## Design (verbatim from each DNN.md; Verification lists omitted)
### D<n> — <title>
<the Decision statement, shape/signatures, and Rejected alternatives, copied
verbatim from the DNN.md — but NOT the Decision's Verification list>

## Ids to cover
R-XXXX-XXXX — <full requirement text, verbatim from the Decision's Verification list>
R-XXXX-XXXX — <...>
(or the single line: (none — structural phase))

## Files to touch
- <service-root-relative path> — <what changes>

## Dependency interfaces (copied in — build must not open a design file)
```go
<exported signatures of the packages this phase depends on>
```

## Done when
- <each deterministic predicate from the phase's Done-when, verbatim/tightened>

## Verify feedback
_(none yet — gather authored this brief; no gaps recorded.)_
```

`gather` writes the contract region and an empty feedback placeholder. `verify`,
on a gap, replaces the `## Verify feedback` region with a single
`## Verify feedback — attempt <N>` block carrying the observed build commit, the
stall streak, and a checklist of only the currently-open gaps — each an `R-id` (or
structural-smoke name) plus the exact failing command and its observed output. The
`Ids to cover` lines stay grep-able as the phase's coverage denominator:
`grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md`.

## This service's toolchain (baked into the prompts)

Taken from design's *Conventions* (`project/design/README.md`); the prompts inline
these so no step guesses:

- **Build / typecheck:** `GOWORK=off go build ./...` from `github/`.
- **Test:** `GOWORK=off go test ./...` from `github/`.
- **"Green"** = build succeeds, `go test` passes with **no failures and no
  `SKIP`**, `gofmt -l .` is empty, and `GOWORK=off go vet ./...` is clean — all
  from `github/`.
- **Test placement:** package-local `*_test.go`, co-located with the code they
  exercise and named for the behavior (e.g. `internal/gh/*_test.go`,
  `internal/web/nginx_test.go`). No separate integration-test home, no per-phase or
  root-level test file.
- **Offline tests only.** The one live-substrate id (`R-DMUT-QF4A`, the real
  GitHub App `health` proof) is verified **out of loop** per
  `project/github-verification.md` and never appears in a brief, so the loop never
  gates on it.
- **Next-phase lookup:** `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`
  (github's STATUS lines are Markdown bullets prefixed with `- `).
