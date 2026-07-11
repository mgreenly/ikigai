# dashboard/project — workspace map

Everything the dashboard service needs to be **designed, planned, and built**
lives under `project/`, at the root of the codebase it governs. Paths in these
docs are written relative to the **service root** (`dashboard/`), which is also
the directory the `ralph` build loop runs from. This file is a map, not a
manual: the folder table below and where to look next. The spec shapes
themselves are defined once, in the `ikispec` skill.

## The folders

| folder | what's in it | written by |
|---|---|---|
| `product/` | `README.md` — the *why*: problem, users, scope, promises, success criteria | `$seal-spec` (rewritten in place) |
| `research/` | `research.md` — external ground truth the design references (optional) | `$seal-spec` (rewritten in place) |
| `design/` | `README.md` (spine) + `INDEX.md` (manifest) + `DNN.md` (one per Decision) | `$seal-spec` (rewritten in place) |
| `plan/` | `README.md` (rules) + `STATUS.md` (manifest + `⬜`/`✅` markers) + `phase-NN.md` (one per phase) | `$seal-spec` (append-only) |
| `loops/` | the generated `ralph` build-loop prompts + `README.md` describing the installed loop | prompt-generator workflow |
| `bugs/` | free-form bug diagnoses / write-ups | free-form (not spec-owned) |
| `requests/` | free-form feature requests | free-form (not spec-owned) |

The spec artifacts — `product/`, `research/`, `design/`, `plan/` — are written
only by `$seal-spec` (open a session with `$open-spec`); that is the sanctioned
way to change them. The loop prompts and `loops/README.md` are **not** spec
artifacts: they are generated from the finished spec by a generator workflow.
`bugs/` and `requests/` are informal scratch owned by no one. Don't add ad-hoc
documents to the spec folders; fold corrections and follow-ons into the existing
spec docs via `$seal-spec` (and append a plan phase) instead.

## The build loop

The `ralph` build loop is described in `project/loops/README.md` — how the
`gather → build → verify` prompts cycle, what each reads and writes, and how it
converges. This map does not restate those mechanics.
