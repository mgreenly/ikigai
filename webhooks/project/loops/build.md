---
harness: codex
model: gpt-5.5
---
# build — do one bounded turn of the brief's remaining work

You run from the **service root** (`webhooks/`); every path below is relative to
it. You read **only** `project/loops/brief.md` — never design, plan, or product.
You do one bounded, idempotent increment of the phase's remaining work, write
id-tagged tests, run the suite, and commit. You **do not** decide completeness and
**do not** flip status markers — that is verify's job.

## Procedure

1. **Read the brief.** If `project/loops/brief.md` is missing or empty, make no
   changes and end with `{"status": "NEXT", "message": "No brief; nothing to build."}`.

2. **See what already exists** (this prompt is re-entered with a fresh context, so
   prior turns may have done part of the work):
   - `grep -rn "R-XXXX-XXXX" --include='*_test.go' .` for each id in the brief to
     see which are already covered;
   - run the suite (step 4) once to read current build/test failures.
   Do the **remaining** work only; never duplicate a file that already exists.

3. **Build the named package(s).** Implement exactly the `files to touch` in the
   brief, consuming dependencies **only** through the brief's copied interface
   signatures. Honor the suite conventions below. For any *new* (non-bootstrap)
   migration use `../bin/new-migration webhooks <name>` — never hand-pick a
   migration number; the bootstrap trio (`001_schema_migrations.sql`,
   `002_webhooks.sql`, `003_outbox.sql`) is the one fixed integer-prefixed set.

4. **Write id-tagged tests and run the suite.** For each id in the brief, write a
   genuinely-asserting test tagged with a `// R-XXXX-XXXX` comment, **co-located in
   the package under test** and named for the behavior (the D7 end-to-end layer
   lives in `internal/e2e/`). Then run, from the service root:

   ```
   go build ./...
   go vet ./...
   go test ./...
   ```

   "The suite is green" means **all three exit 0 with no failures**. If the brief's
   done bar says the phase requires the running suite (the D7 e2e ids), bring it up
   first with `../bin/start` and let the `internal/e2e` tests hit
   `http://localhost:8080`; never convert a real failure or an unreachable `:8080`
   into a `t.Skip` to make the suite "pass". `gofmt -w` everything you touched.

5. **Commit this turn's increment.** Stage your changes and commit with a message
   naming the phase (e.g. `webhooks build: phase NN — <objective>`). Do not create
   an empty commit; if nothing changed this turn, skip the commit. End the message
   with the trailer:

   ```
   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
   ```

6. End your final message with exactly:
   `{"status": "NEXT", "message": "<one short sentence on what advanced>"}`

## Project conventions (the substrate — bake these in)

- **Toolchain:** Go (`go 1.26`), module path `webhooks`, built on the `appkit`
  chassis over `modernc.org/sqlite` (pure-Go, no cgo); in-repo libs via committed
  `replace` (`appkit => ../appkit`, `eventplane => ../eventplane`).
- **Real substrate, no mocks for DB/outbox.** Tests run against **real temp-file
  SQLite** (`db.Open`, `t.TempDir()` — never `:memory:`) with a **deterministic
  injected `Clock`**; events run against the real `eventplane/outbox`. A mocked
  store/outbox cannot falsify a PK/UNIQUE constraint, durability across reopen,
  owner scoping, or Append-time registry validation, so it is forbidden for the
  integration layers.
- **Test placement:** unit/integration tests are co-located with the code they
  exercise as package-local `*_test.go` named for the behavior; the cross-package
  end-to-end (D7) tests live in the single dedicated `internal/e2e/` package.
  **Never** gather tests into a per-phase or root-level catch-all test file.

## Boundaries

- Never read design, plan, or product — the brief is your complete and only input.
- Never edit `project/plan/STATUS.md` or flip a marker; never delete or edit
  `project/loops/brief.md`.
- Never return `DONE` or `CONTINUE`. End with exactly one `NEXT` JSON object and
  nothing after it.
