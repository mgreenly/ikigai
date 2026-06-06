# ralph

**Status: draft 1 — design settled for the run-once slice; not yet built.** This
folder holds the design note. The architecture below is the outcome of an
exploration session; the *implementation scope* is deliberately narrowed to a
single end-to-end slice (create a session, set its prompt, run it once, read the
result). Loop/schedule/event-driven triggers and the event-plane work are
designed-for but **deferred** — see Deferred.

## What ralph is

ralph is a ikigenba suite **service** (path-routed under `/srv/ralph/`, loopback,
behind the dashboard's nginx auth — same chassis as every other service) whose
**domain is agentic tasks**. It is a *meta-service*: a deterministic outer MCP
service that lets the owner create, configure, run, and supervise **agent
sessions**, where each session is a custom-built agent driven directly against a
model provider's API, working inside its own persistent sandbox.

ralph is the **deterministic** part. The non-determinism lives *inside* a run
(what the agent decides to do, given a prompt, a toolset, and its sandbox). ralph
owns only *that* a run happens and *when* — storage, lifecycle, the run's
toolset, the sandbox, output collection, and audit.

ralph is **non-interactive background work.** Interactivity lives in the calling
harness (Claude web / Cowork / Claude Code) that connects to ralph over MCP. You
*delegate* a job to ralph and poll for its status and output; you do not chat
with a run.

## Core concepts

- **Session** — a durable, owner-created unit: a prompt, a run config
  (`provider, model, effort, max_tokens, temperature, tools, …`), and its own
  **persistent sandbox folder**. Lives in ralph's DB (and on disk) until deleted.
  The prompt and config are editable after creation.
- **Run** — one episodic execution of a session: a self-contained agent tool-use
  loop, started by a trigger, that runs to a terminal state and exits. A run is
  **conversationally stateless** — no transcript carries between runs. Its only
  durable memory is the files it leaves in the sandbox folder.
- **Sandbox** — a small, persistent, per-session folder the agent works inside
  (see The sandbox). It is the session's memory: a looping or re-run session
  builds up notes/work *as files* and reads them back on the next run.
- **Trigger** — what starts a run. Draft 1 ships exactly one: **`run` (one-shot,
  ad-hoc)**. Loop, schedule, and event-subscribe are designed as additional
  *bindings* on the same session (see Deferred).
- **Run engine** — ralph's own Go agent loop driving a provider API directly
  (stream → on `tool_use`, dispatch tools, append turns, re-invoke → until a
  non-tool stop). Multi-provider behind a provider seam, borrowed from
  `ikigai-cli`. Tools are plain Go functions in a registry, **not** a second MCP
  surface.

## The sandbox

Each session owns one **small, persistent folder**, created empty. Every run of
that session executes *inside* it.

- **Toolset, two classes:**
  - *Sandbox-executing* — `bash, read, write, edit, grep, glob` — operate on the
    folder. `python` is available. bash is **confined to the folder** and has
    **no network access**.
  - *Ralph-mediated* — `websearch, webfetch` (and later `publish_event`) — run in
    ralph's own Go process, which *does* have network, and hand results back in.
- **The agent's only path to the network is the ralph-mediated tools.** bash
  cannot call out. ralph brokers every external interaction — an enforced
  blast-radius boundary, not merely an omitted capability.
- **The folder persists; the execution sandbox is ephemeral per run.** The only
  thing that survives between runs is what was written to disk. Anything else
  (installed packages, env tweaks, background processes) evaporates; the agent
  must materialize what it wants to keep as files.

## State, memory, and concurrency

- **Runs are conversationally stateless; the folder is the memory.** A re-run or
  loop iteration does not *remember* prior runs — it *reads the files* prior runs
  wrote. This is the Ralph pattern, and it reconciles "runs are episodic and
  non-resumable" with "a session accumulates progress."
- **A session is single-flight.** Because all runs share one mutable folder, two
  runs of the same session cannot execute concurrently — they would corrupt each
  other. At most one run is in flight per session. In draft 1, a `run` request
  while a run is active is **rejected as busy** (the overlap policy for
  loop/schedule/event triggers defers with those triggers).
- **Crash/replay is forward-only on the filesystem.** A crash mid-run leaves the
  folder as it was; only DB bookkeeping rolls back. A replay run inherits whatever
  files the crashed run already wrote (a feature — it picks up where it left off),
  so "fresh run" means *fresh conversation*, not *fresh disk*.

## Delivery — pull, two read surfaces

Delivery is **pull-based, on demand, and decoupled from the calling
conversation.** The foreground agent is under **no obligation to surface a result
automatically** — it kicks off the run and moves on. Nothing comes back until
*you* ask ("show me the chat log", "read the report it wrote"), at which point the
agent fetches it over MCP. There is no auto-poll loop and no completion callback.
(The Chat/Cowork connector model is request/response anyway — ralph cannot inject
a message back into the conversation that started a run. Push-to-owner and
MCP-channel paths are deferred.)

A session has a **status**; the harness **collects each run's output**. When asked,
the foreground agent reads **two** distinct things over MCP:

- **The run output log** — `session_output(offset, limit)` — the narrated stream
  of what the agent *did* (latest run, append-only, line-addressable so a long run
  can be tailed incrementally). This is *how it went*.
- **The sandbox files** — `session_fs_list(path)` + `session_fs_read(path,
  offset, limit)` — the agent's actual *work product* (per session, since the
  folder persists). This is *what you got* — for "research fusion," the answer is
  most likely a `report.md` the agent wrote, not its final chat message.

