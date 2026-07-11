# webhooks/project — workspace layout

Everything the webhooks service needs to be **designed, planned, and built** lives
under `project/`. This file is the only loose file here; everything else is in one
of the folders below. Paths are written relative to the **service root**
(`webhooks/`), which is also the directory the `ralph` build loop runs from.

## The folders

| folder | what's in it | owned by |
|---|---|---|
| `product/` | `product.md` — the *why*, for whom, scope, user-facing promises | `/product-mode` (rewritten in place) |
| `research/` | `research.md` — the design-informing research spine; plus free-form `*-research.md` working notes | `research.md`: `/research-mode` (rewritten in place). Other notes: free-form. |
| `design/` | `README.md` (spine) + `INDEX.md` (manifest + sorted `R-id → Decision` map) + `DNN.md` (one per Decision) | `/design-mode` (rewritten in place) |
| `plan/` | `README.md` (rules) + `STATUS.md` (the manifest — the only home of each phase's `⬜`/`✅` marker) + `phase-NN.md` (one per phase) | `/plan-mode` (append-only) |
| `bugs/` | free-form bug diagnoses / write-ups | free-form (not mode-owned) |
| `requests/` | free-form feature requests | free-form (not mode-owned) |
| `loops/` | the `ralph` build-loop prompts: `gather.md`, `build.md`, `verify.md` (+ the ephemeral `brief.md`) | build-loop infrastructure |

The four **spine documents** (`product/README.md`, `research/research.md`,
`design/README.md`, `plan/README.md`) are each singular and owned by a `/*-mode`
command — that command is the sanctioned way to change them. The `bugs/`,
`requests/`, and extra `research/*-research.md` notes are informal scratch and are
*not* owned by any mode command. Don't add ad-hoc documents to the spine folders;
fold corrections and follow-ons into the existing spine docs via the mode commands
(and append a plan phase) instead.

## The build loop

`ralph` is the autonomous executor. It runs **from this service directory** and is
handed the full paths to the three prompt files — the names and locations are this
project's convention (documented here); `ralph` itself assumes nothing about them:

```
ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

It cycles the prompts in fresh contexts — `gather → build → verify → …` — on a
**two-status** contract: each prompt ends with one JSON object whose `status` is
either `NEXT` (advance to the next prompt, wrapping `verify → gather`) or `DONE`
(stop). `CONTINUE` is unused.

- **gather** — the only prompt that reads the big docs. Greps `STATUS.md` for the
  first `⬜` phase
  (`grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`); if there is none it
  returns `DONE` (the sole exit). Otherwise it reads that one `phase-NN.md`,
  resolves its Decision(s) via `INDEX.md`, and writes a tiny, self-contained
  `loops/brief.md`, then returns `NEXT`.
- **build** — reads **only** `loops/brief.md`; builds the named package +
  id-tagged tests, runs the suite (`go build ./...`, `go vet ./...`,
  `go test ./...` from the service root), commits, leaves the marker untouched.
  Returns `NEXT`.
- **verify** — the independent gate and only prompt that flips a marker. Pass →
  flip that phase's `⬜→✅` in `STATUS.md` and commit; gap → leave it `⬜`. Either
  way it deletes `loops/brief.md`. Returns `NEXT`.

The loop is human-free and **converges**: `verify` can neither halt nor advance a
phase on a gap, so an incomplete phase simply stays `⬜` and is re-gathered and
re-attacked next cycle. The only stops are `gather`'s `DONE` (which requires zero
`⬜` markers — every phase verified green) or a `ralph` budget rail.

### `brief.md` — the ephemeral seam

`loops/brief.md` is the seam between the prompts that keeps `build`'s context
tiny: it is the **complete and only** input `build` and `verify` consume, so
neither opens design or plan. It is created by `gather`, deleted by `verify`,
single-phase (overwritten fresh each cycle), and **never committed** (it is
gitignored at the repo root). Its schema:

```
# Build Brief — Phase NN: <one-line objective>

phase: NN
realizes: <D2 | D7, D8>
decision_files: <project/design/D0k.md[, …]>
status_line: <the exact STATUS.md phase line, verbatim — verify flips this>

## ids to cover
<one bare R-XXXX-XXXX per line; or "(none — structural phase)">

## files to touch
<one path per line — the package(s)/files this phase builds; ../ for harness edits>

## dependency interfaces (copied — build must NOT open design/plan)
<go code block(s) of the exact exported signatures build will call>

## done bar
<each id → behavior + real substrate; "suite green"; the id-coverage + test-placement
 rule; and, only when the phase needs it, "requires the suite up via ../bin/start">
```

Because `gather` is the only big-doc reader and `verify` is the only
marker-flipper, the three prompts compose into a closed, self-correcting loop with
the brief as its single shared state.
