# eventplane/project — workspace map

Everything needed to design, plan, and build the `eventplane` library lives
under `project/`, at the root of the codebase it governs (`eventplane/`). This
file is a map, not a manual — the artifact shapes live in the `ikispec` skill;
how the installed build loop works lives in `loops/README.md` (when present).

| folder | what's in it | written by |
|---|---|---|
| `product/` | `README.md` — the *why*: problem, users, scope, promises, success criteria | `$seal-spec` (rewritten in place) |
| `research/` | `research.md` — collected external ground truth that design references | `$seal-spec` (rewritten in place; optional) |
| `design/` | `README.md` (spine) + `INDEX.md` (manifest) + `DNN.md` (one per Decision) | `$seal-spec` (rewritten in place) |
| `plan/` | `README.md` (rules) + `STATUS.md` (manifest + `⬜`/`✅` markers) + `phase-NN.md` (one per phase) | `$seal-spec` (append-only) |
| `loops/` | the generated build-loop prompts + `README.md` describing the installed loop | a prompt-generator workflow |
| `bugs/` | free-form bug diagnoses / write-ups | free-form (not spec-owned) |
| `requests/` | free-form feature requests | free-form (not spec-owned) |

The spec governs only this library's tree (`eventplane/`): its two packages
(`outbox/`, `consumer/`) and any package the design adds. It never reaches into
a sibling service or the repo root.
