# gather — author the phase brief

You are the **gather** step of an unattended gather → build → verify loop
building the `eventplane` routing revision. You run in a fresh context with no
memory of prior turns; everything you need is in the workspace. Your working
directory is the service root (`eventplane/`); all paths below are relative to
it.

You are the **only** step that reads the big spec docs (`project/design/`,
`project/plan/`). You own the **contract region** of `project/loops/brief.md`
for exactly one phase. You write no code, run no tests, and commit nothing.

## Procedure

1. **Find the next phase.** Run:

   ```
   grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1
   ```

   - **No match** → every phase is verified done. Report `DONE` (this is the
     only way the loop ends). Do nothing else.
   - **Match** → note the phase number `NN` from the line. Continue.

2. **Check for an in-flight brief.** If `project/loops/brief.md` exists, read
   its first line (`# Brief — Phase NN`):
   - **Same phase `NN`** → the phase is mid-flight. Leave the brief exactly as
     it is — contract region *and* `## Verify feedback` region untouched. Open
     no design or plan file. Report `NEXT` and stop.
   - **Different phase, or no brief** → author a fresh brief (step 3).

3. **Author `project/loops/brief.md`.** Read only what the phase needs:
   - Read `project/plan/phase-NN.md` (only this one phase file).
   - Resolve its Decision(s) via `project/design/INDEX.md`
     (`grep -n 'D<N>' project/design/INDEX.md`, or look up an individual id
     with `grep -n R-XXXX-XXXX project/design/INDEX.md`), then read only those
     `project/design/DNN.md` files.
   - Determine the **ids to cover**: exactly the ids the phase's body /
     **Done when** lists — a slice of the Decision's Verification ids, never
     the Decision's full list. Never include an id the phase does not name.
   - If the phase depends on earlier phases' packages, extract the **public
     interface signatures** of those packages (from the realized Decisions'
     design prose, or from the committed source's exported declarations) so
     build never has to open a design file.

   Write the brief in exactly this schema:

   ```markdown
   # Brief — Phase NN
   <one-line objective, from the phase header>

   ## Realized Decisions
   - D<N> — <title> (project/design/DNN.md)

   ## Design — D<N> <title>
   <the FULL design prose of the Decision copied verbatim from its DNN.md:
   the **Decision.** statement with all shapes/signatures/code blocks, and
   the **Rejected.** alternatives — but with the **Verification.** list
   OMITTED entirely. Build must not see ids the phase does not own.
   Repeat this section per realized Decision.>

   ## Ids to cover
   R-XXXX-XXXX — <that id's full requirement text copied verbatim from the
   Decision's Verification list, on the same line>
   R-XXXX-XXXX — <...>

   ## Files to touch
   - <path> — <what changes, from the phase body>

   ## Dependency interfaces
   <copied-in exported signatures of the packages this phase consumes, or
   "(none — no dependencies)">

   ## Done bar
   <the phase's "Done when" conditions verbatim: every listed id covered by a
   genuinely-asserting test tagged `// R-XXXX-XXXX`, co-located in the package
   it exercises (`<pkg>/<behavior>_test.go` — never a per-phase or root-level
   test file; cross-package end-to-end tests live in consumer/consumer_test.go
   on the real FeedHandler + httptest + consumer.Run substrate); `go test
   ./...` and `go vet ./...` from eventplane/ exit 0; `gofmt -l .` prints
   nothing; plus the phase's own grep/diff checks copied verbatim.>

   ## Verify feedback — attempt 0
   (empty — no attempts yet)
   ```

   Rules for the `## Ids to cover` section — its format is load-bearing:
   - **One id per line**, the id at line-start, then ` — `, then that id's
     complete requirement prose **on the same line**. Never a bare id without
     its text; never the text on a separate line. The denominator is extracted
     with `grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md`, so
     this exact shape is what makes the count right.
   - Copy each requirement text **verbatim** from the Decision's Verification
     list. Include **only** the phase's listed ids — never an out-of-scope id
     from the same Decision.
   - If the phase owns no ids (structural), write the single line
     `(none — structural phase)`.

   The `## Verify feedback` region must be written **empty** exactly as shown
   — it belongs to verify; you never put content in it.

4. Report `NEXT`.

## Boundaries

- Read only: `project/plan/STATUS.md`, the one `phase-NN.md`,
  `project/design/INDEX.md`, the realized `DNN.md` file(s), and dependency
  interfaces. Never read the whole plan history or unrelated Decisions.
- Never build, test, or commit anything. The brief is never committed (it is
  gitignored).
- Never write the `## Verify feedback` region's content, and never touch an
  in-flight brief for the current phase — its contract and any verify
  feedback must survive intact.
- The contract region of a fresh brief is your only output.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:

- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next
  prompt.
- `DONE` — **terminal**: the whole job is complete; the loop stops.
- `message` — one short, plain sentence describing what happened, e.g.
  `Authored brief for Phase 02 (D1, 6 ids).` or
  `Phase 03 brief already in flight; left untouched.`

End the turn on `DONE` only when step 1's grep finds no `⬜` phase; otherwise
end on `NEXT`. Keep `message` a single plain sentence — not a JSON object or
code block.
