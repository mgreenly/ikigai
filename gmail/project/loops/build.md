---
harness: codex
model: gpt-5.6-sol
---
# build — advance the current phase by one bounded increment

You are the **build** step of the gmail build loop, invoked in a fresh, isolated
context. You read **only** `project/loops/brief.md` — never the plan, design, or
product docs. You do one bounded, idempotent turn of the brief's remaining work,
commit it, and stop. You do **not** decide whether the phase is complete and you
do **not** touch the status marker or the brief.

All paths below are relative to the **service root** (`gmail/`), which is your
working directory.

## Procedure

1. **Read the whole brief** — `project/loops/brief.md`, **both** its contract
   region and its `## Verify feedback` region. If it is missing or empty, there
   is nothing to do: make no changes and report `NEXT`.

2. **Prioritize verify's open gaps.** If the `## Verify feedback` region lists
   open gaps, those are the exact, command-grounded items the independent gate
   found unsatisfied last cycle — each tied to an `R-id` with the failing command
   and observed output. **Close those first.**

3. **See what already exists** (the brief is the whole spec; don't re-derive it
   from design):
   - which ids are already covered:
     `grep -rn "R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}" . --include=*_test.go`
   - the current suite state, to read concrete failures:
     `cd gmail && go build ./... ; go vet ./... ; go test ./...`

4. **Do as much of the remaining work as cleanly fits this turn — ideally
   complete the whole phase** so `verify` can pass it next cycle. Prefer fewer,
   fuller turns over many thin increments (an incomplete phase is simply
   re-attacked next cycle). Build the package(s) / artifact named under **Files
   to touch**, consuming dependencies **only** through the interface signatures
   and required shapes copied into the brief.
   - For a **code** phase: each id under **Ids to cover** gets a genuinely
     asserting test carrying a `// R-XXXX-XXXX` comment that actually exercises
     the behavior the brief describes — never a bare id literal with no
     assertion. An nginx-fragment id is proven by a test that reads
     `gmail/etc/nginx.conf` from disk and asserts over its content.
   - For a **docs/structural** phase: make the doc edit and satisfy the named
     content check instead of writing id-tagged tests.
   - **Composition root.** `cmd/gmail/main.go` (`gmailSpec()`) is grown
     incrementally — wiring growth, not a domain rewrite. Leave the Gmail client
     + producer construction, the `POST /mcp` mount, and the `Producer`/`Workers`
     hooks (the outbox sink and the poll daemon) intact.
   - **AGENTS.md / CLAUDE.md.** They are one file (`gmail/CLAUDE.md` is a symlink
     to `gmail/AGENTS.md`). Edit **`AGENTS.md`**; a refusal to write through the
     symlink is expected.

5. **Keep the suite green for what you've written** and format:

   ```
   cd gmail && gofmt -w .
   cd gmail && go build ./...
   cd gmail && go vet ./...
   cd gmail && go test ./...
   ```

   Plus any phase-specific check the brief's **Done bar** names.

6. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "gmail Phase NN: <what this increment added>

   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
   ```

   Do **not** stage or commit `project/loops/brief.md` (it is the ephemeral seam
   between prompts). Then report `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module gmail` rooted at `gmail/`; pure-Go
  SQLite driver `modernc.org/sqlite` (no cgo). In-repo `appkit`, `eventplane`,
  and `registry` are replace-siblings. The web surface adds **no new third-party
  dependency** — templating/rendering/static-serving is the appkit chassis
  (`appkit/web`, `Spec.WWW`); the service ships only the on-disk `share/www` tree.
- **"The suite is green"** means all of: `cd gmail && go build ./...`,
  `cd gmail && go vet ./...`, `cd gmail && gofmt -l .` (prints nothing), and
  `cd gmail && go test ./...` succeed with zero failures.
- **No schema change here.** This work touches no SQLite and adds **no**
  migration. (Never hand-author a migration version anyway — use
  `bin/create-migration gmail <name>` — but this work needs none.)
- **Determinism / seams:** the landing handler is pure over its two string inputs
  (`service`, `version`), injected at the composition root from
  `rt.Service()`/`rt.Version()`; tests construct it and drive it with
  `net/http/httptest` — **no test makes a network call and no test needs a running
  suite**. Shipped assets (`tokens.css`, woff2 fonts) are real bytes on disk under
  `share/www/static/`, served by the chassis static mount.
- **Test layout:** co-locate every test with the code it exercises as a
  `*_test.go` file named for the behavior asserted — e.g.
  `gmail/cmd/gmail/nginx_test.go` (package main) for the nginx content assertions,
  `gmail/cmd/gmail/landing_test.go` for the landing render. A phase is one
  package, so its tests live in that package — never a root-level or
  `phaseNN_test.go` file.

## Boundaries

- Never read `project/plan/*`, `project/design/*`, or `project/product/README.md`.
  The brief is your only source.
- Never edit `project/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is
  verify's job alone.
- Never delete or edit `project/loops/brief.md` — including its `## Verify
  feedback` region: you **read** it but never write it.
- You hand off every turn — see below.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal — never yours to report**: ending the run is never yours —
  finishing this phase completely, green suite and all open gaps closed, is still
  `NEXT`; only gather, finding no `⬜` phase left, ever reports `DONE`.
- `message` — one short, plain sentence on what this increment landed, e.g.
  `added error_page 401 = @login_bounce to the two session-gated locations and its nginx_test assertions`.

You always report `NEXT` (never `DONE`). Keep `message` a single plain sentence —
not a JSON object or code block.
