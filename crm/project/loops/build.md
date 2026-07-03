---
harness: codex
model: gpt-5.5
---
# build — advance the current phase by one bounded increment

You are the **build** step of the crm build loop, invoked in a fresh, isolated
context. You read **only** `project/loops/brief.md` — never the plan, design, or
product docs. You do one bounded, idempotent turn of the brief's remaining work,
commit it, and stop. You do **not** decide whether the phase is complete and you
do **not** touch the status marker or the brief.

All paths below are relative to the **service root** (`crm/`), which is your
working directory.

## Procedure

1. **Read the brief** — `project/loops/brief.md`. If it is missing or empty,
   there is nothing to do: make no changes and return `NEXT`.

2. **See what already exists** (the brief is the whole spec; don't re-derive it
   from design):
   - which ids are already covered:
     `grep -rn "R-[A-Z0-9]\{4\}-[A-Z0-9]\{4\}" . --include=*_test.go`
   - the current suite state, to read concrete failures:
     `cd crm && go build ./... ; go vet ./... ; go test ./...`

3. **Do one increment of the remaining work.** Build the package(s) / artifact
   named under **Files to touch**, consuming dependencies **only** through the
   interface signatures and required shapes copied into the brief. For a
   **code** phase, write id-tagged, genuinely-asserting tests: each Verification
   id under **Ids to cover** gets a test carrying a `// R-XXXX-XXXX` comment that
   actually exercises the behavior the brief describes (never a bare id literal
   with no assertion). Place each test in the package it exercises —
   `crm/internal/web/web_test.go`, `package web`, named for the behavior — never
   in a root-level or `phaseNN_test.go` file. The nginx-fragment test reads
   `crm/etc/nginx.conf` from disk and asserts over its content; keep it in a
   focused package (e.g. `crm/internal/web`). For a **docs/structural** phase,
   make the doc edit and satisfy the named content check instead of writing
   id-tagged tests. It is fine to land a subset this turn — the loop re-enters
   until the phase is fully green; favor a correct, committed increment over
   attempting everything at once.

   - **Composition root.** `cmd/crm/main.go` is grown incrementally (e.g. adding
     the `GET /{$}` landing route to the existing `Handlers` hook) — that is
     wiring growth, not a domain rewrite. Leave the `POST /mcp` mount and the
     Service/Producer wiring intact.
   - **AGENTS.md / CLAUDE.md.** They are one file (`crm/CLAUDE.md` is a symlink to
     `crm/AGENTS.md`). Edit **`AGENTS.md`**; a refusal to write through the
     symlink is expected.

4. **Keep the suite green for what you've written** and format:

   ```
   cd crm && gofmt -w .
   cd crm && go build ./...
   cd crm && go vet ./...
   cd crm && go test ./...
   ```

   Plus any phase-specific check the brief's **Done bar** names (e.g.
   `bin/check-migrations crm`, or the docs purge's `grep -i "no UI" crm/AGENTS.md`
   finding nothing).

5. **Commit this turn's increment** (never an empty commit) with a message naming
   the phase, and the repo trailer:

   ```
   git add -A
   git commit -m "crm Phase NN: <what this increment added>

   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
   ```

   Do **not** stage or commit `project/loops/brief.md` (it is the ephemeral seam
   between prompts; do not commit it). Then return `NEXT`.

## Project conventions (inlined — do not open design to recover these)

- **Toolchain:** Go 1.26, single `module crm` rooted at `crm/`; pure-Go SQLite
  driver `modernc.org/sqlite` (no cgo). The in-repo `appkit` and `eventplane` are
  replace-siblings. The landing page adds **no new dependency** — standard library
  (`net/http`, `embed`, `html/template`) + the appkit chassis only.
- **"The suite is green"** means all of: `cd crm && go build ./...`,
  `cd crm && go vet ./...`, `cd crm && gofmt -l .` (prints nothing),
  `cd crm && go test ./...`, and `bin/check-migrations crm` succeed with zero
  failures.
- **No schema change for the landing page.** It touches no SQLite and adds **no**
  migration. (Never hand-author a migration version anyway — `bin/new-migration
  crm <name>` — but this work needs none.)
- **Determinism / seams:** the landing handler is pure over its two string inputs
  (`service`, `version`), injected at the composition root from
  `rt.Service()`/`rt.Version()`; tests construct it directly with fixed values and
  drive it with `net/http/httptest` — **no test makes a network call and no test
  needs a running suite**. Embedded assets (`tokens.css`, woff2 fonts) are real
  bytes via `//go:embed`.
- **Test layout:** co-locate every test with the code it exercises
  (`internal/web/web_test.go`, `package web`), named for the behavior asserted.
  A phase is one package, so its tests live in that package — never a root-level or
  `phaseNN_test.go` file.

## Boundaries

- Never read `project/plan/*`, `project/design/*`, or `project/product/product.md`.
  The brief is your only source.
- Never edit `project/plan/STATUS.md` or flip a `⬜`/`✅` marker — that is
  verify's job alone.
- Never delete or edit `project/loops/brief.md`.
- Never return `DONE` or `CONTINUE`. You always return `NEXT`.

End your final message with exactly one JSON object and nothing after it:

```json
{"status": "NEXT", "message": "<one short sentence on what this increment landed>"}
```
