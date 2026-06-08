# prompts — architecture

**Status:** implemented. This document describes the prompts service as it stands
after the async-runs + multi-source-trigger redesign (`REDESIGN-PLAN.md` Part A,
the frozen contracts; `REDESIGN-DECISIONS.md`, the *why*). It is the companion to
`README.md` (the *why* / concept ledger); this is *how the code is shaped*.

The unit of work is the **run** — first-class, addressable by `run_id`, fully
concurrent, and self-contained on disk. A **prompt** is just the reusable
definition a run is materialized from. There is **no session, no prompt status,
and no single-flight gate**.

---

## 1. Module layout

```
prompts/
├── cmd/prompts/main.go            # composition root: appkit Spec, consumer workers, producer outbox
├── go.mod   (module prompts)
├── Makefile · bin/* · etc/*       # PORT=3004, MOUNT=/srv/prompts/, PROMPTS_* env
└── internal/
    ├── db/                        # SQLite open + embedded migrations runner
    │     └── migrations/          # 002_prompts.sql, 003_prompt_triggers.sql (+ lib 004/005)
    ├── ids/   logging/            # chassis: ULIDs, slog JSON
    ├── mcp/                       # 16-tool MCP surface (tools.go, describe.go) → Service
    ├── prompt/        # THE DOMAIN (package `prompt`)
    │     ├── model.go   #   Prompt, Run, PromptDetail, Config, Trigger, TriggerSpec, run-status consts
    │     ├── store.go   #   SQLite queries: prompts + runs (run_id keyed)
    │     ├── trigger.go #   composite-key (prompt_id, source, event_filter) trigger store + registry
    │     ├── service.go #   CRUD + Run/RunByEvent + run_* readers + trigger validation
    │     └── outcome.go #   run.succeeded / run.failed outbox events
    ├── sandbox/       # per-RUN folder: Create/Root/List/Read/Remove + path confinement
    ├── runner/        # async run lifecycle: Spawn(run)/Cancel(runID)/Recover; reads input/ from disk
    ├── consume/       # multi-upstream fan-in: Subscriptions(sources) + Handler
    └── engine/        # the agent loop: provider seam, tool registry, model registry, stream-json wire
```

The agent **run engine** under `internal/engine/` (provider seam + tool-use loop
+ tool registry + model registry + stream-json wire) is the non-deterministic
half; everything else is deterministic orchestration.

---

## 2. On-disk layout (A2)

`dataDir = dirname(PROMPTS_DB_PATH)/data`. The on-disk unit is the **run
directory**, keyed by `run_id`. There is **no** persistent per-prompt sandbox.

```
data/
  prompts.db
  runs/
    <run_id>/
      input/
        user_prompt.txt       # the pinned user_prompt for THIS run
        system_prompt.txt     # the pinned system_prompt (empty file if none)
        config.json           # the pinned {provider, model, effort?, max_tokens?, temperature?}
        event.json            # the pinned triggering event (event-triggered runs ONLY; absent on manual runs)
      output.jsonl            # append-only stream-json run log
      sandbox/                # the agent's per-run workspace (produced files)
```

- `input/` is written by the service **at spawn** from the prompt's current
  definition, then persisted with the run forever — it *is* the record of exactly
  what executed.
- `event.json` is written by the service **at spawn** for **event-triggered runs
  only** — the pinned triggering event, envelope `{source, type, event_id,
  payload}`. It is **absent on manual runs**.
- The runner reads its execution inputs from `input/` — **never** from the DB or
  the live `Prompt` object.
- `sandbox/` starts **empty every run** (no seeding / carry-over) and is persisted
  after the run for the `run_fs_*` readers.
- Retention / GC of run directories is deferred.

---

## 3. Chassis (appkit)

prompts is built on the shared `appkit` chassis (consumed via a committed
`replace`). `cmd/prompts/main.go` declares an `appkit.Spec` and appkit owns the
verb dispatcher, the loopback server behind the nginx identity gate
(`X-Owner-Email` / `X-Client-Id`, trusted blindly), the migration runner, the
manifest emit, the `/feed` outbox, and the consumer engine. prompts adds:

