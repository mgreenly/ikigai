# prompts

**Status: implemented — the async-runs + multi-source-trigger redesign is in the
tree.** This folder holds the design note. The model below is the outcome of the
redesign settled in `REDESIGN-DECISIONS.md` and built across the eight phases of
`REDESIGN-PLAN.md` (both kept as the durable design record). The architecture is
described in `ARCHITECTURE.md` (the *how the code is shaped*); this is the *why*
and the concept ledger.

## What prompts is

prompts is a ikigenba suite **service** (path-routed under `/srv/prompts/`,
loopback, behind the dashboard's nginx auth — same chassis as every other
service) whose **domain is agentic tasks**. It is a *meta-service*: a
deterministic outer MCP service that lets the owner create, configure, run, and
supervise **agent runs**, where each run is a custom-built agent driven directly
against a model provider's API, working inside its own per-run sandbox.

prompts is the **deterministic** part. The non-determinism lives *inside* a run
(what the agent decides to do, given a prompt, a toolset, and its sandbox).
prompts owns only *that* a run happens and *when* — storage, lifecycle, the run's
toolset, the per-run sandbox, output collection, and audit.

prompts is **non-interactive background work.** Interactivity lives in the
calling harness (Claude web / Cowork / Claude Code) that connects to prompts over
MCP. You *delegate* a job to prompts and poll for its status and output; you do
not chat with a run.

prompts and the sibling **`scripts`** service are **two sides of one coin** — the
same surface, lifecycle, and trigger model, differing only in the input field
(`user_prompt` vs `body`) and the executor (a non-deterministic agent loop vs a
deterministic `python3`/`bash`). They are symmetric **event-plane peers**: each
is both a producer and a multi-upstream consumer, and each can trigger off the
other.

## Core concepts

- **Prompt** — a durable, owner-created **definition**: a `user_prompt`, an
  optional `system_prompt`, a run config (`provider, model, effort?, max_tokens?,
  temperature?`), an optional name, and zero or more **triggers**. A prompt holds
  **no status** and **no sandbox of its own** — it is just the reusable
  definition. Editable after creation; lives in prompts' DB until deleted.
- **Run** — one **first-class, addressable** execution of a prompt, keyed by its
  own `run_id`. A run is **conversationally stateless** (no transcript carries
  between runs) and owns its own per-run sandbox. Runs are **fully concurrent**:
  any number of runs of the same prompt may execute at once. A run records the
  exact inputs it executed (frozen to disk at spawn) and survives the deletion of
  its parent prompt.
- **Sandbox** — a **per-run** folder (`runs/<run_id>/sandbox/`) the agent works
  inside, created **empty every run** (no seeding, no carry-over). It is
  persisted with the run after it finishes, so the `run_fs_*` readers can read the
  work product later.
- **Trigger** — a `(prompt, source, event_filter)` binding that starts a run when
  a matching upstream event fires. A prompt may declare **many** triggers across
  **many** upstreams. There is no schedule baked into prompts itself — scheduling
  comes from the `cron` upstream like any other event source.
- **Run engine** — prompts' own Go agent loop driving a provider API directly
  (stream → on `tool_use`, dispatch tools, append turns, re-invoke → until a
  non-tool stop). Multi-provider behind a provider seam. Tools are plain Go
  functions in a registry, **not** a second MCP surface.

## Materialize → exec → persist

The governing rule for a run's inputs:

1. **Materialize.** At spawn, the service freezes the prompt's *current*
   definition into `runs/<run_id>/input/`: `user_prompt.txt`, `system_prompt.txt`
   (empty file if none), and `config.json` (the resolved config). This `input/`
   tree **is the record of exactly what executed**.
2. **Exec.** The runner reads its execution inputs **from `input/` on disk** —
   never from the DB or the live `Prompt` object — and runs the agent loop with a
   fresh `runs/<run_id>/sandbox/`, streaming stream-json to
   `runs/<run_id>/output.jsonl`.
3. **Persist.** When the run reaches a terminal state, the run row is written and
   the outcome event emitted in one transaction. `input/`, `output.jsonl`, and
   `sandbox/` all persist with the run forever.

The consequence: **editing or deleting a prompt mid-run cannot change what an
in-flight run executes.** Every run is a self-contained, replayable record.

## State, memory, and concurrency

