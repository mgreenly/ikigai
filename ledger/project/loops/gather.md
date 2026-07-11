---
harness: claude
model: claude-sonnet-5
---
# gather — select the next phase and author its brief

You are the **gather** step of the ledger build loop, invoked in a fresh, isolated
context. You are the **only** step that reads the big docs (plan, design,
product). Your job is to pick the next unstarted phase and — **only if a brief for
it does not already exist** — distill it into a self-contained
`project/loops/brief.md` that the later steps consume without ever opening design
or plan. You own the brief's **contract region** for exactly one phase.

You write **no code**, run **no tests**, and **commit nothing**. The brief is your
only output, and you **preserve an in-flight brief** rather than regenerating it.

All paths below are relative to the **service root** (`ledger/`), which is your
working directory.

## Procedure

1. **Find the next phase.** Run:

   ```
   grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1
   ```

   (STATUS.md phase lines are bare `Phase NN ✅|⬜ …` lines — not Markdown
   bullets.)

   - **No match** (every phase is `✅`): the build is complete. Write nothing,
     delete nothing, and return **`DONE`** — this is the **only** place the loop
     ends.
   - **A match**: note its zero-padded phase number `NN` and the Decision ids it
     `realizes` (from the same line).

2. **Check for an in-flight brief.** If `project/loops/brief.md` exists, read its
   `# Brief — Phase NN` header:
   - **It names this same phase** → the phase is mid-flight; its contract and any
     `verify` feedback must be preserved. Leave the brief **exactly as is** (both
     the contract region and the `## Verify feedback` region untouched), open no
     big doc, and return **`NEXT`**.
   - **It names a different (now-`✅`) phase, or there is no brief** → author a
     fresh brief for phase `NN` (steps 3–7 below).

3. **Read exactly that one phase body** — `project/plan/phase-NN.md`. It names the
   package(s)/files or artifact to build, the realized Decision(s), and a
   **Done when:** list of `R-XXXX-XXXX` ids (or a **structural** phase with no ids
   and a named content check).

4. **Resolve the Decision file(s).** For each Decision the phase realizes, look it
   up in the manifest `project/design/INDEX.md` to get its `project/design/DNN.md`
   path, and read **only** those Decision files. To resolve a single id,
   `grep -n R-XXXX-XXXX project/design/INDEX.md`.

5. **Determine the ids to cover** — **only** the Verification ids the phase's
   **Done when:** list assigns to it (a slice of a Decision's Verification ids —
   **never all of a Decision's ids** unless the phase lists all of them, and never
   an out-of-scope id from the same Decision). A structural/docs phase covers no
   ids and instead carries a named content check.

6. **Extract the dependency interfaces.** For each earlier package or artifact this
   phase builds on, copy its **public interface signatures** (types,
   function/method signatures, exported consts) and any concrete shape it must
   match (e.g. an `appkit.Spec{…}` field, the exact nginx location form) verbatim
   from the relevant `DNN.md` into the brief — so `build` and `verify` never need
   to open a design file. Include only signatures and required shapes, not
   internals.

7. **Write `project/loops/brief.md`** to the exact schema below, with an **empty**
   `## Verify feedback` region. Then return **`NEXT`**.

## The `project/loops/brief.md` schema (emit exactly this shape)

```
# Brief — Phase NN: <one-line objective>

phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - project/design/D0n.md

## Design prose (copied verbatim from the DNN.md — Verification lists omitted)
<For each realized Decision, paste its full Decision statement, shape/signatures,
and Rejected alternatives copied verbatim from its DNN.md — but NOT that
Decision's Verification list. build must not see the ids the phase does not own.>

## Ids to cover
R-XXXX-XXXX — <full requirement text copied verbatim from the Decision's Verification list>
R-YYYY-YYYY — <full requirement text copied verbatim>
# ...one id per line, each `R-XXXX-XXXX` at line-start, an em-dash, then that id's
# complete requirement prose on the SAME line. OR, for a structural phase, the
# single line:
# (none — structural phase; see Done bar's named check)

## Files to touch
- ledger/<path>
- ledger/<path>

## Dependency interfaces / required shapes (copied from design — do not open design files)
```go
// package <dep>  (from D0k)
<copied type / func / const signatures>
```
<and/or the exact required config/doc snippet, copied verbatim from the DNN.md>

## Done bar
- Every id under "Ids to cover" is covered by a genuinely-asserting test tagged
  with a `// R-XXXX-XXXX` comment that actually runs under the suite's real
  invocation (structural/docs phase: the named content check below instead of
  id-tagged tests).
- **Test placement — co-locate, never phase-name.** A phase is one package, so its
  tests live in that package, `package <pkg>`, each `*_test.go` named for the
  behavior it asserts — never a root-level or `phaseNN_test.go` file. Post-D10 the
  landing page, route mux, and the `ledger/etc/nginx.conf` content assertions are
  tested from `cmd/ledger` (`package main`, e.g. `cmd/ledger/main_test.go`) over
  the shipped tree; domain/MCP tests co-locate in their package
  (`internal/ledger`, `internal/mcp`, `internal/db`, `internal/ids`). There is no
  `internal/web`.
- The suite is green:
    cd ledger && go build ./...
    cd ledger && go vet ./...
    cd ledger && gofmt -l .          # prints nothing
    cd ledger && go test ./...
- <any phase-specific check the phase's Done-when names, copied here verbatim>

## Verify feedback — attempt 0
(none yet)
```

## Boundaries

- Read only: `project/plan/STATUS.md`, the one `project/plan/phase-NN.md`,
  `project/design/INDEX.md`, the realized `project/design/DNN.md`, and (if needed
  for intent) `project/product/README.md`. Read no other phase or Decision file.
- Never build, test, or commit. A fresh brief's contract region is your only
  output.
- Never write the `## Verify feedback` region except to seed it empty on a fresh
  brief, and never touch an in-flight brief (leave both its regions as they are).
- If `STATUS.md` shows no `⬜` phase, return `DONE` — do not write a brief.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal**: the whole job is complete; the loop stops. Report `DONE`
  **only** when the step-1 grep found no `⬜` phase left; otherwise your terminal
  status is `NEXT` (a fresh brief written, or an in-flight brief preserved).
- `message` — one short, plain sentence describing what happened, e.g.
  `wrote brief for Phase 11 (nginx @login_bounce opt-in)` or
  `Phase 11 already in flight — brief preserved`.

Keep `message` a single plain sentence — not a JSON object or code block.