- the `prompt.Service` (store + sandbox + runner), injected into the MCP handler;
- a **boot-time crash-recovery sweep** (§6) that runs after migrate, before
  serving;
- the **producer** wiring (`prompt.Events`, the outbox injected onto the store so
  the runner's terminal write emits the outcome event on the same tx);
- **five consumer workers**, one per upstream (the `notify` multi-cursor
  pattern), each reading `PROMPTS_<SRC>_FEED_URL` with its own cursor.

`Spec.Consumes = {cron, crm, ledger, dropbox, scripts}`, so appkit emits
`CONSUMES=cron,crm,ledger,dropbox,scripts` into `etc/manifest.env` at deploy.

---

## 4. Data model

### `002_prompts.sql`

`prompts` is the definition (no `status` column); `runs` is first-class and
`run_id`-keyed with **no FK / no cascade** to `prompts`, denormalized
`owner_email` / `prompt_name`, and the trigger context.

```sql
CREATE TABLE prompts (
    id, owner_email, name?, user_prompt, system_prompt?, config_json,
    created_at, updated_at
);

CREATE TABLE runs (
    id,                                  -- ULID; the run, addressable
    prompt_id,                           -- NO FK; may dangle after tombstone delete
    owner_email,                         -- denormalized: run stays owner-addressable
    prompt_name?,                        -- captured at run start, for the outcome event
    status,                              -- running | succeeded | failed | cancelled
    started_at, ended_at?, usage_json?, error?,
    trigger_source?,                     -- '' for manual; else cron|crm|ledger|dropbox|scripts|prompts
    trigger_type?,                       -- the fired event type, e.g. "file.created"
    trigger_event_id?,                   -- the upstream event id that fired this run
    log_path                             -- data/runs/<run_id>/output.jsonl
);
CREATE INDEX idx_runs_prompt ON runs(prompt_id, started_at);
CREATE INDEX idx_runs_status ON runs(status);
```

### `003_prompt_triggers.sql`

A composite-key, multi-source table — N triggers per prompt across N sources, no
cascade. The old cron-only knobs (`max_staleness_secs`, `max_attempts`) are gone.

```sql
CREATE TABLE prompt_triggers (
    prompt_id, source, event_filter, created_at,
    PRIMARY KEY (prompt_id, source, event_filter)
);
CREATE INDEX idx_prompt_triggers_lookup ON prompt_triggers(source, event_filter);
```

Migrations `004_feed_offset.sql` / `005_outbox.sql` are **library-owned** and not
touched.

---

## 5. The domain (`internal/prompt`)

`Service` is the only thing the MCP handler talks to and the only thing that
mutates prompt/run state.

### Prompt lifecycle

- **Create** — validate config (`model.Resolve`; provider is `anthropic`;
  `ANTHROPIC_API_KEY` present), insert the prompt row. **No sandbox is created.**
  Optional `Triggers []TriggerSpec` are applied via `SetTrigger` after insert
  (same validation; the whole create is rejected if any binding is invalid).
- **List / Get** — owner-scoped reads returning `PromptDetail` =
  prompt + `running_count` (`COUNT(runs WHERE status='running')`) + `last_run`.
- **Update** — edit `user_prompt` / `system_prompt` / `config` / `name`,
  re-validating config. **Always allowed** (no `ErrRunning`).
- **Delete** — **tombstone**: removes the prompt row and its triggers
  (`DeleteTriggers`), nothing else. Runs, run directories, and the outbox survive.
  **Always allowed.**

### Run lifecycle — materialize → exec → persist

- **Run(owner, promptID)** — **always accepted**. Generates a `run_id`,
  materializes `runs/<run_id>/input/` (`user_prompt.txt`, `system_prompt.txt`,
  `config.json`) from the prompt's current definition, creates the empty per-run
  sandbox, inserts the run row (`running`, `log_path`, `owner_email`,
  `prompt_name`), and calls `runner.Spawn(run)`. Returns
  `{run_id, status:"running", started_at}` immediately. There is **no
  single-flight gate** — concurrent runs of one prompt are safe (each has its own
  sandbox).
- **RunByEvent(promptID, source, evType, eventID, payload)** — the event path
  (unscoped; owner resolved via `GetPromptByID`). Identical, plus it records the
  trigger context (`trigger_source`, `trigger_type`, `trigger_event_id`) on the
  run row **and** pins the triggering event to `runs/<run_id>/input/event.json`
  (envelope `{source, type, event_id, payload}`) so the runner can deliver it to
  the agent as the second user-message block (§6). Manual `Run` writes no
  `event.json`.

### Run reads (keyed by `run_id`, owner via the run's `owner_email`)

`RunList(owner, promptID)`, `RunGet`, `RunOutput`, `RunCancel`, `RunFsList`,
`RunFsRead`. Owner is checked against the **run's** denormalized `owner_email`
(not the prompt's), so every reader works **after the parent prompt is
tombstoned**. `RunCancel` calls `runner.Cancel(runID)` and is idempotent.

### Triggers

`SetTrigger(owner, promptID, source, eventFilter)` validates `source ∈
TriggerSources()` and `event_filter` plausible for that producer (the §8
registry) with `ErrValidation`, then upserts the composite-key binding.
`ClearTrigger` removes one. `PromptsForEvent(source, evType)` → prompt ids is the
**consumer fan-out** (not owner-scoped).

### Store note — `FinishRun`

`FinishRun(FinishRunInput{RunID, Status, EndedAt, UsageJSON, ErrMsg})` writes the
run's terminal row **and** emits the outcome event in **one transaction**. The
outcome-event fields (`prompt_id`, `prompt_name`, `run_id`, trigger context) are
read **from the run row inside the tx**, so the runner does not thread them in.
`SweepRunning` marks orphaned `running` runs `failed` — **runs only**, no prompt
touch (prompts have no status).

---

## 6. Runner (`internal/runner`)

```go
type Runner interface {
    Spawn(run Run)        // no Prompt param — runner reads input/ from disk
    Cancel(runID string) bool
}
// plus Recover(ctx) (int, error) on the concrete type
```

- **`Spawn(run)`** starts a goroutine and returns. It derives
  `runDir = runsDir/<run.ID>`, reads `input/config.json` (→ `model.Resolve`,
  effort, max_tokens), `input/user_prompt.txt`, `input/system_prompt.txt`, and
  the **optional** `input/event.json` (present only on event-triggered runs,
  absent on manual runs), and runs the agent loop with
  `sandboxRoot = runDir/sandbox`, streaming stream-json into
  `runDir/output.jsonl`. It references **no** `Prompt` field for execution inputs.
  The agent's single user message carries the verbatim `user_prompt` as its first
  `TextBlock`; when `event.json` is present a **second** block (a short preamble +
  the event JSON) is appended, so the run sees the event body that fired it. The
  sandbox itself is **not** seeded — delivery is via the prompt message only.
- A `context.WithTimeout(parent, PROMPTS_RUN_TTL)` is the runaway backstop. The
  `cancels` and `userCancelled` maps are keyed by **`run_id`**.
- **`Cancel(runID)`** signals that run (cancelled, not failed); idempotent.
- On exit the goroutine calls `store.FinishRun` (terminal run write + outcome
  event, one tx). **No prompt-status flip** — there is none.
- **`Recover(ctx)`** is the boot-time crash sweep: `SweepRunning` marks orphaned
  `running` runs `failed` before the server begins listening.

---

## 7. Consume — multi-upstream fan-in (`internal/consume`)

prompts subscribes to **N upstreams**, symmetric with scripts. `cmd` wires one
consumer worker per upstream (the `notify` multi-cursor pattern).

```go
func Subscriptions(sources []string) []consumer.Subscription   // one per upstream, Filter:"*"
func Handler(fire FireFunc, lookup LookupFunc, source string, logger *slog.Logger) consumer.Handler
```

Per event the handler calls `PromptsForEvent(source, ev.Type)` and fires
`RunByEvent` for each matching prompt on its **own goroutine** (unbounded,
non-blocking, **fire-and-forget**). It returns `nil` for any matched event (never
stalls the feed) and `ErrSkip` only on a structurally-malformed envelope (poison
→ log loud + advance). The cursor advances for every event. There is **no
staleness guard and no retry** — the old single `cron.*` subscription,
`fireWithRetry`, and the staleness/attempt knobs are gone.

---

## 8. Triggers & the event plane

### Consumed upstreams (`PROMPTS_<SRC>_FEED_URL` / `_FROM`)

| env var | dev fallback | source |
|---|---|---|
| `PROMPTS_CRON_FEED_URL`    | `http://127.0.0.1:3007/feed` | cron |
| `PROMPTS_CRM_FEED_URL`     | `http://127.0.0.1:3001/feed` | crm |
| `PROMPTS_LEDGER_FEED_URL`  | `http://127.0.0.1:3002/feed` | ledger |
| `PROMPTS_DROPBOX_FEED_URL` | `http://127.0.0.1:3005/feed` | dropbox |
| `PROMPTS_SCRIPTS_FEED_URL` | `http://127.0.0.1:3009/feed` | scripts |

`PROMPTS_<SRC>_FROM` defaults to `tail`. The event plane bypasses nginx, so the
dev fallbacks are direct loopback addresses.

### Known-producer registry (`SetTrigger` validation, A12)

```
cron    -> dynamic; any event_filter matching "cron.*"
crm     -> contact.created | contact.updated | contact.tagged | contact.untagged
ledger  -> transaction.recorded
dropbox -> file.created | file.modified | file.deleted
scripts -> scripts.succeeded | scripts.failed
prompts -> run.succeeded | run.failed     (self-chaining; prompts' OWN feed — fast-follow)
```

This is the mirror image of scripts' registry (which lists prompts as an
upstream): the two services are **symmetric event-plane peers**.

### Producer — outcome events (`outcome.go`)

Every terminal run emits one of two static types on prompts' own `/feed`
(`cancelled` emits nothing):

- `run.succeeded`
- `run.failed` (same payload + an `error` string)

Payload: `{prompt_id, prompt_name, run_id, trigger_source, trigger_type,
trigger_event_id, error?}` — the trigger fields are empty for a manual run.

### Self-consumption fast-follow

`prompts` is in its own registry but is **not** wired day-one. Adding it is a
one-line change: a sixth consumer worker pointed at the local `:3004/feed`
(`PROMPTS_PROMPTS_FEED_URL`) plus the `prompts` CONSUMES entry — letting one
prompt fire on another prompt's `run.succeeded` / `run.failed`. It is a flagged
TODO in `cmd/prompts/main.go`.

---

## 9. Sandbox (`internal/sandbox`)

`Manager` is rooted at `runsDir`; `Create / Root / List / Read / Remove` are keyed
by **`run_id`**, resolving to `runsDir/<run_id>/sandbox`.

1. **Read surface for MCP** — `List(run_id, path)` and `Read(run_id, path, offset,
   limit)`, both rejecting any path that escapes the run's sandbox (clean + `..`
   checks against the resolved root). These back `run_fs_list` / `run_fs_read`.