Status model (strawman): **run** = `running → succeeded | failed` (+ `cancelled`);
**session** = `idle | running` in draft 1 (`looping | scheduled | subscribed`
arrive with their triggers).

## Decided (the draft-1 ledger)

1. **Session** = durable object: prompt + run-config + a **persistent sandbox folder**.
2. **Run** = one episodic, conversationally-stateless execution. **The folder is the memory.**
3. **Folder persists; sandbox is ephemeral per run.** Crash → replay over the *same* folder (forward-only on disk).
4. **Sandbox** = empty folder; `bash + read/write/edit/grep/glob`; python; confined to the folder; **no bash network**. Web only via ralph-mediated `websearch`/`webfetch`. Two tool classes.
5. **Non-interactive background work.** Interactivity lives in the calling harness.
6. **Delivery = pull, on demand.** Session has a status; harness collects run output. The foreground agent surfaces **nothing automatically** — only when *you* ask does it read the **run output log** (`session_output`) and/or the **sandbox files** (`session_fs_list` / `session_fs_read`), both by line range.
7. **Triggers are bindings, not a baked-in policy.** Draft 1 implements **`run` (one-shot) only**; loop/schedule/subscribe are deferred bindings on the same session.
8. **A session is single-flight** (shared folder ⇒ runs can't overlap). Draft 1: `run`-while-busy is rejected; richer overlap policy defers with loop/schedule.

## First slice (run-once only)

Goal: the fusion example end-to-end — *create a session → set its prompt → run it
once → poll status → read the output / the file it wrote.*

- **Chassis** cloned from `../ledger` — loopback (**:3004**), `/srv/ralph/` nginx
  fragment, identity gate, PRM doc, `ikigenba_ralph_health`, `bin/*` lifecycle,
  SQLite + migrations, MCP JSON-RPC. Tool prefix → `ikigenba_ralph_`.
- **Session CRUD** — `ikigenba_ralph_session_create` (prompt + config) / `_list` / `_get`
  (incl. status) / `_update` (edit prompt/config — the "edit" tool) / `_delete`.
  Config is a normalized blob, validated at create time; provider keys are
  deployment secrets via the `.envrc` → env pattern, validated **per session**.
- **Sandbox** — a per-session folder under `data/sandboxes/<id>/`. Draft 1 uses
  *basic* confinement to get running; **isolation hardening** (bubblewrap /
  rootless podman `--network none` + bind-mounted volume + python image, the
  no-bash-network enforcement) is a **known gap, deferred**, and likely a
  ikigenba **platform** concern since it owns the box.
- **Run engine** — borrow the `ikigai-cli` provider seam (Anthropic reference
  adapter) + agent loop + tool registry (`bash/read/write/edit/grep/glob` +
  `websearch/webfetch`). `ikigenba_ralph_session_run` (one-shot) spawns a run, collects
  output to a run record.
- **Read surface** — `ikigenba_ralph_session_output`, `ikigenba_ralph_session_fs_list`,
  `ikigenba_ralph_session_fs_read` (status/usage ride on `ikigenba_ralph_session_get.last_run`). See
  the full MCP tool surface below.

## MCP tool surface (draft 1 — 11 tools, run-once only)

The complete surface for the slice. Deliberately narrow; reads are **session-scoped
to the latest run** (no `run_id` in the surface — browsing prior runs is deferred);
the sandbox is **read-only** from the foreground; the toolset is **fixed**.

**Identity (chassis)**

- `ikigenba_ralph_health` — prove the connector → OAuth → service chain; no side effects.
  *in:* none · *out:* `{status, version, service, owner_email, client_id, details}`.

**Session lifecycle**

- `ikigenba_ralph_session_create` — create a durable session + its empty sandbox folder.
  *in:* `{name?, prompt, system_prompt?, config:{provider, model, effort?, max_tokens?, temperature?}}` ·
  *out:* `{session_id, status:"idle"}` ·
  *errors:* unknown provider/model; **missing provider key** (validated per-session); bad config.
- `ikigenba_ralph_session_list` — enumerate the owner's sessions.
  *in:* none · *out:* `[{session_id, name, status, created_at, last_run:{status, ended_at}}]`.
- `ikigenba_ralph_session_get` — full detail of one session.
  *in:* `{session_id}` · *out:* `{prompt, system_prompt, config, status, created_at, updated_at, last_run:{status, started_at, ended_at, usage, error?}}`.
- `ikigenba_ralph_session_update` — the "edit" tool: change prompt/system_prompt/config/name.
  *in:* `{session_id, name?, prompt?, system_prompt?, config?}` · *out:* updated session ·
  *errors:* **rejected while `running`**.
- `ikigenba_ralph_session_delete` — remove the session, its folder, and its run history.
  *in:* `{session_id}` · *out:* `{ok}` · *errors:* **rejected while `running`** (cancel first).

**Run (one-shot)**

- `ikigenba_ralph_session_run` — start one run. **Async** — returns immediately, does not block.
  *in:* `{session_id}` · *out:* `{status:"running", started_at}` ·
  *errors:* **`busy`** if a run is already in flight (single-flight invariant) ·
  *side effect:* spawns the run engine over the session's folder.
- `ikigenba_ralph_session_cancel` — terminate the in-flight run; the **folder is kept**.
  *in:* `{session_id}` · *out:* `{status:"cancelled"}` · *errors:* no run in flight.

**Read surfaces (pull, on demand)**

- `ikigenba_ralph_session_output` — read the **latest run's output log** by line range (the narrated
  "what the agent did"); tailable while running.
  *in:* `{session_id, offset?=0, limit?=200}` · *out:* `{lines[], total_lines, run_status}`.
