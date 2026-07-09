---
name: open-spec
description: Open a spec-authoring session over the project/ workspace. Use when the user says open-spec, $open-spec, or "open the spec" — it scopes the session to project/*, loads the spec contracts, and shifts into discussing desired outcomes before any spec is written.
---
First use the project-local `$ikispec` skill if it is not already in
context. It is the single source of truth for the `project/` shapes and hard
invariants this session operates under.

# Open Spec

**Opening the spec starts a spec-authoring session.** From this point until the
session is sealed, the work is *talking about* what the project should become —
not building it, and not yet writing it down.

An open session means:

- **Scope is `project/*`.** The session governs the codebase this `project/`
  sits at the root of, and any writes it eventually produces land only under
  `project/` — per `ikispec`'s scope and authoring-write boundaries. No
  implementation, build, or test files are created, edited, scaffolded, or
  committed while the session is open, and direct implementation is not
  offered as a next step.
- **The operator describes desired outcomes.** Listen, ask what's needed to
  follow, and hold the goal at outcome altitude. Track what's settled and
  what's still open, but do not start writing spec documents — discussion is
  not authoring.
- **The session has two named exits.** `$grill-me` interrogates the goal one
  question at a time until every unknown is resolved; `$seal-spec` writes the
  settled goal into `project/` (product, research, design, plan) in one
  automated pass and leaves the workspace ready for the next `ralph` run. The
  usual arc is **open-spec → grill-me → seal-spec**, but the operator drives:
  wait to be told which move comes next.

When the session opens, confirm briefly that the spec is open and scoped to
`project/*`, note whether `project/` already exists (greenfield or extension),
and let the operator talk.
