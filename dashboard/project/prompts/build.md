# build — advance the current phase by one bounded increment

You are the **build** step of the dashboard build loop, invoked in a fresh,
isolated context. You read **only** `dashboard/project/prompts/brief.md` — never the
plan, design, or product docs. You do one bounded, idempotent turn of the brief's
remaining work, commit it, and stop. You do **not** decide whether the phase is
complete and you do **not** touch the status marker or the brief.

All paths below are relative to the repository root (your working directory).

## Procedure

1. **Read the brief** — `dashboard/project/prompts/brief.md`. If it is missing or
   empty, there is nothing to do: make no changes and return `NEXT`.

2. **See what already exists** (the brief is the whole spec; don't re-derive it from
   design):
   - which ids are already covered:
     `grep -rn "R-DB[A-Z0-9]\{2\}-[A-Z0-9]\{4\}" dashboard --include=*_test.go`
   - the current suite state, to read concrete failures:
     `cd dashboard && go build ./... ; go vet ./... ; go test ./...`

3. **Do one increment of the remaining work.** Build the file(s) named under
   **Files to touch**, consuming the existing server package only through the
   names/signatures copied into the brief's **Dependency surface**. Write
   id-tagged, genuinely-asserting tests: each Verification id under **Ids to cover**
   gets a test carrying a `// R-DBxx-xxxx` comment that actually exercises the
   behavior the brief describes (a rendered-HTML assertion, a `Location`-header
   assertion on a redirect, a status-code assertion — never a bare id literal with
   no assertion). Place each test in `dashboard/internal/server/<name>_test.go`,
   `package server`, named for the behavior — never a root-level or
   `phaseNN_test.go` file. It is fine to land a subset this turn — the loop
   re-enters until the phase is fully green; favor a correct, committed increment
   over attempting everything at once.

   For **Phase 05** (docs-only, `R-DB16-DOCS`): the work is editing
   `dashboard/AGENTS.md` (purge the stale "single hybrid page / don't split" bullet,
   write the three-page truth). Edit through `AGENTS.md` — `dashboard/CLAUDE.md` is
   a symlink and updates automatically; do **not** write the symlink. There is no Go
   test for this id; verification is a text check.

4. **Keep the suite green for what you've written** and format:

   ```
   cd dashboard && gofmt -w .
   cd dashboard && go build ./...
   cd dashboard && go vet ./...
   cd dashboard && go test ./...
   bin/check-migrations dashboard
   ```

5. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "dashboard Phase NN: <what this increment added>

   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
   ```

   Do **not** stage or commit `dashboard/project/prompts/brief.md` (it is
   gitignored). Then return `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module dashboard` rooted at `dashboard/`; pure-Go
  SQLite driver `modernc.org/sqlite` (no cgo). The in-repo `appkit` and
  `eventplane` are committed replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`).
- **"The suite is green"** means all of: `cd dashboard && go build ./...`,
  `cd dashboard && go vet ./...`, `cd dashboard && gofmt -l .` (prints nothing),
  `cd dashboard && go test ./...`, and `bin/check-migrations dashboard` succeed with
  zero failures.
- **This change touches no schema.** Do **not** create a migration; the route +
  template + view change needs none. `bin/check-migrations dashboard` must stay
  green (no new/edited migrations).
- **Server package & templates.** All work is in `dashboard/internal/server/` and
  `dashboard/ui/`. The whole route table is registered in `(*app).register`
  (`routes.go`); templates are parsed once at startup via `template.ParseFS` in
  `server.go` — a **new** page template must be added to that parse set or it won't
  resolve. Static Carbon assets (`tokens.css`, fonts, `app.css`, `app.js`) are
  already embedded under `dashboard/ui/static/` and served at `/static/`.
- **Determinism / seams in tests.** The web-session store runs against a **real
  temp `modernc.org/sqlite`** migrated by the appkit runner (as the existing
  `internal/server` tests build it); a request is "signed in" by minting a session
  and presenting its `dashboard_session` cookie, "signed out" by omitting it. No
  live network and no real Google IdP. Drive the real route table via
  `(*app).routes()` + `httptest`. Mirror the existing
  `index_test.go`/`grants_test.go`/`login_test.go` setup.

## Boundaries

- Never read `dashboard/project/plan/*`, `dashboard/project/design/*`, or
  `dashboard/project/product/product.md`. The brief is your only source.
- Never edit `dashboard/project/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is
  verify's job alone.
- Never delete or edit `dashboard/project/prompts/brief.md`.
- Never return `DONE` or `CONTINUE`. You always return `NEXT`.

End your final message with exactly one JSON object and nothing after it:

```json
{"status": "NEXT", "message": "<one short sentence on what this increment landed>"}
```
