# build — do one bounded turn of the brief's remaining work

You run from the **service root** (`repos/`); every path below is relative to
it. You read **only** `project/loops/brief.md` — never design, plan, or product.
You do as much of the phase's remaining work as cleanly fits this one fresh
context, write id-tagged tests, run the suite, and commit. You **do not** decide
completeness and **do not** flip status markers — that is verify's job.

## Procedure

1. **Read the whole brief** — both the contract region **and** the
   `## Verify feedback` region. If `project/loops/brief.md` is missing or empty,
   make no changes and report `NEXT` with `No brief; nothing to build.`

2. **If the feedback region lists open gaps, they are this turn's priority.**
   They are the exact, command-grounded items the independent gate found
   unsatisfied last cycle, each tied to an `R-id` with the failing
   command/output — close **those** first.

3. **See what already exists** (this prompt is re-entered with a fresh context,
   so prior turns may have done part of the work):
   - `grep -rn "R-XXXX-XXXX" --include='*_test.go' .` for each id in the brief
     to see which are already covered;
   - run the suite (step 5) once to read current build/test failures.
   Do the **remaining** work only; never duplicate a file that already exists.

4. **Build the named package(s).** Implement exactly the `files to touch` in the
   brief, consuming dependencies **only** through the brief's copied interface
   signatures. Honor the suite conventions below. **Do as much of the brief as
   cleanly fits this turn — ideally complete the whole phase so `verify` can
   pass it next cycle — preferring fewer, fuller turns over many thin
   increments** (an incomplete phase is simply re-attacked next cycle). For any
   new migration run `../bin/create-migration repos <name>` — never hand-pick a
   migration number, never edit a committed migration.

5. **Write id-tagged tests and run the suite.** For each id in the brief, write
   a genuinely-asserting test tagged with a `// R-XXXX-XXXX` comment,
   **co-located in the package under test** and named for the behavior (see
   *Test placement* below). Then run, from the service root:

   ```
   go build ./...
   go vet ./...
   go test ./...
   gofmt -l .
   ```

   "The suite is green" means **the first three exit 0 with no failures and
   `gofmt -l .` prints nothing**. Never convert a real failure signal into a
   `t.Skip` to make the suite "pass" — a skipped requirement test is a gap.
   `gofmt -w` everything you touched.

6. **Commit this turn's increment.** Stage your changes and commit with a
   message naming the phase (e.g. `repos build: phase NN — <objective>`). Do
   **not** create an empty commit; if nothing changed this turn, skip the
   commit. End the message with the trailer:

   ```
   Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
   ```

7. Report `NEXT` (see *Reporting the result*).

## Project conventions (the substrate — bake these in)

- **Toolchain:** Go (`go 1.26`), module path `repos`, a standalone module on the
  `appkit` chassis over `modernc.org/sqlite` (pure-Go, no cgo); in-repo libs via
  committed `replace` (`appkit => ../appkit`, `eventplane => ../eventplane`)
  plus `require registry` (same pattern); the agent engine via the pinned tagged
  module `github.com/ikigenba/agentkit` (v0.2.0 line, matching prompts).
- **Peers by name, addresses from the registry.** The loopback port comes from
  `registry.MustPort("repos")` (3007) and peer base URLs from
  `registry.BaseURL("github")` etc. — never a hard-coded `127.0.0.1:30xx`
  literal in source (outside tests).
- **Config:** env only, prefix `REPOS_`, read at the composition root, never
  below it; passed down as values.
- **Real substrate, no mocks for DB/git.** Tests run against **real temp-file
  SQLite** through the full embedded migration set (`t.TempDir()`, never
  `:memory:`) with a **deterministic injected Clock**; git behavior runs against
  the **real `git` binary** and bare fixture remotes (`git init --bare` in
  `t.TempDir()`, `file://` URLs) — never a mocked git. Suite peers (github,
  webhooks) are `httptest` stubs that record requests; handlers are exercised
  with `httptest`; no live network I/O in unit tests. The nginx fragment is
  proven by reading `etc/nginx.conf` from disk and asserting over its content
  (nginx is not run by the suite).
- **Time / IO:** time enters through the `Clock` seam; the DB handle is the
  appkit single-writer `*sql.DB` (`rt.DB()`), shared with the producer outbox.
- **Test placement:** unit/integration tests are co-located with the code they
  exercise as package-local `*_test.go` named for the behavior; the
  cross-package assembled-handler, landing/web, and nginx content-assertion
  tests live in the composition-root package `cmd/repos/` (e.g.
  `cmd/repos/nginx_test.go`, mirroring the crm-clone web test set). **Never**
  gather tests into a per-phase or root-level catch-all test file.

## Boundaries

- Never read design, plan, or product — the brief is your complete and only
  input.
- Never edit files outside `repos/` — suite-level preconditions (registry row,
  `go.work`, `bin/start`) are operator-applied and out of scope.
- Never edit `project/plan/STATUS.md` or flip a marker; never delete or edit
  `project/loops/brief.md`, including its `## Verify feedback` region (you read
  it but never write it).
- You hand off **every** turn; you are never the step that ends the run.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal — never yours to report**: ending the run is never yours —
  finishing this phase completely, green suite and all open gaps closed, is
  still `NEXT`; only gather, finding no `⬜` phase left, ever reports `DONE`.
- `message` — one short, plain sentence describing what happened, e.g.
  `Phase 01 store and migrations built; suite green.`

Always end on `NEXT`. Keep `message` a single plain sentence — not a JSON object
or code block.