- **Runs are conversationally stateless; the per-run sandbox is the only
  workspace.** Because each run gets its own empty sandbox, there is **no
  single-flight gate** — two runs of the same prompt cannot corrupt each other,
  so any number may execute concurrently. A `run` request is **always accepted**
  and returns a `run_id` immediately.
- **No cross-run carry-over.** A run's sandbox starts empty (explicit non-goal:
  no seeding from a prior run). If a workflow needs accumulated state, that is a
  fast-follow built on the event plane (a prompt firing on its own
  `run.succeeded`), not in-sandbox persistence.
- **Crash recovery touches runs only.** On boot, runs left `running` by a crashed
  process are swept to `failed`. Prompts have no status to reset.

## Triggers — multi-source fan-in (symmetric with scripts)

A prompt reacts to upstream events by declaring **triggers**. Each trigger is one
`(source, event_filter)` binding; a prompt may hold many. prompts subscribes to
**N upstreams** and decides per event which prompts to fire.

Day-one upstream set (the five external producers; self-consumption is a
flagged fast-follow):

| source | events it publishes (the `set_trigger` registry, A12) |
|---|---|
| `cron`    | any `cron.*` (dynamic) |
| `crm`     | `contact.created` · `contact.updated` · `contact.tagged` · `contact.untagged` |
| `ledger`  | `transaction.recorded` |
| `dropbox` | `file.created` · `file.modified` · `file.deleted` |
| `scripts` | `scripts.succeeded` · `scripts.failed` |
| `prompts` | `run.succeeded` · `run.failed` — **self-chaining; fast-follow TODO** |

`set_trigger` validates `(source, event_filter)` against this static
known-producer registry and rejects an unknown source or an `event_filter` the
producer cannot publish. Firing is **fire-and-forget**: each matching event spawns
a run on its own goroutine; the consumer cursor always advances; there is no
staleness guard and no retry (the old `max_staleness_secs` / `max_attempts` knobs
are gone).

prompts is itself a **producer**: every terminal run emits `run.succeeded` or
`run.failed` (cancelled emits nothing) on its `/feed`. Because `prompts` is listed
in its own registry, the **self-consumption fast-follow** is a one-line addition —
a sixth consumer loop pointed at the local `:3004/feed` so one prompt can fire on
another prompt's outcome. Day-one wires the five external upstreams only.

## Tombstone delete

`delete` is a **tombstone**: it removes the prompt row and the prompt's triggers,
and **nothing else**. The prompt's runs, their `input/` / `output.jsonl` /
`sandbox/` directories, and the outbox all survive. A run is addressable by its
own `run_id` and owner-scoped via the run's denormalized `owner_email`, so
`run_get` / `run_output` / `run_fs_*` keep working after the parent prompt is
gone. Delete is **always allowed** (no "rejected while running").

## Delivery — pull, two read surfaces

Delivery is **pull-based, on demand, and decoupled from the calling
conversation.** The foreground agent kicks off a run and moves on; nothing comes
back until *you* ask, at which point the agent reads over MCP. There is no
auto-poll loop and no completion callback into the originating conversation.

The two read surfaces, both keyed by `run_id`:

- **The run output log** — `run_output(run_id, offset, limit)` — the narrated
  stream-json of what the agent *did* (append-only, line-addressable, tailable
  while running). This is *how it went*.
- **The run's sandbox files** — `run_fs_list(run_id, path)` +
  `run_fs_read(run_id, path, offset, limit)` — the agent's actual *work product*.
  This is *what you got* — typically a `report.md` the agent wrote.

Status rides on the run: a run is `running → succeeded | failed` (+ `cancelled`).
`get`/`list` return a prompt's `running_count` and `last_run` for a quick
overview.

## MCP tool surface (16 tools)

Tool names are bare verbs (no service prefix; see
`docs/adr-mcp-tool-bare-names.md`). **Airtight key rule:** keyed by `prompt_id` → bare
verb; keyed by `run_id` → `run_*`. The wire field for the user-role prompt is
**`user_prompt`**.

**Identity / discovery**

- `health` — auth-chain proof; `{}` → identity envelope.
- `describe` — full overview of the prompt-vs-run model and the
  create→run→poll→read lifecycle; `{}`.

**Prompt lifecycle** (keyed by `prompt_id`)

