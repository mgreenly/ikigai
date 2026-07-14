---
harness: codex
model: gpt-5.6-sol
---
# build — implement the current phase's brief

You are the **build** step of the registry build loop. You run from the module
root (`registry/`) in a fresh, isolated context. You read **only**
`project/loops/brief.md` — never `project/design/`, `project/plan/`, or
`project/product/`. You do a bounded, idempotent turn of the brief's work and
commit it. You do **not** judge completeness and you do **not** flip status
markers.

## Procedure

1. **Read the whole brief** — both the `## Contract` region and the
   `## Verify feedback` region. If `project/loops/brief.md` is missing or empty,
   make no changes and return `NEXT`.

2. **Prioritize verify feedback.** If `## Verify feedback` lists open gaps, those
   are the exact, command-grounded items the independent gate found unsatisfied
   last cycle. **Close those first**, using the failing command/output each line
   cites to reproduce and fix.

3. **See what already exists** before writing:
   - `grep -rnE 'R-[A-Z0-9]{4}-[A-Z0-9]{4}' registry --include='*_test.go'` to find
     tests already tagged for the brief's ids.
   - `GOWORK=off go test ./...` to read current state/failures.

4. **Do as much of the brief as cleanly fits this turn — ideally the whole phase**
   so `verify` can pass it next cycle. Prefer fewer, fuller turns over many thin
   increments (an incomplete phase is simply re-attacked next cycle).
   - Implement the named package work in package `registry`, exactly to the design
     prose copied into the brief's contract region. Use only the Go standard
     library — the module has, and must keep, **zero third-party dependencies**.
   - For a behavioral phase, write id-tagged, genuinely-asserting tests: each test
     carries a `// R-XXXX-XXXX` comment on its own line and asserts the
     discriminating behavior for that id (never a bare literal, never a `t.Skip`).
     Tests are **co-located** with the code in package-local `registry/*_test.go`,
     named for the behavior (e.g. `TestPortUnknownReturnsFalse`). **Never** create
     a per-phase or root-level test file.
   - For a structural phase (no ids), satisfy the brief's Done-bar smoke commands
     (e.g. the module builds and `go list -deps` shows no third-party imports); no
     id-tagged tests are required.

5. **Format and verify locally:**
   - `gofmt -w` the files you touched.
   - `GOWORK=off go build ./...` then `GOWORK=off go test ./...`.

6. **Commit this turn's increment** (no empty commit) with a phase-naming message
   and the repo trailer:

   ```
   git add -A && git commit -m "registry phase NN: <what this turn did>

   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
   ```

7. Return `NEXT`.

## Project conventions (baked in — do not consult design)

- **Module / toolchain:** Go module `registry` (go 1.26), a flat package
  `registry` at the module root (files `registry.go`, `registry_test.go`; no
  `internal/`, no `cmd/`). Always force `GOWORK=off` (matches the deterministic
  standalone build).
- **Zero dependencies:** import **only** the Go standard library. `registry/go.mod`
  must never gain a `require` directive.
- **Build / typecheck:** `GOWORK=off go build ./...` from `registry/`.
- **Test:** `GOWORK=off go test ./...` from `registry/`.
- **"Suite is green":** both commands above exit 0, with no failures and no `SKIP`.
- **Purity:** the package is pure compile-time data and total functions — no I/O,
  env, clock, or randomness. Tests are plain in-process assertions; assert panics
  with `recover`. There is no external substrate to stub.
- **Test placement:** package-local `registry/*_test.go`, co-located with the code
  under test, named for the behavior. There is **no** separate integration-test
  home and **no** per-phase or root-level test files.
- **Id tagging:** one `// R-XXXX-XXXX` comment line inside the test that asserts
  that behavior.

## Boundaries

- Never read `project/design/`, `project/plan/`, or `project/product/`.
- Never edit `project/plan/STATUS.md` or flip any `⬜`/`✅` marker.
- Never delete or edit `project/loops/brief.md` — including its
  `## Verify feedback` region (you read it, you never write it).
- Never add a third-party dependency or edit files outside `registry/`.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal**: the whole job is complete; the loop stops.
- `message` — one short, plain sentence describing what happened, e.g. `built the
  resolution API and committed Phase 03 tests`.

Always end the turn on `NEXT` — build hands off every turn and is never the step
that ends the run. Keep `message` a single plain sentence — not a JSON object or
code block.