- `ikigenba_ralph_session_fs_list` — list the sandbox folder.
  *in:* `{session_id, path?="."}` · *out:* `[{name, type:"file"|"dir", size, modified}]` ·
  *errors:* path escaping the folder is rejected.
- `ikigenba_ralph_session_fs_read` — read a sandbox file by line range (the deliverable, e.g. `report.md`).
  *in:* `{session_id, path, offset?=0, limit?=200}` · *out:* `{lines[], total_lines, truncated}` ·
  *errors:* path-escape; not-a-file.

**Deferred from the surface:** `run_id`-addressed reads + run-history browsing;
`fs_write` / seeding input files; per-session tool selection; the loop/schedule/
subscribe trigger verbs.

## Deferred (explicitly later)

- **Triggers beyond run-once:** loop, schedule (interval/cron), event-subscribe —
  plus their overlap policy (lean: *skip* timers, *queue-one-deep* events).
- **Event plane:** `publish_event` tool; **multi-tenant `eventplane`** (`task_id`
  discriminator on `outbox`; `feed_offset` keyed `(task_id, source)`); the §10
  completion-commit refinement and the at-least-once cursor guarantees (these only
  bite once runs are event-triggered).
- **Delivery beyond pull:** push-to-owner notifications (dashboard push); MCP
  channels / talking back to the calling conversation.
