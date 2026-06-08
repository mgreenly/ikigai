# Decisions — prompts: async runs + MCP surface redesign

Captured from the design discussion. These are the agreed decisions; **no plan
and no implementation yet** — the plan will be written in a later session with
full context. This document is the phase-1 reference: the settled external scheme
and execution model.

It supersedes the earlier proposal `/mnt/projects/empty/ikigenba_prompts-rename-handoff.md`
(written without access to the code). Where they differ, this document wins — see
"Relationship to the prior proposal" at the end.

## Context

`prompts` and `scripts` are treated as **two sides of one coin**: one
non-deterministic executor (Claude/Anthropic agent), one deterministic executor
(`python3`), otherwise nearly identical — both create / run / schedule / emit
events / consume triggers. The goal is a single coherent scheme that fits both,
with the code made to conform afterward. `scripts` (see `../scripts/scripts/`,
which already has async, first-class runs) is effectively the template for the
shared surface.

### Two distinct tool surfaces (only Surface 1 is in scope here)

- **Surface 1 — foreground MCP tools** (`ikigenba_prompts_*`): exposed to the
  **end-user's agent** looking in from outside. Run-scoped operations here carry
  an explicit `run_id`. **This is what these decisions cover.**
- **Surface 2 — the in-run toolset**: exposed to the sandboxed Claude agent
  executing *inside* a run. It is **auto-scoped** to that run's sandbox root — no
  `run_id` is ever passed. Separate concern; **not** part of this redesign.

## Core model — there is a *prompt* and a *run*

No "session", no "snapshot", no "version", no "definition object". Just two things:

- **prompt** — the current, editable definition. Lives in the DB. Addressed by
  `prompt_id`. Fields:
  - `name` — optional, human-readable label; **not unique, not a lookup key**.
  - `user_prompt` — the user-role prompt.
  - `system_prompt` — the system prompt.
  - `config` — `{ model, effort, max_tokens, temperature }`.
- **run** — a `run_id` that points at a **directory of artifacts on disk**. The
  DB row is just a **log line for the execution**: `run_id`, `prompt_id`,
  started-at, ended-at, status (`running | succeeded | failed | cancelled`). The
  substance lives on disk, not in the DB.

### Governing rule — "anything that can change goes on disk, per run"

Because the prompt is editable, its **execution-affecting** parts can differ from
one run to the next. So each run pins its own copy on disk and executes from
there. The split follows directly:

- **On disk, per run** (the changeable inputs + the outputs):
  - inputs: `user_prompt`, `system_prompt`, `config`
  - outputs: the run log, plus any files the agent produced
- **In the DB** (what does *not* vary per run): the prompt's one current
  definition, and the run as a log line.

`name` is editable too, but it does **not** affect execution — it is only a label
— so it stays in the DB and is **not** pinned per run. Only things that change
*what the run does* get captured per run.

This makes "preserve the exact version used, in case the prompt changes later"
fall out for free: the inputs that were executed are literally the files on disk
in the run dir. Editing or deleting the prompt afterward does not touch them.

## Execution model

- **Fully concurrent.** `run` is **always accepted**. The old single-flight
  gate / `ErrBusy` rejection is **removed**.
- **Per-run sandbox**, keyed by `run_id`. There is **no shared cross-run
  workspace** — each run is filesystem-isolated. (This is the deliberate
  trade-off for full concurrency; the old persistent per-session sandbox is
  gone.)
- Each run's sandbox **starts empty**. Seeding / carry-over from a prior run or
  template is an explicit **non-goal**.
- The per-run sandbox is an **output artifact**: it is **persisted after the run
  finishes** so the foreground `run_fs_*` tools can read it later. (Contrast
  scripts today, which deletes its `work/<run_id>` dir — see scripts deltas.)
- Retention / GC of run artifacts is **deferred** (not part of this work).

### Materialize → execute from disk → persist

At spawn:

1. The service **writes** the prompt's current `user_prompt`, `system_prompt`,
   and `config` into the run's on-disk dir.
2. The runner **reads those files from disk** to execute — it does **not** read
   the DB or the live prompt object at exec time. The runner's whole world is a
   run directory: inputs in, log + produced files out.
3. The input files are **persisted with the run**, so they *are* the record of
   exactly what ran.

