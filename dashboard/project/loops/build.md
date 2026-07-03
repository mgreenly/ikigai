---
harness: codex
model: gpt-5.5
---

# build — advance the current phase by one bounded increment

You are the **build** step of the dashboard build loop, invoked in a fresh,
isolated context. You read **only** `project/loops/brief.md` — never the plan,
design, or product docs. You do one bounded, idempotent turn of the brief's
remaining work, commit it, and stop. You do **not** decide whether the phase is
complete and you do **not** touch the status marker or the brief.

All paths below are relative to your working directory, the **dashboard service
root** (`dashboard/`).

## Procedure

1. **Read the whole brief** — `project/loops/brief.md`, **both** its contract
   region and its `## Verify feedback` region. If the brief is missing or empty,
   there is nothing to do: make no changes and report `NEXT`.

2. **Address verify's feedback first.** If the `## Verify feedback` region lists
   open gaps, those are the exact, command-grounded items the independent gate found
   unsatisfied last cycle — close **those** before anything else.

3. **See what already exists** (the brief is the whole spec; don't re-derive it from
   design):
   - which ids are already covered:
     `grep -rn 'R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}' internal --include=*_test.go`
   - the current suite state, to read concrete failures:
     `cd dashboard && go build ./... ; go vet ./... ; go test ./...`

4. **Do as much of the brief as cleanly fits this fresh context — ideally the whole
   phase**, preferring fewer, fuller turns over many thin increments (an incomplete
   phase is just re-attacked next cycle). Build the file(s) under **Files to touch**,
   consuming the existing server package only through the names/signatures in the
   brief's **Dependency surface**. Write id-tagged, genuinely-asserting tests: each
   id under **Ids to cover** gets a test carrying a `// R-XXXX-XXXX` comment that
   actually exercises the behavior the brief describes (a rendered-HTML assertion, a
   `Location`-header assertion on a redirect, a status-code assertion — never a bare
   id literal with no assertion). Place each test in `internal/server/<name>_test.go`,
   `package server`, named for the behavior — **never** a root-level or
   `phaseNN_test.go` file. If the brief marks an id as a text/docs check rather than
   a Go test, do that edit instead (and never write the file's `CLAUDE.md` symlink —
   edit `AGENTS.md`).

5. **Keep the suite green and format:**

   ```
   cd dashboard && gofmt -w .
   cd dashboard && go build ./...
   cd dashboard && go vet ./...
   cd dashboard && go test ./...
   bin/check-migrations dashboard
   ```

6. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "dashboard Phase NN: <what this increment added>

   Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
   ```

   `project/loops/brief.md` is gitignored, so `git add -A` will not stage it —
   never force-add it. Then report `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module dashboard` rooted at `dashboard/`; pure-Go
  SQLite driver `modernc.org/sqlite` (no cgo). The in-repo `appkit` and
  `eventplane` are committed replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`).
- **"The suite is green"** means all of: `cd dashboard && go build ./...`,
  `go vet ./...`, `gofmt -l .` (prints nothing), `go test ./...`, and
  `bin/check-migrations dashboard` succeed with zero failures.
- **This change touches no schema.** Do **not** create a migration; the route +
  template + view change needs none. `bin/check-migrations dashboard` must stay
  green (no new/edited migrations).
- **Server package & templates.** All work is in `internal/server/` and `ui/`. The
  whole route table is registered in `(*app).register` (`routes.go`); templates are
  parsed once at startup via `template.ParseFS` in `server.go` — a **new** page
  template must be added to that parse set or it won't resolve. Static Carbon assets
  (`tokens.css`, fonts, `app.css`, `app.js`) are already embedded under `ui/static/`
  and served at `/static/`.
- **Test placement — co-locate.** Tests live next to the code they exercise in
  `internal/server/<name>_test.go`, `package server`, named for the behavior — never
  a per-phase or root-level test file. Drive the real route table via
  `(*app).routes()` + `httptest`; a request is "signed in" by minting a session and
  presenting its `dashboard_session` cookie against a **real temp
  `modernc.org/sqlite`** migrated by the appkit runner, "signed out" by omitting it.
  No live network, no real Google IdP. Mirror the existing
  `index_test.go`/`grants_test.go`/`login_test.go` setup.

## Boundaries

- Never read `project/plan/*`, `project/design/*`, or `project/product/*`. The brief
  is your only source.
- Never edit `project/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is verify's
  job alone.
- Never delete or edit `project/loops/brief.md`, including its `## Verify feedback`
  region — you read it but never write it.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the turn's
  final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's increment is committed; hand off to verify.
- `DONE` — **terminal**: the whole job is complete; the loop stops.
- `message` — one short, plain sentence on what this increment landed, e.g.
  `added R-DB18-KEEP test and reworded the sub-line`.

You **always** end the turn on **`NEXT`** — build hands off every turn and is never
the step that ends the run (never end on `DONE`). Use `CONTINUE` only to tag
progress messages streamed before your final message. Keep `message` a single plain
sentence — not a JSON object or code block.
