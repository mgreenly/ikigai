---
harness: claude
model: claude-sonnet-5
---
# gather — select the next phase and author its brief

You run from the **service root** (`webhooks/`); every path below is relative to
it. You are the **only** prompt that reads the big docs (design, plan, product).
Your whole job this turn is to make sure `project/loops/brief.md` holds a fresh,
self-contained contract for the next not-started phase. You **write no code, run
no tests, and commit nothing.** You own the brief's **contract region** only; you
never write or touch its **verify-feedback region**.

## Procedure

1. **Find the next phase.** Run:

   ```
   grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1
   ```

   (Phase lines in this tree are bare lines beginning with the literal word
   `Phase`, not Markdown bullets.)

   - **No match** (every phase is `✅`): the whole job is complete. Write no
     brief, change nothing, and report **`DONE`** (see *Reporting the result*).
     This is the **only** way the loop ever ends.
   - **A match:** note the zero-padded phase number `NN` (e.g. `14`), the Decision
     ids after `realizes` (e.g. `D14` or `D7, D8`), and copy the **entire matched
     `STATUS.md` line verbatim** — verify needs the exact text to flip its marker.

2. **Preserve an in-flight brief.** If `project/loops/brief.md` already exists,
   read its `# Build Brief — Phase NN` header:
   - If it names **this same phase**, the phase is mid-flight — its contract and
     any accumulated `verify` feedback must be preserved. **Leave the brief exactly
     as is** (both regions untouched), open no big doc, and report `NEXT`.
   - If it names a **different** (now-`✅`) phase, it is stale; overwrite it in the
     next steps.

3. **Read exactly the phase body** `project/plan/phase-NN.md` — its objective, the
   files it builds, its dependency phases, and its **Done when** bar.

4. **Resolve the Decision file(s).** For each `Dk` the phase realizes:

   ```
   grep -nE '^- Dk ' project/design/INDEX.md
   ```

   gives `project/design/D0k.md` and that Decision's `R-XXXX-XXXX` ids. Read only
   those Decision file(s) — their Decision text, Rejected alternatives, and
   **Verification** lists.

5. **Determine the ids to cover.** Take **only** the ids this phase's body /
   **Done when** lists — a slice of the realized Decision's Verification ids, never
   the whole set when the phase names a subset, and never an id from that same
   Decision the phase does not own. A phase whose `STATUS.md` line reads
   `realizes —` is **structural**: it owns no ids.

6. **Extract dependency interfaces.** For each earlier phase this one depends on,
   copy the **public signatures** build will call (exported types, funcs, methods,
   sentinels, constants) from the dependency package under `internal/` (or
   `cmd/webhooks/`) — so build never needs to open a design or plan file. Take them
   from the realized Decision text and the already-built source. Phase 14 depends
   on no earlier phase; write `(none — no earlier phase)`.

7. **Write `project/loops/brief.md`** to the schema below (overwrite any stale
   brief), with the **contract region filled in** and the **verify-feedback region
   empty** exactly as shown. Then report `NEXT`.

## `project/loops/brief.md` schema (emit exactly these sections)

```
# Build Brief — Phase NN: <one-line objective>

phase: NN
realizes: <D14 | D7, D8>
decision_files: <project/design/D0k.md[, …]>
status_line: <the exact STATUS.md phase line, verbatim>

## realized design (verbatim from the DNN.md — Verification list omitted)
<For each realized Decision, paste its full design prose copied verbatim from its
 DNN.md — the "# Decision N — …" header, the **Decision.** statement with its
 shape/signatures, and the **Rejected.** alternatives — but STOP before that
 Decision's "Verification." list (build must not see the ids the phase does not
 own).>

## ids to cover
<One id per line, each line in the exact form:
   R-XXXX-XXXX — <full requirement text copied verbatim from the Decision's Verification list>
 The id at line-start, an em-dash, then that id's complete requirement prose on the
 same line — never a bare id, never the text on a separate line. This keeps
 `grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md` an exact enumerator
 of the phase's ids. For a structural phase, the single line: (none — structural phase)>

## files to touch
<One path per line — the package(s)/files this phase builds, e.g.
 etc/nginx.conf, cmd/webhooks/nginx_test.go, internal/db/migrations/…;
 for repo-root harness edits use ../ paths, e.g. ../go.work, ../bin/start>

## dependency interfaces (copied — build must NOT open design/plan)
<go code block(s) of the exact exported signatures build will call, each labelled
 with its source phase/package; "(none — no earlier phase)" when this phase has no
 dependency phase>

## done bar
- <for each id: the behavior to assert and the substrate it must run against —
  real temp-file SQLite + injected deterministic Clock; httptest for handlers;
  real outbox for events; disk-read content assertions for the nginx fragment;
  no mocks for DB/outbox>
- suite green: `go build ./...` && `go vet ./...` && `go test ./...` all exit 0
- every id covered by a `// R-XXXX-XXXX`-tagged, genuinely-asserting test,
  co-located in the package under test (the cross-package end-to-end layer lives
  in `internal/e2e/`; the nginx content-assertion test lives in
  `cmd/webhooks/nginx_test.go`), named for the behavior — never a per-phase or
  root-level catch-all test file
- <ONLY if this phase's tests require the running suite (the D7 e2e ids through
  real nginx on :8080): note "requires the suite up via `../bin/start`; an
  all-skipped end-to-end layer is a GAP, not a pass">

## Verify feedback — attempt 0
(none yet)
```

## Boundaries

- Read only `project/plan/STATUS.md`, the one `project/plan/phase-NN.md`, the
  realized `project/design/D0k.md` file(s) (resolve via `INDEX.md`), and the
  dependency source under `internal/` / `cmd/webhooks/` for signatures. Consult
  `project/product/README.md` only if the phase objective is unclear.
- Never build, test, or commit. Never edit `STATUS.md`. Never write or touch the
  brief's **verify-feedback region**, and never touch an in-flight brief for the
  active phase. A fresh brief's **contract region** is your only output.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal**: the whole job is complete; the loop stops.
- `message` — one short, plain sentence describing what happened, e.g.
  `Brief written for phase 14.` or `Phase 14 already in flight; brief preserved.`

Report `DONE` **only** when the step-1 grep found no `⬜` phase left; in every
other case (a fresh brief written, or an in-flight brief preserved) report `NEXT`.
Keep `message` a single plain sentence — not a JSON object or code block.
