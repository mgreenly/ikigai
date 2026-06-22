# wiki/docs — permitted documents

Only **four** documents may exist under `wiki/docs`. Each is **singular** —
exactly one of it, never a slugged or per-feature variant — and each is **owned
by its `/*-mode` command**, which is the only sanctioned way to create or change
it. Nothing else may be added here.

| document | owner command | authority | edit discipline |
|---|---|---|---|
| `product.md` | `/product-mode` | the *why*, for whom, scope, user-facing promises | rewritten in place |
| `research.md` | `/research-mode` | prior-art / research that informs the design (optional, non-contractual) | rewritten in place |
| `design.md` + `design/` (`DNN.md`, `INDEX.md`) | `/design-mode` | the *how* and the proof of each behavior | rewritten in place |
| `plan.md` + `plan/` (`STATUS.md`, `phase-NN.md`) | `/plan-mode` | construction order and history | append-only |

## No other documents — adding any is forbidden

Do **not** create any document under `wiki/docs` other than the four above. In
particular:

- **No `<slug>-design.md` / `<slug>-plan.md` pairs**, and no per-feature or
  per-task doc of any kind. There is one design and one plan for the whole wiki.
- **No `feature_request-*.md`, ADRs, notes, scratch, or status files.**
- **No new file to capture a follow-on or a correction.** Work that continues or
  corrects the wiki is folded into the **existing** documents through the mode
  commands: update `product.md`/`design.md` in place (`/product-mode`,
  `/design-mode`) and **append** a phase to the plan (`/plan-mode`) — never a new
  document.

If a change seems to need a new document, that is the signal to re-enter the
owning mode command and fold it into the existing doc instead.

## Build-loop infrastructure (not documents)

`gather.md`, `build.md`, `verify.md`, and `LOOP.md` are fixed prompts for the
autonomous build loop — operator infrastructure, not subject documentation. They
are not authored ad-hoc and are not part of the document set above.