- `create` — `{user_prompt**, config**, name?, system_prompt?, triggers?}` →
  `{prompt_id, ...}`. `triggers` is an optional array of `{source, event_filter}`
  applied at create time (the whole create is rejected if any is invalid).
- `list` — `{}` → prompts with `running_count` + `last_run`.
- `get` — `{prompt_id**}` → full detail incl. `running_count` + `last_run`.
- `update` — `{prompt_id**, user_prompt?, system_prompt?, config?, name?}` —
  **always allowed**.
- `delete` — `{prompt_id**}` — **tombstone**; runs survive. Always allowed.
- `set_trigger` — `{prompt_id**, source**, event_filter**}` — attach one binding;
  call repeatedly to attach several.
- `clear_trigger` — `{prompt_id**, source**, event_filter**}` — remove one
  binding.

**Run** (`run` is keyed by `prompt_id`; the rest by `run_id`)

- `run` — `{prompt_id**}` → `{run_id, status:"running", started_at}`. **Always
  accepted** (full concurrency).
- `run_list` — `{prompt_id**}` → the prompt's runs, newest first.
- `run_get` — `{run_id**}` → one run's detail (works post-tombstone).
- `run_output` — `{run_id**, offset?, limit?}` → the run's stream-json log.
- `run_cancel` — `{run_id**}` — idempotent.
- `run_fs_list` — `{run_id**, path?}` — list the run's sandbox; path-escape
  rejected.
- `run_fs_read` — `{run_id**, path**, offset?, limit?}` — read a sandbox file;
  path-escape / not-a-file rejected.

## The sandbox

Each **run** owns one empty folder, created fresh, that the agent works inside;
it is persisted with the run afterward.

- **Toolset, two classes:**
  - *Sandbox-executing* — `bash, read, write, edit, grep, glob` — operate on the
    folder. `python3` is available via `bash`. bash is **confined to the folder**.
  - *Agent-mediated* — network tools run in prompts' own Go process and hand
    results back in.
- **Confinement** in this build is Go-level path checks + bash `cmd.Dir`. Real OS
  / network isolation (bubblewrap / rootless podman `--network none`) is a known
  gap and likely a ikigenba **platform** concern.

## Config & secrets

- **Public config** (`PROMPTS_*` env): port `3004`, resource id, auth server, db
  path, log level, `PROMPTS_RUN_TTL` (the run backstop), and one
  `PROMPTS_<SRC>_FEED_URL` + `PROMPTS_<SRC>_FROM` (default `tail`) per upstream
  (`CRON`, `CRM`, `LEDGER`, `DROPBOX`, `SCRIPTS`). The event plane bypasses nginx,
  so the dev fallbacks are direct loopback addresses.
- **Secret**: `ANTHROPIC_API_KEY` — the only one. On the box: SSM app-config →
  launcher → process env. Locally: `~/.secrets/ANTHROPIC_API_KEY` → `.envrc` →
  env. Read once via `os.Getenv`; never on disk, never logged.

## Deferred (explicitly later)

- **Self-consumption** (prompts firing on its own `run.succeeded` / `run.failed`)
  — a one-line fast-follow (the sixth consumer loop + the `prompts` CONSUMES
  entry).
- **Retention / GC** of run directories.
- **Sandbox seeding / cross-run carry-over** — explicit non-goal.
- **Sandbox isolation hardening** (bwrap/podman, network isolation) — likely a
  platform concern.
- **Push-to-owner delivery** and talking back into the calling conversation.

## Sources (read before coding)

**Suite (this repo):**
- `../README.md` — suite overview (two planes, event model).
- `../docs/event-protocol.md` — **normative** event-plane wire contract.
- `../docs/event-plane-decisions.md`, `../docs/event-plane-technical-overview.md`.
- `../scripts/` — the symmetric sibling (same surface + trigger model).

**Exemplars (this repo):**
- `../crm/`, `../ledger/`, `../dropbox/` — event-plane **producers** (upstreams).
- `../notify/` — the **multi-cursor consumer** pattern prompts' cmd mirrors.
- `../appkit/`, `../eventplane/`, `../agentkit/` — the shared chassis / libraries.

**Design record (this folder):**
- `REDESIGN-DECISIONS.md` — the ratified *why* and external scheme.
- `REDESIGN-PLAN.md` — the frozen Part A contracts + the eight implementing
  phases. `PLAN.md` is the **superseded** run-once build plan, kept for history.
</content>
</invoke>
