---
name: ikispec
description: "The project/ spec contracts: authoritative output shapes of product, research, design, and plan; authority boundaries; and hard invariants every writer keeps. Loaded by $open-spec, $grill-me, $seal-spec, and the loop-prompt generator workflows — not spoken by the user directly; use whenever any of those read or write the project/ spec."
---

# Spec Shapes

This skill is the **single source of truth for what the `project/` tree looks
like**. Everything that writes or reasons about the spec or spec-authoring
workflow — an `$open-spec` session's discussion, a `$grill-me` interrogation,
`$seal-spec`, and the loop-prompt generator workflows — takes the shapes below
from here and restates them nowhere.

## The workspace: `project/`

Everything needed to design, plan, and build the application lives under
`project/`, at the root of the codebase it governs. Every artifact has exactly
one writer:

| folder | what's in it | written by |
|---|---|---|
| `product/` | `README.md` — the *why*: problem, users, scope, promises, success criteria | `$seal-spec` (rewritten in place) |
| `research/` | `research.md` — collected external ground truth that design references | `$seal-spec` (rewritten in place; optional) |
| `design/` | `README.md` (spine) + `INDEX.md` (manifest) + `DNN.md` (one per Decision) | `$seal-spec` (rewritten in place) |
| `plan/` | `README.md` (rules) + `STATUS.md` (manifest + `⬜`/`✅` markers) + `phase-NN.md` (one per phase) | `$seal-spec` (append-only) |
| `loops/` | the generated build-loop prompts + `README.md` describing the installed loop | a prompt-generator workflow |
| `README.md` | the workspace map: this folder table and pointers — thin and static | `$seal-spec` |

The loop prompts and `loops/README.md` are **not** spec artifacts — they are
generated from the finished spec by a generator workflow and describe whichever
loop topology is installed. This skill owns the spec shapes; the generator owns
the loop shapes.

## Authority boundaries

The spec is split across authorities that **never restate each other** — each
fact lives in exactly one place:

- **Product owns *why/promise*** — outcomes only; no mechanism, formats, exit
  codes, or test assertions.
- **Research owns *external evidence*** — non-contractual facts design cites.
- **Design owns *shape + its checkable proof*** — seams, interfaces, types, the
  test strategy, and the minted `R-XXXX-XXXX` requirement-id denominator.
- **Plan owns *construction order + history*** — dependency-ordered phases,
  append-only.

Where product and design could overlap (behavior), product states the
*promise*; design states the *exact, checkable proof of that promise*. This
boundary is load-bearing — it is what keeps the three from overlapping.

## Hard invariants (no writer relaxes these)

- **Scope boundary.** A `project/` governs **only its own codebase** — the tree
  it sits at the root of, never a sibling service, the repo root, or shared
  tooling. No Decision may name a seam/file outside that tree; no phase may
  build/edit/test outside it. Cross-module work is a signal the work is
  misfiled, not a license to cross.
- **Authoring write boundary.** Spec authoring is a **docs-only mode**. During
  an `$open-spec` session's discussion, `$grill-me`, `$seal-spec`, and
  loop-prompt generator workflows, the only permitted writes are the
  `project/` artifacts named in
  the workspace table above, by their listed writers. Authors may describe
  future implementation paths such as `cmd/`, `internal/`, `go.mod`, `Makefile`,
  tests, `bin/`, or generated source, but **must not create, edit, format,
  scaffold, test, or commit them**. If an authoring run is about to touch an
  implementation/build file, it must stop before writing and report the boundary
  violation. If it already wrote such a file in the same run, it must revert
  only its own out-of-bound writes, report them, and continue authoring only
  after the checkout is back inside the boundary. Implementation files are
  produced only by an explicit build loop after the operator runs it; they are
  never produced by spec authoring. While in spec-authoring mode, do not present
  direct implementation as an available next-step choice.
- **Real minted ids.** Every design Verification item carries an `R-XXXX-XXXX`
  id minted with `idgen -n <count> -p R` — **never hand-written, never
  invented, never renumbered.** Fresh id per newly added behavior; delete an id
  (and its test) when its behavior goes. One id, one behavior, used in exactly
  one place.
- **Product, research, and design are the single CURRENT statement.** They are
  rewritten in place to match the goal as it stands now: a changed detail is
  *replaced*, not stacked beside its old self; a dropped one is *removed*. None
  of the three may contain a fact that is no longer true — no history, no
  "previously", no superseded paragraphs. History lives only in the plan.
