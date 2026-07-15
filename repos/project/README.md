# repos/project — workspace layout

Everything the repos service needs to be **designed, planned, and built** lives
under `project/`. This file is the map. Paths are written relative to the
**service root** (`repos/`), which is also the directory the build loop runs
from.

## The folders

| folder | what's in it | written by |
|---|---|---|
| `product/` | `README.md` — the *why*, for whom, scope, user-facing promises | `$seal-spec` (rewritten in place) |
| `research/` | `research.md` — collected external ground truth design references | `$seal-spec` (rewritten in place) |
| `design/` | `README.md` (spine) + `INDEX.md` (manifest + sorted `R-id → Decision` map) + `DNN.md` (one per Decision) | `$seal-spec` (rewritten in place) |
| `plan/` | `README.md` (rules) + `STATUS.md` (the manifest — the only home of each phase's `⬜`/`✅` marker) + `phase-NN.md` (one per phase) | `$seal-spec` (append-only) |
| `loops/` | the generated build-loop prompts + `loops/README.md` describing the installed loop | a prompt-generator workflow |

The spine documents are singular and written by `$seal-spec` — the sanctioned
way to change them. Don't add ad-hoc documents to the spine folders; fold
corrections and follow-ons into the existing spine docs via `$seal-spec` (and
append a plan phase) instead.

The suite-level direction this service was carved from (the two-plane
architecture and the deferred release/materialization model) lives in
`docs/repos-design.md` at the repo root; this `project/` tree governs only the
`repos/` codebase and carries only the settled v1 scope.

See `project/loops/README.md` (once generated) for how the installed build loop
works.