- **Sandbox isolation hardening** (above).
- **Runtime feed discovery / registry** for dynamically-born task feeds; source
  naming & anti-impersonation; agent-defined event types vs. the interop contract
  (the README's original open questions — they belong to the event-plane phase).

## Reuses vs. new

**Reuses (clone/borrow):**
- ikigenba **chassis** from `../ledger` (loopback bind, nginx fragment, identity
  gate, PRM doc, `ikigenba_<svc>_health`, `bin/*` lifecycle, SQLite + migrations, MCP
  JSON-RPC).
- **run engine** architecture from `ikigai-cli` (provider seam + neutral wire
  types + tool registry + the iteration loop).
- **event plane** library `../eventplane` (producer `outbox` + consumer engine) —
  *only when the deferred event work lands; it needs multi-tenant changes first.*

**New (what ralph forces on the system):**
- **The sandbox** — a persistent, confined, per-session work folder with a
  network-isolated bash + a curated toolset. No other service has one; the
  isolation enforcement is genuinely new infra (and likely platform-level).
- **Multi-tenant `eventplane`** (deferred) and **runtime feed discovery**
  (deferred) — see Deferred.

## Sources (read before coding)

**Platform (sibling `metaspot` repo — authoritative; on conflict it wins):**
- `../../metaspot/AGENTS.md` — platform spec (Service layer, launcher, SSM
  app-config secrets, `bin/*` lifecycle).
- `../../metaspot/docs/path-routing-architecture.md` — server topology + the nginx
  `auth_request` auth contract every service lives under.
- `../../metaspot/docs/connector-and-install.md` — suite plugin + client install
  layer (and why connector delivery is request/response — the constraint behind
  pull-based delivery).

**Suite (this repo):**
- `../README.md` — suite overview (two planes, event model).
- `../docs/event-protocol.md` — **normative** event-plane wire contract (for the
  deferred event phase: §3 addressing, §7–§12).
- `../docs/event-plane-decisions.md`, `../docs/event-plane-technical-overview.md`.

**Exemplars (this repo):**
- `../ledger/` — the health-only **chassis skeleton** to clone from.
- `../crm/` — event-plane **producer** (outbox wiring) — *for the deferred phase.*
- `../notify/` — event-plane **consumer** — *for the deferred phase.*
- `../eventplane/` — the shared library to make multi-tenant — *deferred.*

**Run-engine reference (external, not in this repo):**
- `~/projects/ikigai-cli` (Go) — api-direct, multi-provider agent CLI; the
  settled architecture to borrow ralph's run engine from. Key files:
  `app-root/internal/provider/provider.go` (the seam: `Client.Stream`, neutral
  `Request`/`Block`/`Event`), `app-root/internal/provider/{anthropic,openai,google}/`,
  `app-root/internal/agent/loop.go` (the iteration loop), `app-root/internal/tools/`
  (registry + dispatch), `app-root/internal/model/registry.go`,
  `app-root/internal/scope/` (effort-native mapping).
- `~/projects/ralph-loops` (Go) — an outer **loop/orchestration** harness; useful
  for the deferred run-supervision/lifecycle patterns.