- **The plan is APPEND-ONLY history.** The plan only grows: append new
  `phase-NN.md` files + new `STATUS.md` lines. Completed phases are the record
  of work already done — never rewrite, reword, or delete a finished
  `phase-NN.md`, never delete a `STATUS.md` line, even if the design it
  realized has since changed. The sole mutation to an existing phase is
  flipping its one `⬜ → ✅` in `STATUS.md`. A design that moved on is captured
  by a *new* appended phase.
- **Deterministic exit conditions.** Every phase carries a
  mechanically-checkable, reproducible, *reachable* "Done when" (green suite,
  exit code, exact match count) — never a prose judgment, never a
  self-referential/unsatisfiable check (classically a `grep` for a phrase the
  phase's own `project/` docs also contain, so it can never return empty).
  Structural/docs-only phases too: a green build plus a `project/`-excluded
  grep or a named smoke, never a prose claim.
- **Total coverage of the denominator.** The phases collectively realize
  **every** *current* design Verification id in **exactly one** phase — no
  current id unassigned, none split, none duplicated. Coverage is
  **one-directional**: design (rewritten in place, the current statement) is the
  denominator; the plan (append-only history) must cover all of it. Verify
  mechanically that no current design id is missing from the plan — the
  design-only difference must be empty:

  ```
  comm -23 <(grep -hoE 'R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/design/*.md   | sort -u) \
           <(grep -hoE 'R-[A-Z0-9]{4}-[A-Z0-9]{4}' project/plan/phase-*.md | sort -u)
  ```

  This prints the ids in design not yet covered by any phase; **empty output is
  the pass condition.** The reverse direction is deliberately **not** checked:
  the plan may — and over a long-lived project will — contain ids that are no
  longer in design. Those are **retired requirements** — a behavior that was
  built when its id was current, then dropped from design when it stopped
  applying. Its `phase-NN.md` is frozen and keeps the id forever as the record
  of that work; **never delete a retired id from the plan to chase parity** —
  doing so would violate the append-only invariant. An id present in the plan
  but absent from design is expected, not a defect. Because finished phases are
  frozen, a current id minted later can only be covered by a newly appended
  phase — coverage of the current denominator must be right at authoring time.

## `project/product/README.md` — the product shape

Owns **intent** — *why* this exists, *for whom*, what is in and out of scope,
and what we **promise** the user — stated once, in **outcome terms**. It must
NOT state mechanism, exact formats, exit codes, or test assertions; those
belong to design.

Sections, in order:

- **Title** — `# <name> — Product`.
- **Authority header** — a short paragraph beginning `**Authority: intent.**`
  stating what this doc owns and, explicitly, what it does not (mechanism,
  formats, exit codes, test assertions → design), plus the promise-vs-proof
  boundary.
- **## Problem** — the pain in the user's world; no solution yet.
- **## Purpose** — one paragraph: what the thing *is* and the single job it
  does.
- **## Users** — who runs it and what they are trying to get done.
- **## Scope** — what it does and, by exclusion, what it deliberately does not.
  Fold non-goals in as bounded "nothing else" statements; only break out a
  separate `## Non-goals` section when the exclusions genuinely need their own
  emphasis.
- **## Contractual constants** *(only if any exist)* — fixed, promised values
  the design must use verbatim and never re-declare (a baseline constant, a
  starting version, a protocol value). Promises, not implementation detail.
  Omit the section when there are none.
- **## What we promise (user-facing behavior)** — the observable behavior in
  outcome terms: what the user does and what they get back. Short example
  invocations/output where they sharpen the promise. No mechanism, no exit
  codes, no internal formats.
- **## Success criteria (outcomes)** — a bullet list of user-observable
  outcomes, each phrased as a *result* the user could confirm, never as a test
  assertion or mechanism. The verification gate runs the built artifact against
  exactly this list, so every item must be outcome-shaped and checkable
  end-to-end against the real thing.

## `project/research/research.md` — the evidence base

**Optional and non-contractual**: the build loop never reads it, and it feeds
no downstream doc mechanically. It exists for one exact thing: **collected
external ground truth, gathered so design never has to do web research.**
Typical contents: the exact API footprint of a REST service (just the parts the
design will use, documented precisely), a library's actual behavior, a
protocol's real constraints — and sometimes options deliberately evaluated and
*not* chosen, so better/worse can be judged side by side. Design decisions
reference these facts instead of re-deriving them.

Write it only when design depends on external facts not already in hand; skip
it entirely otherwise. It is rewritten in place — a single, coherent statement
of the current research, never a running log.

## `project/design/` — the design shape

Owns **shape and its proof** — *how* the thing is built and *how each behavior
is proven*. Product owns the why and the promises; design states the exact,
checkable form of those promises and never re-declares the why. It *uses* the
product's contractual constants by value but does not own them. It is the
single, current statement of the architecture: a changed Decision is rewritten
in place; stale decisions are removed, not stacked.

Design defines **how the software is tested** as part of the architecture —
seams exist so behavior can be exercised in isolation, and every component is
shaped so its correctness can be verified. Part of that responsibility is
identifying which claims **cannot** be proven in isolation — claims that hinge
on a real external contract (a provider/API accepting what it's sent, a real
DB/filesystem/network enforcing a constraint) — so the test plan exercises
those for real, not only the mockable ones.

Design docs carry interfaces, types, seams, naming, and the test plan.
Illustrative signatures, struct definitions, and interface declarations belong
in the doc; full implementations do not.

Split for **addressability** — a build phase reads only the one Decision it
realizes, never the whole architecture:

### `project/design/README.md` — the spine (cross-cutting facts only)

- **Title** — `# <name> — Design`.
- **Authority header** — a short paragraph beginning
  `**Authority: shape and its proof.**` stating what design owns (how + proof),
  that product owns the why and the promises, and that design uses the
  product's contractual constants by value but does not own them. Note this is
  the single current statement, rewritten in place, with history living in the
  plan.
- **## Requirement ids** — states plainly that: each Decision ends with a
  **Verification** list (the concrete behaviors that decision requires); every
  item carries a minted `R-XXXX-XXXX` id — a stable, unique handle for that one
  behavior; the ids live inline in these lists and nowhere else (**no separate
  requirements document**); and **design's responsibility for ids ends at
  minting them** — how coverage is measured and when the work is "done" are
  downstream's concern and must not be specified here.
- **## Conventions** — shared facts every Decision leans on. **Required, and
  must state the project's toolchain so downstream phases need not guess it:
  the exact build/typecheck command, the exact test command, and what "the
  suite is green" concretely means.** Also any other cross-cutting facts
  (language/version, module path, exit-code taxonomy, formatting rules, a
  shared time/IO source). State the commands, not the coverage rule.
- **## Layout** — describes the split: `INDEX.md` is the manifest; `DNN.md` is
  one self-contained file per Decision (zero-padded; referenced in prose and
  the plan as `D<N>`); this README holds only the spine. Restate that design is
  rewritten in place: a changed Decision is rewritten in its `DNN.md` and
  `INDEX.md` is regenerated; a new Decision adds a `DNN.md` and an INDEX entry.

### `project/design/DNN.md` — one self-contained file per Decision

- A header `# Decision N — <title>`.
- **Decision.** — the seams/interfaces/types/naming, with illustrative
  signatures and struct/interface declarations (never full implementations).
- **Rejected.** — the alternatives considered and why each lost.
- **Verification.** — a bullet list, each line
  `R-XXXX-XXXX — <the behavior a test must assert>`. State each behavior so it
  is *falsifiable*: a wrong implementation must fail it. Pin the discriminating
  property, not a weaker one a degenerate implementation also satisfies — when
  the Decision moves off a specific bad value or state, name the value or
  threshold the behavior excludes (e.g. "≥ 16384", not "non-zero"). A pure
  seam/structure decision with no behavior of its own says so explicitly and
  carries no ids (its proof is the behavioral ids of the decisions it enables).
  - **Verify the claim against a substrate that can falsify it — not a proxy a
    stub also passes.** Ask *what would have to be true for this test to fail,
    and can the chosen substrate make it fail?* A claim whose correctness
    depends on a real external contract is **not** verified by an assertion run
    against a mock or fake — the mock accepts whatever it's handed; such a test
    confirms a field was set, never that the system runs. For every such claim,
    mint a **distinct id whose test exercises the real dependency** — a
    live/integration/smoke check — name that substrate on the id, and name the
    observable outcome that proves it actually ran (a completed call, a
    returned result), not merely that a value was configured. If the
    architecture is shaped so an entire capability is only ever driven against
    mocks, that is itself the smell: at least one id must drive it end-to-end
    against the real thing.

### `project/design/INDEX.md` — the manifest

- **Title** — `# <name> — Design Index`.
- A short contract paragraph: each Decision maps to its `DNN.md`; every id maps
  to its Decision/file; id lookup is a grep against this index. Regenerate it
  whenever a Decision is added or its Verification ids change.
- **## Decisions** — one line per Decision in number order: `D<N>` → its file,
  its title, and the ids it owns (or "none — structural").
- **## Verification ids → Decision** — every minted id, **sorted**, each mapped
  to its Decision and file.

(The construction order that realizes the design lives in the plan — design
carries no `## Status` section.)

### Minting the Verification ids

The `R-XXXX-XXXX` ids are **real, minted ids — never hand-written or made up.**
Mint them with the `idgen` tool (`R` = requirement prefix):

```
idgen -n <count> -p R
```

Mint as many as a Decision's Verification list needs, assign one id per
behavior, and paste each inline. Ids are **stable handles**: when editing the
design in place, do **not** renumber or regenerate existing ids — mint a fresh
id for each newly added behavior; when a behavior is removed, delete its id
with it (its test goes too).

## `project/plan/` — the plan shape

Owns **construction order and history**. Unlike product and design, the plan is
**append-only**: phases are added at the end and marked done as they land;
completed phases are never rewritten or deleted, so the plan doubles as the
construction history. To extend the project: update product and design in place
first, then **append** a new phase.

**One phase = one package = one build-turn context.** Each phase is a single
coherent unit of work — almost always one package — scoped to that unit's
design Decisions and the *interfaces* (not internals) of the packages it
depends on, and **sized so the build loop can carry it in one fresh build-turn
context** and ideally finish it in a turn or two. The loop does *not* build a
phase in one long accumulating context — size to a single build turn, not an
imagined single sitting. Sizing a phase as large as cleanly fits one turn is
good: fewer cycles, less context churn. If a single Decision is too large for
one context it is split across phases, and each affected phase names the
**slice** of that Decision's Verification ids it carries.

Split for addressability — the loop greps a manifest for the next unit of work
and reads exactly one phase file, never the whole history:

### `project/plan/README.md` — the invariant rules (static; never grows)

- **Title** — `# <name> — Plan`.
- **Authority header** — a paragraph beginning
  `**Authority: construction order and history.**` stating that this document
  and the `project/plan/` directory own the build order and the record of
  what's built, that the plan is append-only, and how to extend it (update
  product and design in place, then append a new `phase-NN.md` + `STATUS.md`
  line — never edit a finished phase except to flip its marker). State the
  **coverage invariant** here too (every *current* design id realized in exactly
  one phase; coverage is one-directional, so the plan may also carry retired ids
  from frozen phases whose behavior has since left design; later current ids
  need a newly appended phase).