2. **Confinement for the engine toolset** — `bash` runs with
   `cmd.Dir = sandboxRoot`; `read`/`write`/`edit` validate the path resolves under
   `sandboxRoot`; `glob`/`grep` use `sandboxRoot` as the search root.

Confinement is Go-level path checks + bash `cmd.Dir` only — **no** network
isolation, **no** OS sandbox. Real isolation (bubblewrap / rootless podman
`--network none`) is a known, deferred gap, likely a ikigenba **platform**
concern. The agent reaches `python3` through `bash`, so the only runtime
requirement is a `python3` interpreter on the box's `PATH`.

---

## 10. MCP tool surface (16 tools)

Tool names are bare verbs (no service prefix; see
`docs/adr-mcp-tool-bare-names.md`). **Airtight key rule:** keyed by `prompt_id` → bare
verb; keyed by `run_id` → `run_*`. The wire field for the user-role prompt is
**`user_prompt`**. There is no `session_` subprefix.

| tool | key | input (** required) | service |
|---|---|---|---|
| `health` | — | `{}` | (chassis) |
| `describe` | — | `{}` | (overview) |
| `create` | → `prompt_id` | `{user_prompt**, config**, name, system_prompt, triggers}` | `Create` |
| `list` | — | `{}` | `List` |
| `get` | `prompt_id` | `{prompt_id**}` | `Get` |
| `update` | `prompt_id` | `{prompt_id**, user_prompt, system_prompt, config, name}` | `Update` |
| `delete` | `prompt_id` | `{prompt_id**}` | `Delete` |
| `set_trigger` | `prompt_id` | `{prompt_id**, source**, event_filter**}` | `SetTrigger` |
| `clear_trigger` | `prompt_id` | `{prompt_id**, source**, event_filter**}` | `ClearTrigger` |
| `run` | `prompt_id` → `run_id` | `{prompt_id**}` | `Run` |
| `run_list` | `prompt_id` | `{prompt_id**}` | `RunList` |
| `run_get` | `run_id` | `{run_id**}` | `RunGet` |
| `run_output` | `run_id` | `{run_id**, offset, limit}` | `RunOutput` |
| `run_cancel` | `run_id` | `{run_id**}` | `RunCancel` |
| `run_fs_list` | `run_id` | `{run_id**, path}` | `RunFsList` |
| `run_fs_read` | `run_id` | `{run_id**, path**, offset, limit}` | `RunFsRead` |

