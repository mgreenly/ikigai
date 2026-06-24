# wiki

The **wiki** service for the ikigenba single-tenant suite — a knowledge base
(ingest / search / ask-RAG) exposed as **MCP**, deployed at
`<account>.ikigenba.com/srv/wiki/`. Loopback-only domain service over SQLite,
no UI and no token logic; nginx (owned by the dashboard) is the trust boundary.
See the repo-root `CLAUDE.md` for the suite-wide picture and the deploy model.

## Designing / planning / building work — this service has a live build loop

The design+plan workspace lives entirely under **`wiki/docs/`** (not the
repo-root `docs/`, which is suite-level only). The convention:

- **Design** is rewritten in place: `docs/design.md` (the spine) +
  `docs/design/DNN.md` (one file per Decision) + `docs/design/INDEX.md` (the
  manifest and the sorted `R-id → Decision` reverse map). Add a Decision by
  appending the next `DNN.md` and an INDEX entry — never renumber, never edit a
  shipped Decision's existing ids.
- **Plan** is append-only: `docs/plan.md` (the spine) + `docs/plan/phase-NN.md`
  (one per phase) + `docs/plan/STATUS.md` (the manifest — the **only** place a
  phase's `⬜`/`✅` marker lives). Extend by appending the next `phase-NN.md` and
  one STATUS line; never rewrite a finished phase except to flip its marker.

**The build loop already exists** — it is `docs/gather.md`, `docs/build.md`,
`docs/verify.md` (operator overview in `docs/LOOP.md`). After authoring/extending
the design+plan, the correct next step is **to run that loop**, not
`/create-loop` (the loop prompts are written and current). From the repo root:

```
ralph wiki/docs/gather.md wiki/docs/build.md wiki/docs/verify.md
```

`ralph` cycles the three prompts in fresh contexts (`gather → build → verify → …`);
`gather` picks the next `⬜` phase from `STATUS.md`, `build` writes the code +
id-tagged tests, `verify` is the independent gate that flips the one marker. The
loop stops when `gather` finds no `⬜` phase.