The exact on-disk layout / file format of the inputs (e.g. separate
`user_prompt` / `system_prompt` text files + a `config.json`, vs one combined
`input.json`) is a **phase-2 implementation detail**, not settled here.

## Delete is a tombstone, not a purge

`delete` removes **only** the current prompt definition row from the DB. Run
log-lines and their on-disk artifacts are **append-only history** — never
cascaded by delete, reclaimed only by a future retention/GC pass.

- No `ON DELETE CASCADE` from a run to its prompt.
- A run stays addressable and readable by `run_id` after its prompt is gone.
- `prompt_id` on a run becomes a possibly-dangling historical label.

This matches the suite's grain (ledger's immutable journal, the append-only
outbox): the *definition* is editable/deletable; the *history of what happened*
is a fact you do not erase as a side effect.

## Foreground MCP surface

Prefix `ikigenba_prompts_`. **Naming rule:** the `ikigenba_prompts_` prefix
already names the object — so prompt operations are **bare verbs** (no
`prompt_`/`session_` sub-prefix), and run operations live under a **`run_`
sub-namespace**.

> **Airtight rule:** keyed by `prompt_id` → bare verb. keyed by `run_id` →
> `run_*`.

| tool | acts on | key |
|---|---|---|
| `health` | service | — |
| `describe` | service | — |
| `create` | the prompt | `prompt_id` (returns it) |
| `list` | the caller's prompts | — |
| `get` | the prompt | `prompt_id` |
| `update` | the prompt | `prompt_id` |
| `delete` | the prompt (tombstone) | `prompt_id` |
| `set_trigger` | the prompt | `prompt_id` |
| `clear_trigger` | the prompt | `prompt_id` |
| `run` | start a run → returns `run_id` | `prompt_id` |
| `run_list` | a prompt's runs | `prompt_id` |
| `run_get` | one run (the log line) | `run_id` |
| `run_output` | one run's output log | `run_id` |
| `run_cancel` | one run | `run_id` |
| `run_fs_list` | one run's produced file tree | `run_id` |
| `run_fs_read` | a file in one run's tree | `run_id` |

Notes:
- This **drops** the redundant `session_`/`prompt_` sub-prefix that exists today
  (`ikigenba_prompts_session_create` → `ikigenba_prompts_create`).
- The run-scoped sandbox readers are `run_fs_list` / `run_fs_read` (keyed by
  `run_id`), **not** the old `fs_list` / `fs_read` (which were `session_id`-keyed
  reads of the persistent sandbox).
- `health` and `describe` keep their names and take no inputs.

## Naming rationale (why these choices)

- **Object name `prompt`, not `session` / `task` / `job` / `agent`.**
  - Agrees with the immovable server name `ikigenba_prompts` — the *prompts*
    server manages *prompts*; an agent reasons about it with zero friction.
  - **Distinctiveness for a general-purpose-agent consumer:** `task`, `job`,
    `run`, `agent` are words the calling agent already owns for its *own local*
    actions, so naming the object that way makes it compete with the host agent's
    native verbs ("run that task" → does it locally). `prompt` is distinctive
    enough that a one-line description disambiguates fuzzy user phrasing.
  - With the persistent cross-run sandbox gone, there is no session-like durable
    state left to name — so `prompt` (just the current editable thing) is now
    *correct*, not a compromise.
- **Field names `user_prompt` / `system_prompt`** (not `text` / `instructions`):
  symmetric, and **mechanism-accurate** — they map exactly onto the Anthropic API
  roles (user turn / system prompt), so an agent composing a `create` call knows
  precisely where each string lands. Accepted cost: mild recursion
  (`prompt.user_prompt`), which is cosmetic and never ambiguous.
- **Drop the object sub-prefix on tools** (bare verbs): mirrors scripts; the
  service prefix already namespaces the object, so repeating it on every tool is
  noise. Gives the single airtight `prompt_id`→bare / `run_id`→`run_*` rule.

## The coin — prompts vs scripts

Identical in: **surface, lifecycle, full concurrency, materialize→exec→persist,
and tombstone-delete.** They differ in exactly three places:

1. **Input field**: `user_prompt` + `system_prompt` (prompts) vs `body` (scripts).
2. **Executor**: Anthropic agent (prompts) vs `python3` subprocess (scripts).
3. **Extra tools**: prompts adds the per-run `run_fs_list` / `run_fs_read`
   readers (scripts has none — its produced output is just stdout/stderr).

