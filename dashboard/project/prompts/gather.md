# gather — select the next phase and write its brief

You are the **gather** step of the dashboard build loop, invoked in a fresh,
isolated context. You are the **only** step that reads the big docs (plan, design,
product). Your single job is to pick the next unstarted phase and distill it into a
tiny, self-contained `project/prompts/brief.md` that the later steps consume
without ever opening design or plan.

You write **no code**, run **no tests**, and **commit nothing**. The brief is your
only output.

All paths below are relative to the repository root (your working directory). The
dashboard's design+plan workspace lives under `dashboard/project/`.

## Procedure

1. **Find the next phase.** Run:

   ```
   grep -nE '^Phase .* ⬜' dashboard/project/plan/STATUS.md | head -1
   ```

   - **No match** (every phase is `✅`): the build is complete. Write nothing,
     delete nothing, and return **`DONE`** (this is the only place the loop ends).
   - **A match**: note its zero-padded phase number `NN` and the Decision ids it
     `realizes` (from the same line).

2. **Read exactly that one phase body** — `dashboard/project/plan/phase-NN.md`. It
   names the package/files to build, the realized Decision(s), and a **Done when:**
   list of `R-XXXX-XXXX` ids.

3. **Resolve the Decision file(s).** For each Decision the phase realizes, look it
   up in the manifest `dashboard/project/design/INDEX.md` to get its
   `dashboard/project/design/DNN.md` path, and read **only** those Decision files.
   To resolve a single id, `grep -n R-XXXX-XXXX dashboard/project/design/INDEX.md`.

4. **Determine the ids to cover** — the Verification ids the phase's **Done when:**
   list assigns to it (normally all of the realized Decisions' ids). Phase 05 is a
   docs-only phase whose single id (`R-DB16-DOCS`) is verified by a text check on
   `dashboard/AGENTS.md`, not a Go test — say so in the brief.

5. **Extract the dependency surface.** This change lives in one package
   (`dashboard/internal/server`) plus the `dashboard/ui/` templates. Copy into the
   brief the exact handler/route/helper/view-model names the phase leans on
   (verbatim from the Decision files) — e.g. `(*app).sessionOwner`,
   `(*app).requireSession`, `clearSessionCookie`, `a.sessions.Lookup`,
   `a.pats.ListByOwner`, `a.oauthTokens.ListChainsByOwner`, `serviceRows`, the
   `pat_block`/`grants_block`/`pat_created` partials, the route patterns in
   `(*app).register` — so `build` and `verify` never need to open a design file.
   Include names/signatures, not internals.

6. **Write `project/prompts/brief.md`** (under `dashboard/`) to the exact schema
   below (overwrite any existing brief). Then return **`NEXT`**.

## The `dashboard/project/prompts/brief.md` schema (emit exactly this shape)

```
# Brief — Phase NN: <one-line objective>

phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - dashboard/project/design/D0n.md

## Ids to cover
R-DBxx-xxxx
R-DByy-yyyy
# ...one bare id per line

## Files to touch
- dashboard/internal/server/<file>.go
- dashboard/ui/html/<file>

## Dependency surface (copied from design — do not open design files)
<copied handler / route / helper / view-model names + signatures the phase uses>

## Done bar
- Every id under "Ids to cover" is covered by a genuinely-asserting test tagged
  with a `// R-DBxx-xxxx` comment (Phase 05's `R-DB16-DOCS` is verified by a text
  check on dashboard/AGENTS.md instead — name that explicitly).
- **Test placement — co-locate.** Tests live in
  `dashboard/internal/server/<name>_test.go`, `package server`, each named for the
  behavior it asserts — never a root-level or `phaseNN_test.go` file. They drive
  the real route table via the package's existing test harness
  (`(*app).routes()` / `httptest`), with a real temp-SQLite session store and an
  injected session cookie for "signed in".
- The suite is green:
    cd dashboard && go build ./...
    cd dashboard && go vet ./...
    cd dashboard && gofmt -l .          # prints nothing
    cd dashboard && go test ./...
    bin/check-migrations dashboard
```

## Boundaries

- Read only: `dashboard/project/plan/STATUS.md`, the one
  `dashboard/project/plan/phase-NN.md`, `dashboard/project/design/INDEX.md`, the
  realized `dashboard/project/design/DNN.md`, and (if needed for intent)
  `dashboard/project/product/product.md`. Read no other phase or Decision file.
- Never build, test, or commit. The brief is the only file you write.
- If `STATUS.md` shows no `⬜` phase, return `DONE` — do not write a brief.

End your final message with exactly one JSON object and nothing after it. Use
`DONE` only for the no-`⬜`-phase case; otherwise `NEXT`:

```json
{"status": "NEXT", "message": "wrote brief for Phase NN (<short objective>)"}
```