`create` returns `{prompt_id, ...}`; `run` returns `{run_id, status:"running",
started_at}`. `create`'s optional `triggers` is an array of
`{source, event_filter}` applied at create time. `set_trigger` may be called
repeatedly to attach several bindings.

---

## 11. End-to-end flow

1. `create {user_prompt, config, triggers?}` → validate → insert prompt row →
   apply inline triggers → `{prompt_id}`. **No sandbox yet.**
2. `run {prompt_id}` → generate `run_id` → materialize `runs/<run_id>/input/` +
   empty `sandbox/` → insert run row (`running`) → `runner.Spawn(run)` →
   `{run_id, "running", started_at}`. A **second** `run` of the same prompt runs
   concurrently in its own isolated sandbox.
3. goroutine: read `input/`, drive the agent loop, stream into
   `runs/<run_id>/output.jsonl`; on finish → `FinishRun` writes the terminal run
   row + emits `run.succeeded` / `run.failed` in one tx. On an **event-triggered**
   run the triggering event was pinned to `input/event.json` at spawn and is
   delivered to the agent as the **second block** of the run's single user message
   (the first block is the verbatim `user_prompt`); a manual run gets only the
   first block.
4. `run_get {run_id}` polls to terminal; `run_output {run_id, offset, limit}`
   tails the log; `run_fs_list` / `run_fs_read {run_id, path}` read the work
   product. All keep working after the parent prompt is `delete`d (tombstone).