- **One phase = one package = one build-turn context** — the sizing paragraph
  above.
- **Done bar** — a phase is **done** when every Verification id it realizes (or
  its explicit slice) is covered by a clearly-named test and the suite is
  green; point at design's Conventions for what "green" concretely means; state
  that every phase's acceptance bar is deterministic exit conditions, never a
  subjective judgment, never a self-referential/unsatisfiable check.
- **## Layout** — `STATUS.md` is the manifest and the **only** home of status
  markers; `phase-NN.md` is one body file per phase (zero-padded; sub-phases
  keep their suffix, e.g. `phase-07a.md`); this README is the static rules.
  Restate append-only for the layout: never rewrite or delete a `phase-NN.md`,
  never delete a `STATUS.md` line; the only build-time mutation is flipping one
  phase's `⬜ → ✅` in `STATUS.md`.

### `project/plan/STATUS.md` — the manifest (the only home of status markers)

- **Title** — `# <name> — Plan Status`.
- **Contract paragraph** — one line per phase in build order, the only place a
  phase's marker lives; each phase line is a Markdown bullet beginning with
  `- Phase` carrying `✅` (done) or `⬜` (not started); the build loop finds its
  next work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`
  and reads only that phase's body file. Note it deliberately carries **no bare
  status glyph** outside phase lines, so the anchored grep matches only phase
  lines.
- **The phase lines** — `- Phase NN <marker> realizes <Decision ids>` (or
  `realizes —` for a pure structural phase) `— <one cohesive objective>`.
  Aligned and grep-able. A phase body file carries **no** marker of its own.

### `project/plan/phase-NN.md` — one body file per phase

- A header `# Phase N — <one cohesive objective>` — **no status token**.
- A `*Realizes design Decision <n> (<short label>)[ and <m> (...)]. Depends on
  Phase <k>.*` line — exactly which Decisions this phase builds and which
  earlier phase(s) it needs.
- A short body: what gets built (the package/seam and its paths), stated as the
  observable end state, not an implementation recipe.
- **Done when:** the acceptance bar as deterministic exit conditions — its
  Verification ids covered by genuine tests (each id listed with its behavior)
  and the suite green; a structural phase gets a deterministic check instead (a
  clean build, exact named files/targets, a `project/`-excluded grep).

## `project/README.md` — the workspace map (thin and static)

The top-level README is a **map, not a manual**: the folder table (as at the
top of this skill, adapted to the project), who writes each artifact, where the
codebase root is, and a pointer to `project/loops/README.md` for how the
installed build loop works. It carries **no** loop mechanics, no brief schema,
and no restatement of the shapes above. `$seal-spec` writes it and keeps it true
when the structure changes; the loop overview belongs to the generator workflow
that installed the loop.
