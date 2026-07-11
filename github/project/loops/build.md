# Loop: build

You run from the **service root** (`github/`), in a fresh, isolated context. You
read **only** `project/loops/brief.md` — never the big design/plan/product docs.
You do a bounded, idempotent turn of the brief's remaining work and commit it. You
do **not** decide completeness and you do **not** flip status markers.

## Procedure

1. **Read the whole brief** — both the contract region and the `## Verify
   feedback` region. If `project/loops/brief.md` is missing or empty, make no
   changes and report **`NEXT`**.

2. **Prioritize verify feedback.** If `## Verify feedback` lists open gaps, those
   are the exact, command-grounded items the independent gate found unsatisfied
   last cycle. Close **those first**, each tied to its `R-id` and its recorded
   failing command/output.

3. **See what already exists** (idempotent — you may be re-entering a
   partly-built phase):

   ```sh
   grep -rn "R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}" --include=*_test.go .
   GOWORK=off go test ./...        # read the current failures
   ```

4. **Build as much of the brief as cleanly fits this one turn — ideally the whole
   phase**, preferring fewer, fuller turns over many thin increments (an
   incomplete phase is simply re-attacked next cycle). Build the package(s) named
   in **Files to touch**, consuming dependencies **only** through the interface
   signatures copied into the brief (never open a design file to learn them). For
   every id under **Ids to cover**, write a genuinely-asserting test tagged with a
   `// R-XXXX-XXXX` comment that pins the discriminating property in the id's
   requirement text. For a structural phase (`(none — structural phase)`), satisfy
   the brief's **Done when** smokes instead (build + the named grep/command
   checks).

5. **Format, verify locally, commit.**

   ```sh
   gofmt -w .
   GOWORK=off go build ./...
   GOWORK=off go vet ./...
   GOWORK=off go test ./...
   ```

   Commit this turn's increment (never an empty commit) with a message naming the
   phase, and end the commit body with:

   ```
   Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
   ```

   Leave the `STATUS.md` marker `⬜`. Do not touch the brief. Report **`NEXT`**.

## Project conventions (this service)

- **Module root** for every command is `github/` (where you already run).
- **Build / typecheck:** `GOWORK=off go build ./...`. **Test:** `GOWORK=off go
  test ./...`. Forcing `GOWORK=off` matches the production build and proves the
  module resolves standalone via its `replace` directives.
- **"The suite is green"** means, all from `github/`: `GOWORK=off go build ./...`
  succeeds, `GOWORK=off go test ./...` passes with **no failures and no `SKIP`**,
  `gofmt -l .` prints nothing, and `GOWORK=off go vet ./...` is clean.
- **Zero new third-party dependencies.** Use only the Go standard library and the
  chassis already wired via `replace` (`appkit`, and `eventplane` only for a
  shared type). No `go-github`, no JWT library, no `x/oauth2`.
- **Test placement:** unit tests are **package-local `*_test.go`, co-located with
  the code they exercise and named for the behavior** (e.g. `internal/gh/*_test.go`,
  `internal/mcp/*_test.go`, `internal/web/nginx_test.go`). There is **no** separate
  integration-test home and **no** per-phase or root-level test file. Never create
  a test file outside the package whose code it exercises.
- **Offline tests only.** The GitHub client is exercised against an injected
  `http.RoundTripper` stub (redirect the package `apiBase`/host `var` at the stub);
  handlers via `httptest`. **No unit test performs live network I/O.** Never tag a
  mocked/stubbed test with a live-substrate id — the one such id (`R-DMUT-QF4A`) is
  verified out of loop and never appears in a brief.
- **Fail loudly.** Surface errors as typed values (`ErrAppAuth`, `ErrNotFound`,
  `ErrInvalid`); never swallow a failure or convert it into a silent success.
- **Never log a credential.** The private key and any token value are never
  written to logs or test output.

## Boundaries

- Never read design, plan, or product docs — the brief is your only input.
- Never edit `STATUS.md` or flip a marker; never create/delete/edit the brief
  (including its `## Verify feedback` region — you read it, never write it).
- Always report **`NEXT`** — build hands off every turn and is never the step that
  ends the run.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal — never yours to report**: ending the run is never yours —
  finishing this phase completely, green suite and all open gaps closed, is still
  `NEXT`; only gather, finding no `⬜` phase left, ever reports `DONE`.
- `message` — one short, plain sentence describing what happened, e.g.
  `Built internal/gh client and 15 id-tagged tests; suite green.`

Always end the turn on **`NEXT`** (build never ends the run). Keep `message` a
single plain sentence — not a JSON object or code block.