---

## 12. Deploy notes

- **Restart the dashboard after deploying** so it re-reads the prompts manifest —
  the tool set changed (16 bare/`run_*` tools, the `user_prompt` field). See the
  suite `CLAUDE.md` deploy flow (bump → ship → stage → deploy).
- `etc/manifest.env` is **regenerated from the binary at deploy** (`opsctl
  deploy`); `CONSUMES` is emitted from `Spec.Consumes`
  (`cron,crm,ledger,dropbox,scripts`).
- One secret: `ANTHROPIC_API_KEY` (SSM app-config → launcher → env), seeded
  before first start.
- The box needs a `python3` interpreter on `PATH` for the agent's `bash`-mediated
  `python3`.

---

## 13. Surface 2 — the in-run suite toolset

§10 is **Surface 1**: the foreground prompts MCP tools (bare verbs) an owner
drives through nginx to manage prompts and runs. **Surface 2** is the other
direction — the toolset the sandboxed Claude agent *inside* a run can reach. On
top of the built-in, sandbox-confined tools (`read`/`bash`/`write`/`edit`/
`glob`/`grep`, §9) the in-run agent is now also handed **the other suite
services' MCP tools** — send email, post to the ledger, read dropbox, and so on —
so a run can act across the whole suite *on behalf of the run's owner*. This is a
distinct **MCP/tool plane**; it does not touch `CONSUMES` or any `/feed` wiring
(the event plane, §7/§8), which is unchanged.