### Implied `scripts` PLAN deltas (to keep the coin symmetric)

These change the current `../scripts/scripts/PLAN.md` as written:

- **Delete becomes a tombstone.** §A5 currently says
  `DeleteScript(...) // cascades runs+triggers; caller removes logs`. Both halves
  change: do **not** cascade run rows, and do **not** remove artifacts. Delete
  removes the script row only; runs + artifacts survive as append-only history.
- **Persist the executed body.** §A6 currently materializes
  `work/<run_id>/main.py` and **deletes** it at stop. Instead, **persist** the
  body (and config) as an on-disk artifact in the run dir, so a script run can
  show the exact body it executed — the same "inputs that can change live on disk
  per run" rule. (scripts is otherwise already the template: concurrent,
  `run_id`-addressed, with `run_list` / `run_get` / `run_output` / `run_cancel`.)

### Implied `prompts` deltas (from the 2026-06-07 scripts reconciliation)

Walking the `scripts` PLAN against this doc surfaced changes that land on
**prompts**, not scripts. Captured here so prompts' future plan inherits them:

- **prompts gains general multi-source triggers.** "Consume triggers" is a shared
  coin trait, but prompts today consumes **cron only** while scripts is a general
  `{source, event_filter}` fan-in over all deployed producers (one consumer loop
  per upstream, validated against producer registries). To stay symmetric,
  **prompts adopts scripts' general trigger model** (scripts PLAN §A9/§A12).
  Committed prompts scope; does not block the scripts build. *(Premise "prompts is
  cron-only today" is from this doc, not yet verified against prompts code —
  confirm when planning.)*
- **Coin-difference #3 dissolved — both services now have `run_fs_*`.** Because
  scripts now **persists** its per-run dir (the "inputs on disk per run" rule
  applied to scripts), a script that writes files leaves a readable tree, so
  scripts also exposes `run_fs_list`/`run_fs_read`. The coin's three differences
  collapse to **two**: input field (#1) and executor (#2). Update the "three
  places" framing accordingly when revising this doc.
- **Tombstone delete — settle the trigger question the same way.** scripts
  resolved the open sub-question as: on delete, **runs do not cascade** (history),
  but **triggers do cascade** (a trigger is live, forward-looking definition, not
  history; a tombstoned object must not keep a binding that fires future work).
  prompts should mirror this once it has triggers.
- **Completion-event verb.** scripts aligned its event to the status enum:
  `scripts.succeeded`/`scripts.failed` (was `scripts.completed`). This matches
  prompts' `run.succeeded`/`run.failed` — confirm prompts keeps `succeeded`.

## Explicitly deferred / out of scope

- The plan, phasing, and any code changes (next session).
- Exact on-disk layout/format of the per-run input artifacts.
- Retention / GC of run artifacts.
- Sandbox seeding / cross-run carry-over (non-goal).
- Surface 2 (the in-run toolset) — unchanged by this work.
- **Tool descriptions & per-field schema guidance**: the desire to make every
  description and input-field doc "spot on" stands, and the prior proposal's
  drafts (its §C/§D, plus the settled `config` field text: model ids, `effort`
  set, `offset/limit`, etc.) are useful **input** — but they were written against
  the old single-flight / `last_run` / sub-prefixed surface and are **not
  ratified** here. Description text gets finalized against *this* surface during
  the plan/build.

## Relationship to the prior proposal

`/mnt/projects/empty/ikigenba_prompts-rename-handoff.md` was written without the
code and assumed the durable/ephemeral concepts were conflated. In reality the
code already separated `session` (durable) from `run` (ephemeral). Net:

- **Kept from it:** eliminate the word `session`; the durable object aligns with
  the server name; per-field schema guidance and crisp descriptions are
  worthwhile.
- **Changed:** the object is named by the **service prefix**, not a `prompt_`
  tool sub-prefix (proposal kept a sub-prefix; we drop it). Input field is
  `user_prompt` + `system_prompt` (proposal chose `text`). Runs become
  **first-class, concurrent, addressable by `run_id`** with a full `run_*`
  namespace incl. `run_list` / `run_get` / `run_fs_*` (proposal kept
  single-flight, `last_run`-only, and `run_*` on just output/cancel). Persistent
  per-session sandbox is **removed** in favor of per-run sandboxes.
