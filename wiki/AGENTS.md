# wiki

The **wiki** service for the ikigenba single-tenant suite — a knowledge base
(ingest / search / ask-RAG) exposed as **MCP**, deployed at
`<account>.ikigenba.com/srv/wiki/`. Loopback-only domain service over SQLite
that serves its MCP surface alongside a session-gated human **read surface** for
search and subject pages at the mount root, and runs no token logic; nginx
(owned by the dashboard) is the trust boundary.
See the repo-root `CLAUDE.md` for the suite-wide picture and the deploy model.

## Designing / planning / building work — this service has a live build loop

> ⚠️ **You do not change this service's code by hand unless the operator
> explicitly instructs you to in this session.** Absent that explicit
> instruction, your job in an interactive session is to **diagnose** and to
> **author/extend the design+plan docs** — never to write or edit Go code,
> tests, or migrations directly. All code (and its id-tagged tests) normally
> enters this service **only** through the ralph build loop's `build` prompt,
> against a planned phase. The default holds even for a one-line fix or an
> "obvious" bug — there is **no exception for trivial changes**, no "just this
> once," no "I'll write it and let the loop verify." And the override is
> **narrow and literal**: being asked to *fix the bug*, *troubleshoot*, or
> *deploy* is **not** being told to edit the code by hand — the instruction to
> change code directly must be explicit and unmistakable. When in doubt, you do
> not have it. When a fix is needed and you lack that instruction, capture it as
> a design Decision (`project/design/DNN.md` + INDEX) and a plan phase
> (`project/plan/phase-NN.md` + one `STATUS.md` line), then let the loop build it —
> or stop and hand the loop to the operator.
>
> **In bounds:** editing files under `wiki/project/` (design, plan, STATUS — this
> is how you feed the loop), running read-only diagnostics, and running the
> local suite (`bin/start`). **Out of bounds:** editing anything under
> `wiki/internal/`, `wiki/cmd/`, or `wiki/internal/db/migrations/`; generating a
> migration; or mutating service state (applying a migration, hand-editing a dev
> DB) to "test a fix." Diagnosing a bug ends at the proposal — capturing the fix
> in the design+plan is the handoff, not implementing it.

The design+plan workspace lives entirely under **`wiki/project/`** (not the
repo-root `docs/`, which is suite-level only); `project/README.md` is the map of
that workspace. The convention:

- **Design** is rewritten in place: `project/design/design.md` (the spine) +
  `project/design/DNN.md` (one file per Decision) + `project/design/INDEX.md` (the
  manifest and the sorted `R-id → Decision` reverse map). Add a Decision by
  appending the next `DNN.md` and an INDEX entry — never renumber, never edit a
  shipped Decision's existing ids.
- **Plan** is append-only: `project/plan/plan.md` (the spine) + `project/plan/phase-NN.md`
  (one per phase) + `project/plan/STATUS.md` (the manifest — the **only** place a
  phase's `⬜`/`✅` marker lives). Extend by appending the next `phase-NN.md` and
  one STATUS line; never rewrite a finished phase except to flip its marker.

**The build loop already exists** — it is `project/loops/gather.md`,
`project/loops/build.md`, `project/loops/verify.md` (the workspace layout is
documented in `project/README.md`). After authoring/extending the design+plan,
the correct next step is **to run that loop**, not
`/create-gather-build-verify-prompts` (the loop prompts are written and current).
`ralph` runs from this service directory and is handed the prompt paths:

```
ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
```

`ralph` cycles the three prompts in fresh contexts (`gather → build → verify → …`);
`gather` picks the next `⬜` phase from `STATUS.md`, `build` writes the code +
id-tagged tests, `verify` is the independent gate that flips the one marker. The
loop stops when `gather` finds no `⬜` phase.