The pieces are split by reuse: the generic seam lives in `agentkit`
(`agentkit/agent.ToolSource` + the outbound `agentkit/mcpclient`), and the
suite-specific policy lives in **`prompts/internal/suite`** (discovery, identity
injection, self-exclusion).

### Loopback mechanism

Every suite service binds `127.0.0.1:<PORT>` and mounts a bare `POST /mcp` behind
`requireIdentityHeaders` (appkit). nginx is "the trust boundary" only because it
is the sole *public* ingress; the `/srv/<svc>/` prefix it strips applies to
public ingress only, so on loopback the service answers at the bare `/mcp` path —
`MOUNT` is irrelevant there. A co-resident process therefore reaches a peer
directly. For each discovered peer, prompts connects to
`http://127.0.0.1:<PORT>/mcp` and speaks **JSON-RPC 2.0 over HTTP POST**
(`Content-Type: application/json`) — `tools/list`, then `tools/call`. There is
**no SSE** and **no `initialize` handshake**: the suite handlers are stateless per
request (identity comes from the headers on each call), so discovery and dispatch
call the two methods directly. The client is `agentkit/mcpclient`; a peer that
ever required `initialize` would simply be dropped at discovery.

### Snapshot at spawn

The service + tool set is **frozen once when the run starts** and fixed for the
run's life — the same "pin inputs per run" rule the rest of the service follows
(§2). The runner calls
`suite.Discover(ctx, manifestRoot, run.OwnerEmail, run.PromptID)` in `execute`,
before `agent.Run`. `Discover` reads the on-box per-service manifests via
`appkit/inventory.Read(manifestRoot)` (which globs
`<manifestRoot>/*/etc/manifest.env`, keeps the `MCP=true` services, and reads
each `PORT`), then
concurrently `tools/list`es each peer and builds an immutable name→descriptor +
name→owning-client index. A service that comes up *after* spawn is not
re-discovered mid-run; runs are short relative to deploy cadence.

### Identity injection

Discovery builds each peer's `mcpclient.Client` with two headers, sent on every
request to that peer:

| header | value | purpose |
|---|---|---|
| `X-Owner-Email` | the run's `owner_email` | the identity the peer acts as |
| `X-Client-Id`   | `prompts:<prompt_id>`  | downstream audit attribution |

`requireIdentityHeaders` trusts a non-empty `X-Owner-Email` **blindly** — no token
logic, no authorization-server round-trip. So **prompts becomes a second trusted
loopback identity-injector alongside nginx.** The injected owner is always the
run's *own* `owner_email`, captured on `prompt.Run` when the owner created the
prompt through nginx — legitimate delegation, not cross-owner escalation.

### Self-exclusion

The `prompts` service is dropped from discovery by name (`selfName ==
"prompts"`), so a run is never handed prompts' own tools — no run-spawns-run
recursion. Self-chaining stays an event-plane concern (§8).

### Best-effort everywhere

Nothing in this path can crash a run. A down, unreachable, or garbled peer is
logged loud and **skipped** at discovery; `Discover` never returns an error and
always returns a non-nil (possibly empty) `ToolSource`. A downstream `tools/call`
that errors at the transport level or returns `isError` becomes an `is_error`
`tool_result` block, not a Go error — the agent loop sees it and continues. A
per-call client timeout (30s) is well under the run TTL (§6), so a wedged peer
surfaces as an `is_error` result rather than a run-killing hang.

### Wiring (the `agent.ToolSource` seam)

