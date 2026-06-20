# LOOP — the wiki build loop (author/operator overview)

This file documents the unattended build loop. It is **not** a prompt and is
never read by the loop itself — `gather`, `build`, and `verify` are each read in
a fresh, isolated context and are fully self-contained. This overview is for the
human author/operator.

## Invocation

```
ralph wiki/docs/gather.md wiki/docs/build.md wiki/docs/verify.md
```

`ralph` re-invokes these three prompts in order, each in a **fresh context**,
cycling `gather → build → verify → gather → …`. All state lives in the workspace
(the plan's `STATUS.md` markers, the git history, and the ephemeral brief); no
context is carried between turns.

## The two-status contract

Every prompt ends its final message with exactly one JSON object:

```json
{"status": "NEXT", "message": "<one short sentence>"}
```

- **`NEXT`** — advance to the next prompt, wrapping `verify → gather`. Returned by
  `build` and `verify` **always**, and by `gather` whenever it selected a phase.
- **`DONE`** — stop the loop. Returned **only** by `gather`, and only when
  `STATUS.md` has no `⬜` phase left.
- `CONTINUE` is unused.

So the only programmatic exit is `gather → DONE`. The other stop is an external
`ralph` budget/iteration rail.

## What each step does (and what it may touch)

| step | reads | writes | commits | returns |
|---|---|---|---|---|
| **gather** | plan `STATUS.md`, one `phase-NN.md`, `design/INDEX.md`, the realized `design/DNN.md`, (product for intent) | `wiki/docs/brief.md` only | nothing | `NEXT`, or `DONE` if no `⬜` |
| **build** | `wiki/docs/brief.md` only | package code + id-tagged tests | the code increment | always `NEXT` |
| **verify** | `wiki/docs/brief.md` + the suite | the one `⬜→✅` flip (pass only); deletes the brief | the one-line flip (pass only) | always `NEXT` |

- **gather** is the only step that opens the big docs; it distills exactly one
  phase into the brief.
- **build** never opens design/plan/product — the brief is its whole world. It
  does one bounded, idempotent increment and may land a subset; the loop re-enters
  until green. It never flips a marker and never touches the brief.
- **verify** is the independent gate — the only step that flips a marker or
  deletes the brief. It writes no production code.

## State machine — why it is human-free and converges

- **The `⬜`/`✅` marker in `wiki/docs/plan/STATUS.md` is the sole completion
  signal.** It is the only durable per-phase status, append-only: a phase line is
  never deleted, only flipped once, by verify.
- **The brief lifecycle is create → consume → delete.** `gather` creates
  `wiki/docs/brief.md`; `build` and `verify` consume it; `verify` deletes it as
  its final step (on pass **and** on gap). It exists only between a gather and the
  next verify, is single-phase (overwritten fresh each cycle), and is **never
  committed** (gitignored at `/wiki/docs/brief.md`).
- **Convergence:** `verify` can neither halt the loop nor advance a phase on a
  gap. An incomplete or non-green phase just stays `⬜`; the next `gather`
  re-selects it, `build` does another increment, `verify` re-checks. Each cycle
  either makes a phase greener or certifies it. The loop therefore drives every
  phase to verified-green, and ends only when `gather` finds zero `⬜` markers
  (→ `DONE`) — or when an external `ralph` budget/iteration rail trips.

## The `wiki/docs/brief.md` schema

`gather` emits exactly this; `build` and `verify` rely on it being the complete,
self-contained spec for one phase (so neither opens design or plan):

```
# Brief — Phase NN: <one-line objective>

phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - wiki/docs/design/D0n.md

## Ids to cover
R-XXXX-XXXX
R-YYYY-YYYY
# ...one bare R-id per line so `grep -oE 'R-[A-Z0-9]{4}-[A-Z0-9]{4}' wiki/docs/brief.md`
# enumerates them, OR the single line:  (none — structural phase)

## Files to touch
- wiki/<path>

## Dependency interfaces (copied from design — do not open design files)
```go
// package <dep>  (from D0k)
<copied type / func / const signatures>
```

## Done bar
- Every id under "Ids to cover" is covered by a genuinely-asserting test tagged
  with a `// R-XXXX-XXXX` comment (structural phase: green build + named smoke).
- The suite is green (commands below).
- <any phase-specific check the phase's Done-when names, copied verbatim>
```

## Coverage convention and the "done" bar

Design mints the `R-XXXX-XXXX` ids but deliberately does not define how coverage
is measured — the loop defines it:

- An id counts as **covered** only when it appears in a `// R-XXXX-XXXX` comment
  on a test (in a `*_test.go` file) that **genuinely asserts** the behavior that
  Decision's Verification list describes — never a bare literal, never an
  always-passing test. When verify is unsure a test really asserts, the id is
  treated as **uncovered**.
- A phase is **done** when every id it owns is so covered **and** the suite is
  green. A **structural phase** carries no ids and is proven by the green build
  plus any integration smoke it names.

## Project conventions the prompts inline

Taken from `wiki/docs/design.md` *Conventions* (the single source of truth for the
toolchain); reproduced here for the operator:

- **Toolchain:** Go 1.26, single `module wiki` rooted at `wiki/`; pure-Go SQLite
  driver `modernc.org/sqlite`. `appkit`/`eventplane` are in-repo replace-siblings;
  `github.com/ikigenba/agentkit` is published/proxy-fetched with no `replace`.
- **"The suite is green"** = all of these pass with zero failures:
  `cd wiki && go build ./...`, `cd wiki && go vet ./...`,
  `cd wiki && gofmt -l .` (no output), `cd wiki && go test ./...`,
  `bin/check-migrations wiki`.
- **Production-shaped build** (Phase 01's extra check): `GOWORK=off`, no `go.work`
  in scope, the local `~/projects/agentkit` path absent — agentkit resolves from
  the module proxy at its pinned tag.
- **Migrations:** ordered SQL under `wiki/internal/db/migrations/`, embedded as
  `db.FS`; created only via `bin/new-migration wiki <name>`; committed migrations
  are immutable (`bin/check-migrations wiki` enforces this).
- **Determinism / test seams:** clock and IO injected at `cmd/wiki/main.go`; the
  LLM is **always mocked** through the `internal/llm` seam (no live calls, green
  offline); the test DB is a **real temp SQLite** migrated by the appkit runner;
  pure functions are table-tested directly.

## Files

- `wiki/docs/gather.md`, `wiki/docs/build.md`, `wiki/docs/verify.md` — the three
  loop prompts.
- `wiki/docs/brief.md` — ephemeral, gitignored (`/wiki/docs/brief.md`); created by
  gather, deleted by verify; never committed.
- `wiki/docs/plan/STATUS.md` — the manifest; the only home of `⬜`/`✅` markers.
