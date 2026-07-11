# wiki — the build loop (as installed)

This is the author-facing overview of the **`gather → build → verify`** build
loop installed under `project/loops/`, kept beside the prompts it describes so it
can never drift from them. `project/README.md` (the workspace map) points here;
the spec shapes it does not restate live in `project/design/` and `project/plan/`.

## Running it

The loop runs from the **service root** (`wiki/`), which is `ralph`'s working
directory, so every path the prompts touch is service-root-relative (`project/…`).
Start it with the wrapper:

```
project/loops/run
```

which is exactly:

```sh
#!/bin/bash

exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` cycles the three prompts in **fresh, isolated contexts** —
`gather → build → verify → gather → …` — carrying no memory between turns; all
state lives in files under the service root.

## The status contract

Each turn ends by reporting a `status` and a one-sentence `message`. `ralph` reads
only the **last** message of a turn and acts on its status:

- **`NEXT`** — *terminal*: advance to the next prompt (wrapping `verify → gather`).
  **build and verify always report `NEXT`.**
- **`DONE`** — *terminal*: stop the loop. **Only `gather` ever reports it**, and
  only when no `⬜` phase remains in `STATUS.md`.
- **`CONTINUE`** — *non-terminal*: the status a streaming model (e.g. gpt-5.5 under
  codex, which coerces every streamed message into the schema) tags the progress
  messages it emits **before** its terminal message. It never advances the loop;
  `ralph` ignores all but the terminal message.

The harness supplies the `{status, message}` schema out of band per backend (codex
via `--output-schema`; claude via `--json-schema` surfaced as a `StructuredOutput`
tool) — the prompts describe only the contract, never a transport.

## Per-step reads / writes / commits / flips

| step | reads | writes | commits | flips a marker |
|---|---|---|---|---|
| **gather** | `STATUS.md`; for a fresh brief, the one `phase-NN.md` + its Decision `DNN.md`(s) + `INDEX.md` + dependency interface signatures | `brief.md` **contract** region (fresh brief only); nothing on an in-flight brief | no | no |
| **build** | `brief.md` only (contract + feedback) | source packages + id-tagged `*_test.go` (or the named config file, for a structural phase) | yes (the code increment) | no |
| **verify** | `brief.md` (contract + own prior feedback) + the suite | on pass: the one `STATUS.md` line; on gap: `brief.md` **feedback** region | yes (the one-line flip, on pass) | **yes** (only verify) |

The toolchain the loop bakes in (from design's *Conventions*): build/typecheck
`go build ./...` + `go vet ./...`; test `go test ./...`; **green** = those plus
`gofmt -l .` printing nothing, all with zero failures. Tests are `*_test.go`
co-located in the package they exercise; cross-package integration tests live in
`internal/wiki/`. A **structural/config phase** (e.g. an `wiki/etc/nginx.conf`
edit) carries no `R-ids` — it is proven by a green suite plus the named fragment
check the phase states (a `project/`-excluded grep over that file).

## The brief lifecycle

`project/loops/brief.md` is the **single-phase**, **never-committed** (it is in
`.gitignore`) seam between the prompts — the complete and only input `build` and
`verify` consume, so neither opens a design/plan/product doc.

- **gather authors the contract once** when a phase first becomes the active `⬜`
  phase, then **no-ops while it's in flight** — on later cycles it sees the brief
  already names the current phase and leaves it (contract *and* feedback) untouched,
  opening no big doc.
- **build consumes it** every cycle, closing verify's open gaps first, and commits.
- **verify passes → deletes it**; **verify finds gaps → overwrites the feedback
  region** with only the currently-open gaps and leaves the brief in place, so it
  **persists across cycles** until the phase passes or a stall reset discards it.

## Why it converges (and is human-free)

`verify` can neither halt the loop nor advance a phase on a gap, so an incomplete
phase simply stays `⬜` and is re-attacked next cycle — now with verify's grounded,
command-tied feedback in front of `build`, and without `gather` re-reading the big
docs (it no-ops on the in-flight brief). The persisted feedback also gives `verify`
cross-cycle memory: it distinguishes *slow convergence* (the open-gap id set
shrinking/changing) from a *true stall* (the **same** gap ids unsatisfied for 3
consecutive attempts with **no new build commit**). On a true stall it does a
**trajectory reset** — discards the brief and logs the stall to `~/.ralph/verify.log`,
leaving `⬜` — so the next `gather` rebuilds the contract fresh from spec. The only
exit is `gather → DONE`, which requires **zero `⬜` markers** — so the run ends only
when every phase is verified green (or a `ralph` budget rail trips).

## The `brief.md` schema

Two regions, one writer each — they never clobber each other:

```
# Brief — Phase NN

## Contract  (gather-owned — verify never writes here)

- Phase: NN — <one-line objective>
- Realizes: D<n>[, D<m>]
- Decision files: project/design/D0n.md[, …]

### Design prose — Decision <n> (<title>)
<the Decision's full statement, shape/signatures, and Rejected alternatives,
 copied verbatim from DNN.md — with that Decision's Verification list OMITTED>

### Ids to cover
R-XXXX-XXXX — <full requirement text, verbatim from the Decision's Verification list>
<one id per line; the id at line-start, an em-dash, then the requirement prose on
 the SAME line — only the ids this phase's Done-when lists; or `(none — structural phase)`>

### Files to touch
- <path> …

### Dependency interface signatures
<the exported signatures of depended-on packages, copied in>

### Done bar
<deterministic acceptance: green suite + every id covered by a genuinely-asserting
 co-located `// R-id` test, or the named structural check for a config phase>

## Verify feedback — attempt N  (verify-owned — gather writes this empty)
- build commit observed: <sha | none>
- stall streak: <n>
- open gaps:
  <one line per open gap: R-id + exact failing command + observed output; or (none)>
```

- The **contract region** is gather-owned, written once when the phase becomes
  active; verify never writes here.
- The **feedback region** is verify-owned; gather writes it empty on a fresh brief
  and never touches it again; build reads it but never writes it; verify
  **overwrites** it (never appends) each gap cycle.
