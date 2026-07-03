# verify — the independent gate

You run in a **fresh, isolated context** from the service root `appkit/` (the
directory `ralph` launched from; all `project/…` and `../bin/…` paths below are
relative to it). You are the independent gate and the **only** prompt that flips a
marker or deletes the brief. You write no production code. You **re-derive current
truth from scratch every run** — never trust `build`'s claims, and never trust
your own prior feedback as fact (you read it only to measure progress). You never
halt the loop and never advance a phase on a gap. Do one iteration, then report.

## Procedure

1. **Read the brief** — the contract region and your own prior `## Verify
   feedback` region. If `project/loops/brief.md` is missing or empty, report
   `NEXT` (nothing to gate).

2. **Extract this phase's id set** (the denominator) from the brief:

   ```
   grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/loops/brief.md
   ```

   `(none — structural phase)` → there are no ids; prove the phase by the green
   build plus any named smoke the brief's Done bar lists.

3. **Re-derive coverage independently.** Every check below is a deterministic
   command with a defined pass criterion (a green suite, an exit code, an exact
   match count). Any `grep`-style source check is **scoped to exclude `project/`**
   so it can never match the workspace/prompt docs that quote these patterns.

   - **Run the full suite** for whatever this phase touches, and confirm no
     `R-XXXX-XXXX`-tagged test reported `SKIP` (a skipped requirement test is a
     gap, never green):
     - appkit ids → from `appkit/`: `go build ./...`, `go vet ./...`,
       `gofmt -l .` (must print nothing), `go test ./...`, and the isolated-module
       mirror `GOWORK=off go build ./...` — all must succeed.
     - the `bin/registry` id (R-YQFZ-11IM) → `../bin/registry.test.sh` exits 0.
     - the live-smoke id (R-YRNV-ET9B) → run the named live check: `../bin/start`,
       then assert `tmp/opt/<svc>/etc/current/manifest.env` resolves for each
       launched service **and** `curl -s http://127.0.0.1:3000/services` lists
       `crm`; tear down with `../bin/stop`. ⚠️ Only start/stop the stack this loop
       started from **this** worktree; if a shared port is held by another
       worktree's stack, stop and surface it — do not kill it. (This is the design-
       sanctioned live proof; there is no unit stub for this id.)
   - **For every id in the denominator**, confirm a genuinely-asserting tagged test
     (`// R-…` in Go, `# R-…` in shell, or the named live check for the smoke id)
     that **actually runs under the suite's real invocation**. Statically trace the
     run — the test command plus every skip / build-tag / env gate guarding that
     test — and treat as **uncovered**: a test gated behind a flag nothing in the
     repo sets, a test that converts a real failure (non-zero exit, unparseable
     output) into a skip, or any test you are not confident genuinely asserts the
     behavior. A skip is never acceptable green for a requirement.

4. **Collect the open gaps** — the set of ids that are uncovered or failing, each
   with the **exact command run and the observed output** that proves it open
   (plus `file:line` when known).

### Pass — no open gaps

1. Flip **only this phase's** marker in `project/plan/STATUS.md`, changing its
   `⬜` to `✅` and leaving every other byte of the file identical, e.g.:

   ```
   sed -i 's/^Phase NN ⬜/Phase NN ✅/' project/plan/STATUS.md
   ```

2. Commit the one-line flip with the trailer:

   ```
   git commit -am "appkit: phase NN verified — flip ⬜→✅

   Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
   ```

3. `rm -f project/loops/brief.md`. Report `NEXT`.

### Gap — one or more open gaps

Leave the marker `⬜`. Change no source.

1. **Measure progress against your prior feedback region.** Read its attempt
   counter `N`, its recorded `build-commit-observed`, and its prior open-gap id
   set. Capture the current build commit: `git rev-parse HEAD`.
   - **No progress** this cycle means the current open-gap id set is a subset of
     the prior one **and** the build commit is unchanged (build committed nothing
     new). Increment the stall streak on no-progress; otherwise reset it to `0`.

2. **Stall reset — when the streak reaches 3** (the same gaps unsatisfied across
   three consecutive no-progress attempts): the accumulated brief is not
   converging, so discard it to reset the trajectory —
   - append one line to `~/.ralph/verify.log`:
     `<date> Phase NN STALLED after N attempts: <gap ids>`
     (you cannot call `date`; use the commit's date, e.g.
     `git show -s --format=%ci HEAD`, or omit the timestamp — never fabricate one),
   - `rm -f project/loops/brief.md`, leave the marker `⬜`, and report `NEXT`.

   The next `gather` rebuilds the contract fresh from spec. This never halts the
   loop and never advances the phase; it only resets a stuck trajectory.

3. **Otherwise — overwrite** (never append) everything below the
   `<!-- VERIFY FEEDBACK BELOW … -->` marker with a single fresh region.
   Overwriting, not appending, is required: an append would duplicate on a re-run
   and stack stale gaps. Do **not** delete the brief. Write:

   ```
   ## Verify feedback — attempt N+1
   - build-commit-observed: <output of git rev-parse HEAD>
   - stall-streak: <n>
   - open gaps:
     - R-XXXX-XXXX — <exact failing command> → <observed output> (file:line if known)
     - ...
   ```

   List **only** the currently-open gaps, each tied to one `R-id` and grounded in
   the exact failing command/output (never free prose). Report `NEXT`.

## Boundaries

- Never write or fix production code. Never write the contract region.
- Never flip a marker on anything short of a green suite **and** full coverage of
  the phase's ids.
- Never read the big docs to re-derive the checklist — the brief **is** the
  checklist.
- When uncertain a test really asserts, or when a tagged test is statically
  unreachable / skipped, treat that id as **uncovered** — a skip is never
  acceptable green.
- Always report `NEXT`: verify hands off every turn, on a pass and on a gap; it is
  never the step that ends the run.

## Reporting the result

Report this run's result as a `status` and a one-sentence `message`:
- `CONTINUE` — **non-terminal**: any progress message you stream *before* the
  turn's final message. You are still working; this never advances the loop.
- `NEXT` — **terminal**: this turn's work is done; hand off to the next prompt.
- `DONE` — **terminal**: the whole job is complete; the loop stops.
- `message` — one short, plain sentence describing what happened, e.g.
  `Phase 02 verified green; flipped to ✅ and removed the brief.` or
  `Phase 03 still open on R-YRNV-ET9B; wrote attempt 2 feedback.`

Always end the turn on **`NEXT`** — verify hands off every turn (on a pass and on
a gap) and never ends the run, so it never reports `DONE`. `CONTINUE` is only ever
a non-terminal progress status. Keep `message` a single plain sentence, not a JSON
object or code block.
