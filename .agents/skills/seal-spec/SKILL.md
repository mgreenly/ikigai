---
name: seal-spec
description: Explicit workflow for sealing a settled goal into the project/ spec in one automated pass — the closing move of an open-spec session. Use only when the user explicitly invokes seal-spec, says $seal-spec or "seal the spec", or clearly asks to write the settled goal into project/product, project/research, project/design, and project/plan.
---
First use the project-local `$ikispec` skill if it is not already in
context. It is the single source of truth for every output shape and hard
invariant below; do not paraphrase shapes from memory.

# Seal Spec

**Sealing the spec is the "go do the work" step — the closing move of an
`$open-spec` session.** It assumes the goal is already settled —
discussed in this session, usually sharpened by a `$grill-me` interrogation —
and turns that settled goal into the binding spec: product aligned, research
captured, design decided and minted, plan phases appended. It writes the docs;
it does not interview.

**Run to completion automatically, end to end, in one pass.** Make the
reasonable calls, write the docs, append the phases, and report. **Do not stop
to check in.** The only thing that halts you is a **true gap you cannot resolve
without a human** (see the last section). Absent that, finish the run.

Sealing trades interaction for speed, never correctness for speed: every shape
and hard invariant in `ikispec` holds exactly — the authority boundaries,
the scope boundary, real minted ids, rewrite-in-place for product/research/
design, the append-only plan, deterministic exit conditions, and total coverage
of the id denominator.

**Greenfield is the same run.** When `project/` is empty or absent, "current
state" is simply nothing, "align" means "write from scratch", and the appended
phases start at 01 — create the full structure from the shapes in `ikispec`,
including the thin `project/README.md` workspace map. (`project/loops/` is not
yours: a prompt-generator workflow writes it, as a separate,
operator-invoked step after the spec exists.)

## Procedure

1. **Read the current state.** The existing `project/product/README.md`,
   `project/research/research.md` if present, the design spine + `INDEX.md` +
   the `DNN.md` files it lists, and `project/plan/STATUS.md` + the phase files.
   Know what already exists before changing it. Settled Decisions are settled —
   don't reopen them. (Greenfield: note there is nothing, and build the
   structure from scratch as you go.)
2. **Product — align in place, only if the user would notice.** Product owns
   *user-facing* intent, so touch it **only when the discussed goal changes
   something the user could observe** — a new/changed promise, scope, or
   outcome. A purely under-the-hood change (swapping an algorithm, refactoring
   internals, a perf change with identical observable behavior) has **no
   product delta — skip this step entirely** and let design own it. When there
   *is* a user-facing change, fold it into `project/product/README.md` in
   outcome terms, keeping the mandated section order; rewrite affected sections
   and keep it a single coherent statement.
3. **Research — capture external ground truth design will need; otherwise
   skip.** Reach for it **only when design depends on external facts not
   already in hand** — typically facts from the web: an external API's exact
   footprint (just the parts the design will use, documented precisely), a
   library's real behavior, prior art worth comparing — including options you
   evaluate and *don't* choose, so better/worse is on the record. Do a focused
   pass and write/update `project/research/research.md` in place; design then
   references those facts instead of re-deriving them. When the codebase and
   existing docs already supply everything, omit this step entirely.
4. **Design — decide, mint, record.** For each new/changed behavior make the
   architectural call yourself (seams, interfaces, types, naming, test
   strategy). **Mint ids with `idgen -n <count> -p R` before writing them.**
   Write/rewrite the affected `DNN.md` (new Decision → new file; changed
   Decision → rewrite in place) and **regenerate `INDEX.md`** (Decisions list +
   sorted id→Decision map). Every verifiable element states how it's tested; a
   claim that hinges on a real external contract gets a distinct id whose test
   drives the real dependency, not a stub.
5. **Plan — APPEND the new phases.** For the newly-designed work, append
   dependency-ordered `phase-NN.md` files (header, `*Realizes … Depends on …*`
   line, observable end-state body, deterministic **Done when**), each sized to
   a single fresh build-turn context, and add one
   `- Phase NN ⬜ realizes <ids> — <objective>` line per phase to `STATUS.md`.
   Never touch finished phases. Before finishing, run the coverage check from
   `ikispec`: every *current* design id realized in exactly one phase (the
   design-only `comm -23` difference is empty). Coverage is one-directional —
   the plan may hold retired ids from frozen phases; never delete one to chase
   parity.
6. **Workspace map.** If the structure changed (greenfield, or a folder
   added/removed), write/update the thin `project/README.md` to match.
7. **Report.** List every path written, the Decisions added/changed with their
   minted ids, and the appended phase numbers. Then **stop**.

## Boundaries

- **Do NOT run the executor.** Authoring `project/` is always fine. Launching
  the `ralph` binary on the loop prompts is a separate, explicit, operator-only
  action — never start it on your own initiative.
- **Do NOT write, regenerate, or edit `project/loops/`.** The loop prompts and
  `loops/README.md` belong to the prompt-generator workflows. There
  is no step in a seal-spec run that touches a loop file.
- Everything else is `ikispec`' law: scope boundary, authority boundaries,
  minted ids, current-statement rewrites, append-only plan, deterministic exit
  conditions, total id coverage.

## The only reason to stop before finishing

Sealing runs to completion automatically. It makes the reasonable calls without
asking and never pauses just to confirm. Stop and ask the operator **only** when
you hit a **true gap you cannot resolve without a human**: a genuinely
load-bearing fork — one that changes the shape of what gets built — that the
discussed goal, the codebase, and the existing docs cannot settle. Even then,
stop *narrowly*: resolve and complete everything the gap doesn't block, and pose
the single specific question the run is stuck on. A choice with a reasonable
default is **not** a gap — decide it, note the call in your report, and finish
the run.
