# github/project — workspace layout

Everything the github service needs to be **designed, planned, and built**
lives under `project/`. This file is the map. Paths are written relative to the
**service root** (`github/`), which is also the directory the `ralph` build loop
runs from.

## The folders

| folder | what's in it | owned by |
|---|---|---|
| `product/` | `README.md` — the *why*, for whom, scope, user-facing promises | `$seal-spec` (rewritten in place) |
| `research/` | design-informing research notes (`*-research.md`), if any | free-form (not mode-owned) |
| `design/` | `README.md` (spine) + `INDEX.md` (manifest + sorted `R-id → Decision` map) + `DNN.md` (one per Decision) | `$seal-spec` (rewritten in place) |
| `plan/` | `README.md` (spine) + `STATUS.md` (the manifest — the only home of each phase's `⬜`/`✅` marker) + `phase-NN.md` (one per phase) | `$seal-spec` (append-only) |
| `bugs/` | free-form bug diagnoses / write-ups | free-form (not mode-owned) |
| `requests/` | free-form feature requests | free-form (not mode-owned) |
| `loops/` | the `ralph` build-loop prompts: `gather.md`, `build.md`, `verify.md` (+ the ephemeral `brief.md`) | build-loop infrastructure |

There is also a loose `project/github-verification.md` — a one-off post-work
verification note that sits outside the spine folders.

The spine documents (`product/README.md`, `design/README.md`,
`plan/README.md`) are each singular and written by `$seal-spec` — the sanctioned
way to change them. The `bugs/`, `requests/`, and `research/` notes are informal
scratch and are *not* spec-owned. Don't add ad-hoc documents to the spine
folders; fold corrections and follow-ons into the existing spine docs via
`$seal-spec` (and append a plan phase) instead.

## The build loop

`ralph` is the autonomous executor. It runs **from this service directory**
(`github/`) and is handed the full paths to the three prompt files under
`loops/`:

```
ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

It cycles the prompts in fresh contexts — `gather → build → verify → …`:
`gather` picks the next `⬜` phase from `plan/STATUS.md` and writes a
self-contained brief, `build` writes the code + id-tagged tests, and `verify` is
the independent gate that flips the one marker. The loop stops when `gather`
finds no `⬜` phase.