`agentkit/agent.Run` takes an `agent.Options{… Tools ToolSource}`. When `Tools`
is non-nil the loop appends the source's `Descriptors()` to `req.Tools` (so the
model sees the built-ins **plus** the suite tools) and routes each `tool_use`
block whose name the source `Owns` to the source's `Dispatch`; everything else
falls through to the built-in `tools.Dispatch(ctx, sandboxRoot, …)`. Peers now
register **bare verbs** (`health`, `reflection`, `list`, …; see
`docs/adr-mcp-tool-bare-names.md`), so the same verb is exposed by many services
and is no longer unique across peers. To keep the in-run flat namespace
collision-free, the suite layer **re-qualifies** each discovered bare verb back to
`ikigenba_<svc>_<verb>` (using the owning service's manifest `APP` name) and keys
its dispatch index — and the advertised `provider.Tool.Name` — on that qualified
name; at dispatch it translates the qualified name back to the bare verb the peer
actually answers to before `tools/call`. Built-in tools have no prefix
(`read`/`bash`/…), so `Owns` stays an exact-name membership test with no
collisions against the built-ins. `prompts/internal/runner` builds the source at spawn via an injectable
discover seam (so tests inject a fake) and passes it as `Options.Tools`;
`buildRequest` keeps advertising only the built-in `tools.All()` — the suite tools
are added by the agent loop from the `ToolSource`, the single source of truth.
Suite tool calls flow through the same `dispatchTools` path as built-ins, so they
land in the run's `output.jsonl` stream-json log and the tracer for free.

### Config

| env | default | meaning |
|---|---|---|
| `PROMPTS_MANIFEST_ROOT` | `/opt` | root globbed for `*/etc/manifest.env` to discover loopback MCP peers; the repo root in local dev. |

Read at the composition root (`cmd/prompts/main.go`) and threaded into
`runner.New`, then into `suite.Discover`. **No new secret.** Downstream
credentials (e.g. gmail's per-owner Google grant) remain the *downstream*
service's concern — prompts only asserts the identity header; the peer looks up
that owner's grant.

### Security note — a deliberate trust escalation

This is a real, intentional expansion of a run's reach, and the design records it
as such:

- A run's **blast radius grows** from "its sandbox directory" to "anything the
  run's owner can do across the suite" — driven by a non-deterministic agent.
- prompts is now a **second loopback identity-injector** alongside nginx. This is
  consistent with the existing model (loopback bind = the trust boundary,
  everything co-resident is trusted) but it *is* an expansion of who injects
  `X-Owner-Email`. The injected owner is always the run's own `owner_email`, so
  this is delegation within one owner, never privilege escalation across owners.
- Sandbox **path-confinement (§9) still governs the *file* tools** only; it does
  **not** and cannot constrain the MCP tools, which reach real owner data in real
  services by design.
- **OS / network isolation of the sandbox remains the pre-existing deferred gap**
  (§9, §14). This change *intentionally* grants the agent loopback access to
  privileged peers, so any future bubblewrap/podman `--network` story must
  **allowlist the loopback MCP ports** rather than cut all networking — see §14.

### Known limitation — tool descriptions are dropped

`mcpclient.Tool` parses `{name, description, inputSchema}` from each peer's
`tools/list`, but the provider-neutral descriptor type `agentkit/provider.Tool`
carries only **`Name` + `InputSchema`** — it has **no tool-level description
field**. So a discovered suite tool is advertised to the model by **name + JSON
input schema** (whose own per-property descriptions *do* carry through verbatim,
since the schema round-trips as raw JSON), but the upstream tool-level prose
description is **dropped** at the `provider.Tool` boundary. This is a current
limitation, not a fundamental one — a fast-follow could fold the description into
the schema, or extend `provider.Tool` with a description field.

---

## 14. Deferred / known gaps

- **Self-consumption** (prompts → its own feed) — one-line fast-follow.
- **Retention / GC** of run directories.
- **Sandbox seeding / cross-run carry-over** — explicit non-goal.
- **Sandbox isolation hardening** (bwrap/podman, network isolation) — likely
  platform-level. Note this now interacts with Surface 2 (§13): the in-run agent
  is *intentionally* granted loopback access to the suite's privileged MCP peers,
  so a future `--network` story must **allowlist the loopback MCP ports** rather
  than cut all networking — otherwise it would also sever the suite toolset.
- **Per-prompt allow-list / deny-list** of suite services or tools — all-on today
  (§13); an additive `config` field later.
- **Push-to-owner delivery** / talking back into the calling conversation.
</content>
