# Loop: verify

You run from the **service root** (`github/`), in a fresh, isolated context. You
are the independent gate — the **only** prompt that flips a status marker or
deletes the brief. You **never halt** and **never advance a phase on a gap**. You
write no production code. You **re-derive current truth from scratch** every run:
you never trust `build`'s claims, and you read your own prior feedback only to
measure progress, not to believe it.

## Procedure

1. **Read the brief** — its contract region and its own prior `## Verify feedback`
   region. If `project/loops/brief.md` is missing or empty, report **`NEXT`**.

2. **Extract this phase's coverage denominator** from the brief:

   ```sh
   grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md
   ```

   If the brief's `## Ids to cover` is `(none — structural phase)`, this is a
   **structural phase**: there are no ids; verification is the green suite plus the
   brief's **Done when** smokes (run each named command and check its exact
   predicate).

3. **Run the full suite** (all green checks below must pass), from `github/`:

   ```sh
   GOWORK=off go build ./...
   GOWORK=off go vet ./...
   gofmt -l .                     # must print nothing
   GOWORK=off go test ./... -v 2>&1 | tee /tmp/github-verify.out
   ```

   Green requires: build exits 0, `go vet` clean, `gofmt -l .` empty, `go test`
   passes with **no failures**, and **no `SKIP`**:

   ```sh
   grep -E '^--- SKIP' /tmp/github-verify.out    # must be empty — a skipped test is a gap
   ```

4. **Check coverage for every denominator id.** An id counts as **covered** only
   when a test tagged `// R-XXXX-XXXX` **genuinely asserts** the behavior in the
   id's requirement text **and actually runs** under the command in step 3:

   ```sh
   grep -rn "// R-XXXX-XXXX" --include=*_test.go .   # locate the tagged test(s)
   ```

   (`--include=*_test.go` restricts to test code and never matches the `project/`
   docs that quote ids — keep every doc-style grep scoped this way.) Then, for each
   id, **statically trace reachability**: read the tagged test and every skip /
   build-tag / env gate guarding it. Treat as **uncovered** — no matter how genuine
   the assertion reads —:
   - an id with no tagged test, or a tag on a test that only checks a literal / does
     not assert the discriminating property;
   - a test gated behind a build tag or env flag that **nothing in the repo sets**,
     so step 3's invocation never runs it;
   - a test that converts a real failure (non-zero exit, unparseable output) into a
     `SKIP` or a pass.

   A **skip is never acceptable green** for a requirement. When unsure a test truly
   asserts, treat the id as **uncovered**.

5. **Decide.**

   - **Pass** — suite green (step 3) **and** every denominator id covered (step 4),
     or, for a structural phase, suite green **and** every Done-when smoke's
     predicate met. Then:
     - Flip **only this phase's** `⬜→✅` in `project/plan/STATUS.md` (change no
       other line).
     - Commit the one-line flip with a message naming the phase and the trailer
       `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
     - `rm -f project/loops/brief.md`.
     - Report **`NEXT`**.

   - **Gap** — any check in step 3/4 fails. Leave the marker `⬜`, change **no**
     source. Collect the **open gaps**: each an uncovered/failing id (or structural
     smoke) with the **exact command + observed output** that proves it open.
     Then measure progress and either reset or record feedback (below). Report
     **`NEXT`**.

### Gap: measure progress, then reset or record

Read the prior `## Verify feedback — attempt N` region (if any) for its attempt
counter `N`, its recorded build commit, and its prior open-gap id set. Capture the
current build commit: `git rev-parse HEAD`.

- **No progress** this cycle = the current open-gap id set is a subset of the prior
  one **and** the build commit is unchanged (build committed nothing new).
  Increment the stall streak on no-progress; reset it to `0` otherwise.

- **Stall reset** — when the streak reaches **3** (same gaps unsatisfied across
  three consecutive no-progress attempts): the accumulated brief is not
  converging. Append one line to `~/.ralph/verify.log`:

  ```
  <YYYY-MM-DD> Phase NN STALLED after N attempts: <gap ids>
  ```

  then `rm -f project/loops/brief.md`, leave the marker `⬜`, and report **`NEXT`**.
  The next `gather` rebuilds the contract fresh from spec. (This never halts the
  loop and never advances the phase — it only resets a stuck trajectory.)

- **Otherwise** — **overwrite** (never append) the `## Verify feedback` region with
  a single `## Verify feedback — attempt <N+1>` block carrying: the captured build
  commit, the stall streak, and a checklist of **only** the currently-open gaps —
  each line an `R-id` (or structural-smoke name) + the exact failing command + its
  observed output (+ `file:line` when known). Do **not** delete the brief. Report
  **`NEXT`**.

## Boundaries

- Never write or fix production code; never write the brief's contract region.
- Never flip a marker on anything short of green suite + full coverage (or, for a
  structural phase, green + all Done-when smokes).
- Never read the big design/plan docs to re-derive the checklist — the brief **is**
  the checklist.
- Treat a skipped or statically-unreachable id test as **uncovered**.
- Always report **`NEXT`** — verify hands off every turn, on a pass and on a gap,
  and is never the step that ends the run.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal — never yours to report**: ending the run is never yours —
  finishing this phase completely, green suite and all open gaps closed, is still
  `NEXT`; only gather, finding no `⬜` phase left, ever reports `DONE`.
- `message` — one short, plain sentence describing what happened, e.g.
  `Phase 03 verified green; flipped to ✅.` or `Phase 04 gap: R-EJS4-2851 untested (attempt 2).`

Always end the turn on **`NEXT`** (verify never ends the run — only `gather`'s
`DONE` does). Keep `message` a single plain sentence — not a JSON object or code
block.
