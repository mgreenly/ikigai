---
harness: codex
model: gpt-5.6-terra
---
# build — advance the current phase, closing verify's gaps first

You are the **build** step of the dropbox build loop, invoked in a fresh,
isolated context. You read **only** `project/loops/brief.md` — never the plan,
design, or product docs. You do a bounded, idempotent turn of the brief's
remaining work, commit it, and stop. You do **not** decide whether the phase is
complete, you do **not** flip the status marker, and you do **not** touch the
brief (including its feedback region).

All paths below are relative to the **service root** (`dropbox/`), which is your
working directory. Toolchain commands run **directly from here** (no `cd
dropbox`).

## Procedure

1. **Read the whole brief** — `project/loops/brief.md`, **both** the contract
   region and the `## Verify feedback` region. If it is missing or empty, there
   is nothing to do: make no changes and report `NEXT`.

2. **If `## Verify feedback` lists open gaps, address those first.** They are the
   exact, command-grounded items the independent gate found unsatisfied last
   cycle — each tied to an `R-id` and the failing command/output. Close them
   before anything else.

3. **See what already exists** (the brief is the whole spec; don't re-derive it
   from design):
   - which ids already have tagged tests:
     `grep -rn "R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}" . --include=*_test.go`
   - the current suite state, to read concrete failures:
     `go build ./... ; go vet ./... ; go test ./...`

4. **Do as much of the brief as cleanly fits this turn — ideally the whole
   phase.** Prefer fewer, fuller turns over many thin increments (an incomplete
   phase is simply re-attacked next cycle, so there is no benefit to stopping
   short). Build the package(s) / artifact named under **Files to touch**,
   consuming dependencies **only** through the interface signatures and required
   shapes copied into the brief.
   - **Code phase:** write id-tagged, genuinely-asserting tests — each
     Verification id under **Ids to cover** gets a test carrying a
     `// R-XXXX-XXXX` comment that actually exercises the behavior the brief
     describes (never a bare id literal, never an always-pass test, never a test
     gated behind a skip/flag nothing sets). **Co-locate every test with the code
     it exercises**, `package <pkg>`, named for the behavior — landing-page /
     `share/www` and nginx-fragment content tests in `cmd/dropbox` (driven over
     the shipped tree), MCP tool tests in `internal/mcp`, sync-engine tests in
     `internal/dropbox`, cross-package integration in `cmd/dropbox` — **never** a
     per-phase (`phaseNN_test.go`) or root-level test file.
   - **Docs / structural phase:** make the doc edit and satisfy the named content
     check instead of writing id-tagged tests.
   - **Composition root.** `cmd/dropbox/main.go` is grown incrementally (wiring a
     new route or Spec hook) — that is wiring growth, not a domain rewrite. Leave
     the `POST /mcp` mount, the loopback `GET /content` / `GET /list` byte routes,
     and the Service/Producer/Workers (sync engine) wiring intact.
   - **CLAUDE.md.** dropbox has **no `AGENTS.md` symlink** — `CLAUDE.md` is a
     single regular file; edit it directly for the docs phase.

5. **Format and confirm the suite is green** for what you've written (run
   directly from the service root):

   ```
   gofmt -w .
   go build ./...
   go vet ./...
   go test ./...
   ```

   Plus any phase-specific check the brief's **Done bar** names (e.g. the docs
   purge's `grep -i "no UI" CLAUDE.md` finding nothing).

6. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "dropbox Phase NN: <what this increment added>

   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
   ```

   Do **not** stage or commit `project/loops/brief.md` (it is the ephemeral seam
   and is git-ignored). Then report `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module dropbox` rooted at the service root;
  pure-Go SQLite driver `modernc.org/sqlite` (no cgo). `appkit`, `eventplane`,
  and `registry` are in-repo replace-siblings. The chassis owns the server
  (`appkit.Main(appkit.Spec{…})`); consume its surfaces (`appkit/web`,
  `appkit/mcp`, `rt.WWW()`, `Spec.Handlers`) only through the signatures copied
  into the brief.
- **"The suite is green"** means all of, run directly from the service root:
  `go build ./...`, `go vet ./...`, `gofmt -l .` (prints nothing), and
  `go test ./...` succeed with zero failures.
- **No schema change** unless the brief says so. Never hand-author a migration
  version — `bin/create-migration dropbox <name>` stamps one — but the
  landing-page / conversion work needs none.
- **Determinism / seams:** handlers are pure over injected inputs (e.g. the
  landing handler over `service`/`version` strings from `rt.Service()` /
  `rt.Version()`; MCP tools over an injected `dropbox.Service`); tests drive them
  with `net/http/httptest` and fixed values — **no test makes a network call and
  no test needs a running suite**. Shipped `share/www` assets are exercised as
  the real files that ship.
- **Test layout:** co-locate every test with the code it exercises, `package
  <pkg>`, named for the behavior asserted — never a per-phase or root-level test
  file.

## Boundaries

- Never read `project/plan/*`, `project/design/*`, or `project/product/README.md`.
  The brief is your only source.
- Never edit `project/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is
  verify's job alone.
- Never delete or edit `project/loops/brief.md`, including its `## Verify
  feedback` region — you **read** the feedback but never write it.
- You hand off every turn; ending the run is never yours.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:

- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal — never yours to report**: ending the run is never yours —
  finishing this phase completely, green suite and all open gaps closed, is still
  `NEXT`; only gather, finding no `⬜` phase left, ever reports `DONE`.
- `message` — one short, plain sentence describing what happened, e.g.
  `added error_page @login_bounce to both session-gated locations and tagged tests for Phase 21`.

You always report `NEXT` (even when you believe the phase is now fully done —
that call is verify's, not yours). Keep `message` a single plain sentence — not a
JSON object or code block.
