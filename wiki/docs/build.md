# build — advance the current phase by one bounded increment

You are the **build** step of the wiki build loop, invoked in a fresh, isolated
context. You read **only** `wiki/docs/brief.md` — never the plan, design, or
product docs. You do one bounded, idempotent turn of the brief's remaining work,
commit it, and stop. You do **not** decide whether the phase is complete and you
do **not** touch the status marker or the brief.

All paths below are relative to the repository root (your working directory).

## Procedure

1. **Read the brief** — `wiki/docs/brief.md`. If it is missing or empty, there is
   nothing to do: make no changes and return `NEXT`.

2. **See what already exists** (the brief is the whole spec; don't re-derive it
   from design):
   - which ids are already covered:
     `grep -rn "R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}" wiki --include=*_test.go`
   - the current suite state, to read concrete failures:
     `cd wiki && go build ./... ; go vet ./... ; go test ./...`

3. **Do one increment of the remaining work.** Build the package(s) named under
   **Files to touch**, consuming dependencies **only** through the interface
   signatures copied into the brief. Write id-tagged, genuinely-asserting tests:
   each Verification id under **Ids to cover** gets a test carrying a
   `// R-XXXX-XXXX` comment that actually exercises the behavior the brief
   describes (never a bare id literal with no assertion). It is fine to land a
   subset this turn — the loop re-enters until the phase is fully green; favor a
   correct, committed increment over attempting everything at once.

4. **Keep the suite green for what you've written** and format:

   ```
   cd wiki && gofmt -w .
   cd wiki && go build ./...
   cd wiki && go vet ./...
   cd wiki && go test ./...
   ```

   Plus any phase-specific check the brief's **Done bar** names (e.g. the
   production-shaped build, or `bin/check-migrations wiki` once migrations
   exist).

5. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "wiki Phase NN: <what this increment added>

   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
   ```

   Do **not** stage or commit `wiki/docs/brief.md` (it is gitignored). Then return
   `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module wiki` rooted at `wiki/`; pure-Go SQLite
  driver `modernc.org/sqlite` (no cgo). The in-repo `appkit` and `eventplane` are
  replace-siblings; `github.com/ikigenba/agentkit` is a published, proxy-fetched
  dependency with no `replace` in `wiki/go.mod`.
- **"The suite is green"** means all of: `cd wiki && go build ./...`,
  `cd wiki && go vet ./...`, `cd wiki && gofmt -l .` (prints nothing),
  `cd wiki && go test ./...`, and `bin/check-migrations wiki` succeed with zero
  failures.
- **Migrations:** ordered SQL under `wiki/internal/db/migrations/`, embedded via
  `//go:embed` as `db.FS`. **Never hand-author a version number** — create one
  with `bin/new-migration wiki <name>`. Never edit a committed migration.
- **Determinism / seams:** the clock and every external effect (LLM provider, DB)
  are injected at the composition root (`cmd/wiki/main.go`), so domain code is
  tested with a fixed clock and no network. **The LLM is always mocked in tests**
  via the `internal/llm` seam — a capturing/scripted mock provider; **no test
  makes a live LLM call** and the suite is green offline with no
  `ANTHROPIC_API_KEY`. **The DB in tests is a real temp `modernc.org/sqlite`**
  opened on a temp path and migrated by the appkit runner. Pure functions
  (`normalize`, `ftsPhrase`, `stripCodeFence`, `SearchLimits.Resolve`, truncation)
  are table-tested directly.

## Boundaries

- Never read `wiki/docs/plan/*`, `wiki/docs/design/*`, or `wiki/docs/product.md`.
  The brief is your only source.
- Never edit `wiki/docs/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is
  verify's job alone.
- Never delete or edit `wiki/docs/brief.md`.
- Never return `DONE` or `CONTINUE`. You always return `NEXT`.

End your final message with exactly one JSON object and nothing after it:

```json
{"status": "NEXT", "message": "<one short sentence on what this increment landed>"}
```
