---
name: ralph
description: "Orientation map of the ralph autonomous-build family: interactive authoring, unattended execution, the ralph executor, and pointers to the project-local skills and generated prompts. Use for orientation around ralph, project/ specs, open-spec, seal-spec, build loops, and audit loops."
---

# Ralph

`ralph` builds a software project **unattended**: you settle a spec
interactively with an agent, then hand a generated prompt loop to the `ralph`
binary, which drives an agent in a fresh context every turn until the work
declares itself done.

This skill is the **map** — what each part is and how they connect. It states
no contracts of its own; every shape lives in exactly one owning document,
pointed at below.

## The two halves

- **Authoring (interactive, human-in-the-loop)** — you and an agent settle the
  `project/` spec in conversation, then a workflow writes it in one pass.
- **Execution (unattended, no human)** — the `ralph` binary re-runs generated
  prompt files in a loop until done. No interaction; all state lives in the
  workspace.

Intelligence lives at the top (settling the spec); everything below is
mechanical execution.

## The pieces and their owners

| piece | what it is | contract lives in |
| --- | --- | --- |
| `$ikispec` skill | the `project/` artifact contracts: product, research, design, plan shapes + hard invariants | `.agents/skills/ikispec/SKILL.md` |
| `$open-spec` skill | opens the spec-authoring session: scopes work to `project/*`, docs-only, discussion of desired outcomes | `.agents/skills/open-spec/SKILL.md` |
| `$grill-me` skill | generic one-question-at-a-time interrogation, used to sharpen the goal before writing | `.agents/skills/grill-me/SKILL.md` |
| `$seal-spec` skill | the "go do the work" step: one automated pass writing product/research/design/plan per `ikispec` | `.agents/skills/seal-spec/SKILL.md` |
| prompt-generator skills | loop generators; each emits one loop topology (e.g. `$create-gather-build-verify-prompts`) + `project/loops/README.md` describing the installed loop | `.agents/skills/create-*/SKILL.md` |
| `project/` | the spec itself — the single source of truth the loop builds from | `project/README.md` (the workspace map) |
| `project/loops/` | the generated prompts + the installed loop's overview | `project/loops/README.md` |
| `ralph` binary | the executor | `~/projects/ralph/README.md` |

The spec shapes and the loop topologies are **independent**: generators consume
the spec structure but the spec knows nothing about any particular loop. New
loop families (different prompt sequences) are new generator workflows; the spec
contracts don't change.

## The workflow

Every session is the same arc, greenfield or brownfield:

```
$open-spec            open the session: scope to project/*, converse (user story / constraints; research as needed)
   │
$grill-me             interrogate to shared understanding
   │
$seal-spec            one pass: product · research · design (mint ids) · plan (append phases)
   │
$create-gather-build-verify-prompts     (once per project, or when the loop design changes)
   │
ralph project/loops/gather.md project/loops/build.md project/loops/verify.md
                     (operator-launched, unattended: gather ─► build ─► verify ─► …)
```

By convention the committed wrapper `project/loops/run` issues that full `ralph`
invocation (with the operator's chosen harness/model flags), so launching the
build is just `project/loops/run` — a typing-saver, nothing more.

## The executor — `ralph` (`~/projects/ralph`)

A Go binary that runs a sequence of prompt files, each as a clean-context agent
invocation, cycling until a prompt reports `DONE` or a budget rail trips. It is
**work-agnostic**: it owns only the lifecycle and the budget rails
(`--max-iterations/-time/-spend/-tokens`); the prompts own the work.

`ralph`'s only assumptions are that it runs from the **root directory of the
code it's changing** and that it's handed the **full paths to the prompt
files**. It knows nothing about what those prompts are named or where they
live; that is a per-project convention (ours:
`project/loops/{gather,build,verify}.md`).

Each turn ends with a status that drives the loop: `NEXT` (advance to the next
prompt, wrapping past the last back to the first), `DONE` (the whole job is
complete; stop), or `CONTINUE` (non-terminal; a streamed progress message).
State never lives in `ralph` or the model's memory between turns — only in
files. See `~/projects/ralph/README.md` for flags, the status schema, budget
rails, exit codes, and the run ledger at `~/.ralph/runs.jsonl`.

## Supporting reference skills

- `repo` — create a new repo with the three-tier git layout (GitHub
  push-mirror, bare source-of-truth at `/mnt/store/git/<org>/<repo>`, local
  clone at `~/projects/<repo>`).
