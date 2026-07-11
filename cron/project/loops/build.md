---
harness: codex
model: gpt-5.6-terra
---
# build — advance the current phase toward done

You are the **build** step of the cron build loop, invoked in a fresh, isolated
context. You read **only** `project/loops/brief.md` — never the plan, design, or
product docs. You do a bounded, idempotent turn of the brief's remaining work,
commit it, and stop. You do **not** decide whether the phase is complete and you
do **not** touch the status marker or the brief.

All paths below are relative to the **service root** (`cron/`), which is your
working directory.

## Procedure

1. **Read the whole brief** — `project/loops/brief.md`, **both** its contract
   region and its `## Verify feedback` region. If the brief is missing or empty,
   there is nothing to do: make no changes and return `NEXT`.

2. **If `## Verify feedback` lists open gaps, those are this turn's priority.**
   They are the exact, command-grounded items the independent gate found
   unsatisfied last cycle — each tied to an `R-id` with the failing command and
   observed output. Close **those** first.

3. **See what already exists** (the brief is the whole spec; don't re-derive it
   from design):
   - which ids are already covered:
     `grep -rn "R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}" . --include=*_test.go`
   - the current suite state, to read concrete failures:
     `cd cron && go build ./... ; go vet ./... ; go test ./...`

4. **Do as much of the remaining work as cleanly fits this one fresh context —
   ideally complete the whole phase** so `verify` can pass it next cycle. Prefer
   fewer, fuller turns over many thin increments; an incomplete phase is simply
   re-attacked next cycle with verify's feedback in front of you. Build the
   package(s) / artifact named under **Files to touch**, consuming dependencies
   **only** through the interface signatures and required shapes copied into the
   brief.

   - **Code phase:** for each Verification id under **Ids to cover**, write a test
     carrying a `// R-XXXX-XXXX` comment that **genuinely asserts** the behavior
     the brief describes — never a bare id literal with no assertion, never a test
     that always passes, never a test that turns a real failure into a `t.Skip`.
   - **Structural / docs phase:** make the edit and satisfy the named content
     check instead of writing id-tagged tests.
   - **Composition root.** `cmd/cron/main.go` is grown incrementally (wiring
     growth, not a domain rewrite): the service is `appkit.Main(cronSpec())` with
     `cronSpec()` declared inline. Leave the crontab `Store`, the assembled
     `POST /mcp` handler, the LIVE `Publishes` provider, and the tick
     `Producer`/`Workers` wiring intact.

5. **Place every test in the package it exercises, named for the behavior** —
   never a root-level or `phaseNN_test.go` file. Landing / mux / composition-root /
   nginx-fragment tests live in `cron/cmd/cron/` (e.g.
   `cron/cmd/cron/main_test.go`); MCP-surface tests live in `cron/internal/mcp/`.
   A config-artifact test reads `cron/etc/nginx.conf` from disk and asserts over
   its content.

6. **Keep the suite green for what you've written** and format:

   ```
   cd cron && gofmt -w .
   cd cron && go build ./...
   cd cron && go vet ./...
   cd cron && go test ./...
   ```

   Plus any phase-specific check the brief's **Done bar** names.

7. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "cron Phase NN: <what this increment added>

   Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
   ```

   Do **not** stage or commit `project/loops/brief.md` (it is the ephemeral,
   git-ignored seam between prompts). Then return `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module cron` rooted at `cron/`; pure-Go SQLite
  driver `modernc.org/sqlite` (no cgo). The in-repo `appkit`, `eventplane`, and
  `registry` are committed replace-siblings. The web/MCP surfaces add **no new
  third-party dependency** — standard library + the appkit chassis
  (`appkit/web`, `appkit/mcp`) + `registry` only.
- **"The suite is green"** means all of: `cd cron && go build ./...`,
  `cd cron && go vet ./...`, `cd cron && gofmt -l .` (prints nothing), and
  `cd cron && go test ./...` succeed with zero failures.
- **The chassis owns the server.** cron is `appkit.Main(cronSpec())`;
  `cronSpec()` is declared inline in `cmd/cron/main.go`. The web surface is served
  from `cron/share/www/` through the chassis `appkit/web` mechanism (`WWW: true`,
  rendered via `rt.WWW()`); there is no `internal/web` package. The MCP surface is
  the `cron/internal/mcp` tool table (`Instructions` + `Tools(store)` +
  `NewHandler`) over the shared `appkit/mcp` transport. `cron/internal/db` holds
  **only** the embedded migration set and its guard tests.
- **No schema change unless a phase says so.** Never hand-author a migration
  version — `bin/create-migration cron <name>` — but most phases here need none.
- **Determinism / seams:** the landing handler is pure over its two string inputs
  (`service`, `version`), injected at the composition root from
  `rt.Service()`/`rt.Version()`; tests construct it directly with fixed values and
  drive it with `net/http/httptest` over the repo-real `share/www` tree — **no
  test makes a network call and no test needs a running suite**.
- **Test layout:** co-locate every test with the code it exercises, in a
  `*_test.go` file named for the behavior asserted. A phase is one package, so its
  tests live in that package — never a root-level or `phaseNN_test.go` file.

## Boundaries

- Never read `project/plan/*`, `project/design/*`, or `project/product/README.md`.
  The brief is your only source.
- Never edit `project/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is verify's
  job alone.
- Never delete or edit `project/loops/brief.md` — including its `## Verify
  feedback` region: you read it but never write it.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:

- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal — never yours to report**: ending the run is never yours —
  finishing this phase completely, green suite and all open gaps closed, is still
  `NEXT`; only gather, finding no `⬜` phase left, ever reports `DONE`.
- `message` — one short, plain sentence on what this increment landed, e.g.
  `added error_page 401 = @login_bounce to the two session-gated locations plus tests`.

Keep `message` a single plain sentence — not a JSON object or code block.
