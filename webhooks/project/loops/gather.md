---
harness: claude
model: claude-sonnet-5
---
# gather — select the next phase and write the brief

You run from the **service root** (`webhooks/`); every path below is relative to
it. You are the **only** prompt that reads the big docs (design, plan, product).
Your whole job this turn is to pick the next not-started phase and distill it into
a fresh, self-contained `project/loops/brief.md`. **You write no code, run no
tests, and commit nothing.**

## Procedure

1. **Find the next phase.** Run:

   ```
   grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1
   ```

   - **No match** (every phase is `✅`): the whole job is complete. Write no brief.
     End your final message with exactly:
     `{"status": "DONE", "message": "All phases verified green."}`
     This is the **only** way the loop ends.
   - **A match:** note the zero-padded phase number `NN` (e.g. `01`), the Decision
     ids after `realizes` (e.g. `D2` or `D7, D8`), and copy the **entire matched
     `STATUS.md` line verbatim** — verify needs the exact text to flip its marker.

2. **Read exactly the phase body** `project/plan/phase-NN.md` — its objective,
   the files it builds, its dependency phases, and its **Done when** bar.

3. **Resolve the Decision file(s).** For each `Dk` the phase realizes:

   ```
   grep -nE '^- Dk ' project/design/INDEX.md
   ```

   gives `project/design/D0k.md` and that Decision's `R-XXXX-XXXX` ids. Read only
   those Decision file(s) — their Decision text and **Verification** lists.

4. **Determine the ids to cover.** Default to the realized Decisions' full
   Verification id sets, narrowed to the slice the phase's **Done when** explicitly
   assigns if it lists specific ids. A phase whose `STATUS.md` line reads
   `realizes —` is **structural**: it owns no ids.

5. **Extract dependency interfaces.** For each earlier phase this one depends on,
   copy the **public signatures** build will call (exported types, funcs, methods,
   sentinels, constants) from the dependency package — so build never needs to open
   a design or plan file. Take them from the realized Decision text and the
   already-built source under `internal/`.

6. **Write `project/loops/brief.md`** to the schema below (overwrite any
   existing one), then end your final message with exactly:
   `{"status": "NEXT", "message": "Brief written for phase NN."}`

## `project/loops/brief.md` schema (emit exactly these sections)

```
# Build Brief — Phase NN: <one-line objective>

phase: NN
realizes: <D2 | D7, D8>
decision_files: <project/design/D0k.md[, …]>
status_line: <the exact STATUS.md phase line, verbatim>

## ids to cover
<one bare R-XXXX-XXXX per line, so
 `grep -oE 'R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md` enumerates them;
 or the single line: (none — structural phase)>

## files to touch
<one path per line — the package(s)/files this phase builds, e.g.
 internal/db/db.go, internal/db/migrations/00X_*.sql, …;
 for repo-root harness edits use ../ paths, e.g. ../go.work, ../bin/start>

## dependency interfaces (copied — build must NOT open design/plan)
<go code block(s) of the exact exported signatures build will call, each labelled
 with its source phase/package; "(none — no earlier phase)" for phase 01>

## done bar
- <for each id: the behavior to assert and the substrate it must run against —
  real temp-file SQLite + injected deterministic Clock; httptest for handlers;
  real outbox for events; no mocks for DB/outbox>
- suite green: `go build ./...` && `go vet ./...` && `go test ./...` all exit 0
- every id is covered by a `// R-XXXX-XXXX`-tagged, genuinely-asserting test,
  co-located in the package under test (the D7 end-to-end layer lives in
  `internal/e2e/`), named for the behavior — never a per-phase or root-level
  catch-all test file
- <ONLY if this phase's tests require the running suite (e.g. the D7 e2e ids
  through real nginx on :8080): note "requires the suite up via `../bin/start`;
  an all-skipped end-to-end layer is a GAP, not a pass">
```

## Boundaries

- Read only `project/plan/STATUS.md`, the one `project/plan/phase-NN.md`, the
  realized `project/design/D0k.md` file(s) (resolve via `INDEX.md`), and the
  dependency source under `internal/` for signatures. Consult
  `project/product/product.md` only if the phase objective is unclear.
- Never build, test, or commit. Never edit `STATUS.md`. The brief is your **only**
  output.
- End with exactly one JSON object — `DONE` only when no `⬜` remains, otherwise
  `NEXT` — and nothing after it.
