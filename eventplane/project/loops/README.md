# eventplane — Build loop (as installed)

The unattended gather → build → verify loop that builds the routing revision
one phase at a time from `project/design/` + `project/plan/`. Start it from
the service root (`eventplane/`):

```
project/loops/run
```

which is exactly:

```
exec ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` re-invokes each prompt with a **fresh context** and advances on the
**final** message's status alone.

## Status contract

- `NEXT` — terminal: advance to the next prompt (wrapping verify → gather).
- `DONE` — terminal: stop the loop. **Only gather ever reports it**, and only
  when `STATUS.md` has no `⬜` phase left. Build and verify always end on
  `NEXT`.
- `CONTINUE` — non-terminal: the status a streaming model tags its mid-turn
  progress messages with. It never terminates a turn and never drives the
  loop; ralph reads only the last message.

## Who reads / writes / commits / flips what

| step | reads | writes | commits | flips marker |
|---|---|---|---|---|
| gather | `STATUS.md`, one `phase-NN.md`, `INDEX.md`, realized `DNN.md`(s) | brief contract region (fresh briefs only) | never | never |
| build | the brief only (both regions) | source + co-located id-tagged tests | yes — the increment | never |
| verify | the brief + the repo (re-derives truth) | brief feedback region; or deletes the brief | yes — the one-line STATUS flip on pass | pass only: `⬜→✅` |

## Brief lifecycle

`project/loops/brief.md` is the seam between the steps — the complete and
only input build and verify consume. It is **gitignored, never committed**,
single-phase, and **phase-scoped, not per-cycle**:

1. gather authors it once when a phase first becomes the active `⬜` phase;
   on every later cycle of the same phase, gather sees the matching
   `# Brief — Phase NN` header and no-ops (no big doc reads, feedback
   preserved).
2. build consumes it — feedback gaps first, then the remaining contract work.
3. verify either **passes** (flip the marker, commit, delete the brief) or
   finds **gaps** (overwrite the feedback region with only the currently-open
   gaps; brief persists into the next cycle).
4. On a **true stall** — the same gap ids unsatisfied for 3 consecutive
   attempts with no new build commit — verify logs to `~/.ralph/verify.log`,
   deletes the brief, and leaves `⬜`, so the next gather rebuilds the
   contract fresh from spec.

## Why it converges

Verify can neither halt nor advance a phase on a gap, so an incomplete phase
stays `⬜` and is re-attacked next cycle — with verify's command-grounded
feedback in front of build, and without gather re-reading the big docs (it
no-ops on the in-flight brief). The persisted feedback gives verify
cross-cycle memory: a shrinking/changing gap set is slow convergence; an
identical set with no new commit is a stall, answered by the trajectory reset
above. The only exit is gather → `DONE`, which requires zero `⬜` markers —
the run ends only when every phase is verified green (or a ralph budget rail
trips).

## The brief schema

Two regions, one writer each — the writers never clobber each other.

**Contract region** (gather-owned; written once per phase):

```markdown
# Brief — Phase NN
<one-line objective>

## Realized Decisions
- D<N> — <title> (project/design/DNN.md)

## Design — D<N> <title>
<the Decision's full design prose verbatim — Decision statement,
shapes/signatures, Rejected alternatives — with its Verification list OMITTED>

## Ids to cover
R-XXXX-XXXX — <full requirement text verbatim, same line>
(one id per line, id at line-start; or "(none — structural phase)")

## Files to touch
- <path> — <what changes>

## Dependency interfaces
<copied-in exported signatures, or "(none — no dependencies)">

## Done bar
<the phase's Done-when conditions verbatim: id-tagged co-located tests,
go test ./... + go vet ./... exit 0 from eventplane/, gofmt -l . empty,
plus the phase's own grep/diff checks>
```

**Feedback region** (verify-owned; overwritten each gap cycle, written empty
by gather):

```markdown
## Verify feedback — attempt N
- build commit: <sha>
- stall streak: <k>
- open gaps:
  - R-XXXX-XXXX — `<exact failing command>` → <observed output> (<file:line>)
```

The denominator is grep-able: `grep -oE '^R-[A-Z0-9]{4}-[A-Z0-9]{4}'
project/loops/brief.md` yields exactly the phase's id set.
