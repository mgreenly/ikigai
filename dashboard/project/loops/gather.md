---
harness: claude
model: claude-sonnet-5
---

# gather — select the next ⬜ phase and author its brief

You run in a fresh, isolated context, one turn per invocation, as the first step
of an unattended `gather → build → verify` loop that builds the dashboard one
phase at a time. `ralph` runs from the service root (`dashboard/`), so every path
below is service-root-relative.

You are the **only** prompt that reads the big spec docs. Your single job is to
make sure `project/loops/brief.md` holds a correct, self-contained contract for
the **first unstarted phase** — then hand off. You write **no code, run no tests,
and commit nothing**. You own only the brief's **contract region**; you never
write its **feedback region**.

## Procedure

1. **Find the next unstarted phase.** Run:

   ```
   grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1
   ```

   `project/plan/STATUS.md` is the manifest and the only home of the `⬜`/`✅`
   markers; phase lines begin with the literal word `Phase` at column 0.
   - **If it prints nothing** (no `⬜` phase remains), the whole job is complete.
     Report **`DONE`** and stop. This is the sole end of the loop.
   - Otherwise, read the phase number `NN` from the matched line (e.g.
     `Phase 20 ⬜ realizes D17 — …` → `NN = 20`).

2. **Check for an in-flight brief.** If `project/loops/brief.md` exists, read only
   its first heading line `# Brief — Phase MM`:
   - **If `MM == NN`**, this phase is mid-flight — its contract and any `verify`
     feedback must be preserved. **Leave the brief exactly as is** (open no big
     doc, touch neither region), and report **`NEXT`**. You are done this turn.
   - If `MM != NN` (the brief is for an already-`✅` phase), or the brief is
     missing/empty, continue to step 3 and author a fresh brief.

3. **Read exactly the phase body** — `project/plan/phase-NN.md`. It names the
   Decision(s) this phase realizes and the **slice of ids** it covers (a phase may
   cover only some of a Decision's Verification ids — cover exactly those the
   phase lists, never the whole Decision's list).

4. **Resolve the realized Decision(s).** For each Decision the phase names, look it
   up in the manifest `project/design/INDEX.md` to get its `project/design/DNN.md`
   path, and read **only** those Decision files. Resolve an individual id with
   `grep -n R-XXXX-XXXX project/design/INDEX.md`. Read no other phase or Decision.

5. **(intent, only if needed)** You may read `project/product/README.md` for
   user-facing intent. Read no other big doc.

6. **Write `project/loops/brief.md`** to the exact schema below (overwrite any
   stale brief), copying:
   - the **full design prose** of each realized Decision — its Decision statement,
     shape/signatures, and rejected alternatives, **verbatim** from the `DNN.md`,
     but **omitting that Decision's Verification list** (build must not see ids the
     phase does not own);
   - under **Ids to cover**, **only** the ids the phase's body / *Done when* lists,
     one per line, each line exactly `R-XXXX-XXXX — <full requirement text copied
     verbatim from the Decision's Verification list>` (id at column 0, an em-dash,
     then that id's complete requirement prose on the same line). Never a bare id,
     never the text on a separate line, never an id the phase does not own. If the
     phase owns none, write the single line `(none — structural phase)`;
   - the **files to touch**, the **dependency interface signatures** copied in (so
     build never opens a design file), and the **done bar**;
   - a **`## Verify feedback`** region left empty.

   Then report **`NEXT`**.

## The `project/loops/brief.md` schema (emit exactly this shape)

```
# Brief — Phase NN

## Contract
<!-- gather-owned: written once when this phase became active. verify never writes here. -->

**Phase:** NN — <one-line objective>
**Realizes:** D<n>[, D<m>]
**Decision files:** project/design/DNN.md[, project/design/DMM.md]

### Design prose
<Decision statement + shape/signatures + rejected alternatives, verbatim per
realized Decision — the Verification list OMITTED.>

### Ids to cover
R-XXXX-XXXX — <full requirement text, verbatim from the Decision's Verification list>
R-YYYY-YYYY — <full requirement text …>
<!-- these are the ONLY lines in this file that begin with `R-` at column 0 -->
<!-- structural phase → the single line:  (none — structural phase) -->

### Files to touch
- <path> — <what changes>

### Dependency interface signatures
```go
// public signatures of the packages this phase consumes, copied so build
// never opens a design file
```

### Done bar
<deterministic exit conditions: the green suite (from design Conventions) AND
each id above covered by a co-located, genuinely-asserting `// R-id` test that
runs under `go test ./...` with no SKIP; a structural phase names its grep/smoke
instead.>

## Verify feedback
_(empty — no verify attempt yet)_
```

## Boundaries

- Read only: `project/plan/STATUS.md`, the one `project/plan/phase-NN.md`,
  `project/design/INDEX.md`, the realized `project/design/DNN.md`, and (if needed
  for intent) `project/product/README.md`. Read no other phase or Decision file.
- Never build, test, or commit; never edit `STATUS.md`; never write the
  `## Verify feedback` region; never touch an in-flight brief (same-phase header).
- The contract region of a freshly authored brief is your only output.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal**: the whole job is complete; the loop stops.
- `message` — one short, plain sentence describing what happened, e.g.
  `Authored brief for Phase 20 (D17, 5 ids).`

Report **`DONE`** only when step 1's grep found no `⬜` phase; in every other case
(brief authored, or an in-flight brief preserved) report **`NEXT`**. Keep
`message` a single plain sentence — not a JSON object or code block.
