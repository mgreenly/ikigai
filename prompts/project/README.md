# prompts/project — workspace layout

Everything the prompts service needs to be **designed, planned, and built** lives
under `project/`. This file is the map; each folder's spine is its `README.md`.
Paths are written relative to the **service root** (`prompts/`), which is also the
directory the `ralph` build loop runs from.

## The folders

| folder | spine | what's in it | written by |
|---|---|---|---|
| `product/` | `README.md` | the *why*, for whom, scope, user-facing promises | product authority (rewritten in place) |
| `research/` | `research.md` | design-informing external ground truth, plus free-form `*-research.md` notes | research authority (rewritten in place); other notes free-form |
| `design/` | `README.md` | the spine (cross-cutting facts) + `INDEX.md` (manifest + sorted `R-id → Decision` map) + `DNN.md` (one per Decision) | design authority (rewritten in place) |
| `plan/` | `README.md` | the spine (static rules) + `STATUS.md` (the manifest — the only home of each phase's `⬜`/`✅` marker) + `phase-NN.md` (one per phase) | plan authority (append-only) |
| `loops/` | — | the `ralph` build-loop prompts: `gather.md`, `build.md`, `verify.md` (+ the ephemeral `brief.md`) | build-loop infrastructure |

`bugs/` and `requests/` hold free-form diagnoses and feature requests — informal
scratch, owned by no authority. Don't add ad-hoc documents to the spine folders;
fold corrections and follow-ons into the existing spine docs (and append a plan
phase) instead.

## The build loop

`ralph` is the autonomous executor. It runs **from this service directory**
(`prompts/`) and is handed the full paths to the three prompt files:

```
ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

It cycles the prompts in fresh contexts — `gather → build → verify → …`:
`gather` picks the first `⬜` phase from `project/plan/STATUS.md` and writes the
ephemeral `project/loops/brief.md`; `build` reads only that brief and writes the
package + id-tagged tests; `verify` is the independent gate and the only prompt
that flips a `⬜ → ✅` marker. The loop stops when `gather` finds no `⬜` phase.
The prompt bodies under `project/loops/` are the authority on their own contract.
