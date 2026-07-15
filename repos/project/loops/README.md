# repos — build loop (as installed)

This directory holds the **three-prompt build loop** an unattended harness
(`ralph`) re-invokes with a **fresh context** every turn to build the repos
service one phase at a time. This README describes the loop **as it is installed
on disk** — it lives beside the prompts it documents so it can never drift from
them. The workspace map (`project/README.md`) only points here; the spec shapes
live in `project/design/`, `project/plan/`, and `project/product/`.

## Running it

```
project/loops/run
```

`run` is the executable operator wrapper; its entire body is:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` runs from the **service root** (`repos/`, its working directory), so
every workspace path the prompts reference is service-root-relative
(`project/…`). It cycles the three prompts — `gather → build → verify → gather
→ …` — each in its own fresh, isolated context.

## Status contract

Each turn ends by reporting a `status` and a one-sentence `message`. The harness
supplies the `{status, message}` schema out of band (codex via
`--output-schema`, claude via `--json-schema`) and reads back the **final**
message of the turn — a prompt never emits literal JSON:

- **`NEXT`** — terminal: this turn is done; advance to the next prompt (wrapping
  `verify → gather`).
- **`DONE`** — terminal: the whole job is complete; the loop stops. **Only
  `gather` ever reports `DONE`**, and only when no `⬜` phase remains. `build`
  and `verify` always report `NEXT`.
- **`CONTINUE`** — the **non-terminal** status a streaming model tags the
  progress messages it emits *before* its terminal message. `ralph` reads only
  the last message, so `CONTINUE` never advances the loop.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips marker |
|---|---|---|---|---|
| **gather** | `STATUS.md`, the one `phase-NN.md`, its realized `DNN.md` (via `INDEX.md`), dependency source | `brief.md` **contract region** (fresh phase only) | no | no |
| **build**  | `brief.md` only | service code + co-located id-tagged tests | yes (the code increment) | no |
| **verify** | `brief.md` + runs the suite + traces coverage | `brief.md` **feedback region** (on gap), or deletes `brief.md` (on pass/stall) | yes (the one-line `⬜→✅` flip, on pass) | yes (on pass only) |

Next-phase lookup (gather):
`grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1` (phase lines are
Markdown bullets beginning `- Phase`). "The suite is green" (build & verify):
`go build ./...`, `go vet ./...`, and `go test ./...` all exit 0 with no
failures, and `gofmt -l .` prints nothing — all from `repos/`; tests run against
real temp-file SQLite through the embedded migrations, the real `git` binary
over bare fixture remotes, `httptest` peer stubs, and a deterministic injected
Clock — never a mocked git or store.

## Brief lifecycle

`project/loops/brief.md` is the ephemeral seam between the three prompts — the
complete and only input `build` and `verify` consume, so neither opens the big
docs. It is **never committed** (gitignored at the repo root via
`*/project/loops/brief.md`) and describes exactly **one** phase at a time:

- **gather** authors the brief's **contract region** once, when a phase first
  becomes the active `⬜` phase. While that phase stays `⬜`, gather **no-ops**
  on it — it leaves the in-flight brief (contract *and* feedback) untouched and
  opens no big doc.
- **build** consumes the whole brief (contract + feedback), does as much of the
  phase as cleanly fits one turn, commits, and never writes the brief.
- **verify** re-derives truth from scratch. On **pass** it flips `⬜→✅`,
  commits the flip, and **deletes** the brief. On a **gap** it **overwrites**
  the feedback region with only the currently-open gaps (each tied to an `R-id`
  and grounded in the exact failing command/output) and leaves the brief in
  place, so the next `build` sees the feedback. The brief thus **persists across
  cycles** until the phase passes or a stall reset discards it.

## Why it converges (human-free)

`verify` can neither halt the loop nor advance a phase on a gap, so an
incomplete phase just stays `⬜` and is re-attacked next cycle — now with
verify's grounded feedback in front of `build`, and without `gather` re-reading
the big docs (it no-ops on the in-flight brief). The persisted feedback also
gives `verify` cross-cycle memory: it distinguishes *slow convergence* (the
open-gap id set shrinking/changing) from a *true stall* (the **same** gap ids
unsatisfied for **3** consecutive no-progress attempts with **no new build
commit**). On a true stall it does a **trajectory reset** — logs to
`~/.ralph/verify.log`, discards the brief, leaves `⬜` — so the next `gather`
rebuilds the contract fresh from spec. The only exit is `gather → DONE`, which
requires zero `⬜` markers, so the run ends only when every phase is verified
green (or a ralph budget rail trips).

## `project/loops/brief.md` schema

**Contract region** (gather-owned; written once per phase):

```
# Build Brief — Phase NN: <one-line objective>

phase: NN
realizes: <D2 | D3, D4>
decision_files: <project/design/DNN.md[, …]>
status_line: <the exact STATUS.md phase line, verbatim>

## realized design (verbatim from the DNN.md — Verification list omitted)
<each realized Decision's full design prose — header, Decision statement with
 shape/signatures, and Rejected alternatives — copied verbatim, but stopping
 before that Decision's Verification list>

## ids to cover
<one id per line: `R-XXXX-XXXX — <full requirement text copied verbatim>`;
 only the ids this phase owns; or `(none — structural phase)`>

## files to touch
<one path per line; never a file outside repos/>

## dependency interfaces (copied — build must NOT open design/plan)
<go signatures build will call, labelled by source phase/package; or
 "(none — no earlier phase)">

## done bar
<per-id behavior + substrate; suite-green definition; the test-placement rule
 (co-located package-local tests; composition-root web/nginx tests in
 cmd/repos/); any "requires the suite up" note>
```

**Feedback region** (verify-owned; overwritten each gap cycle, empty on a fresh
brief):

```
## Verify feedback — attempt N
<attempt counter, the build commit verify observed, the stall-streak counter,
 and a checklist of ONLY the open gaps — each line an R-id + the exact failing
 command + observed output (+ file:line when known)>
```
