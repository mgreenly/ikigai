# prompts — architecture (draft 1, run-once slice)

**Status:** high-level architecture, settled in design discussion; not yet built.
This document is the durable plan the code is written against. It captures the
module layout, the components, how they fit together, and the procedures
(engine copy-in, lifecycle scripts) that the run-once slice needs. It is the
companion to `README.md` (which holds the *why* / the design ledger); this is
the *how the code is shaped*.

Scope is the run-once slice from `README.md`: create a session → set its prompt
→ run it once → poll status → read the output / the file it wrote. **Anthropic
only** (haiku / sonnet / opus). Loop/schedule/event triggers, the event plane,
push delivery, and real sandbox isolation are all **deferred** (see Deferred).

---

## 1. Where the parts come from

prompts is assembled from three sources, matching the README's "reuses vs. new":

| Layer | Source | Disposition |
|---|---|---|
| **Chassis** — loopback HTTP, nginx identity gate, PRM doc, MCP JSON-RPC, SQLite + migrations, `bin/*` lifecycle | `../ledger` | clone + rename `ledger`→`agent` |
| **Run engine** — provider seam, agent tool-use loop, file toolset, model registry | `~/projects/ikigai-cli` (Go) | **copy into the tree** (see §6) |
| **Orchestration** — sessions, sandboxes, async run lifecycle | new | the genuinely-new code (§5) |

Note on the engine reference: the README cites `ikigai-cli`; a sibling project
`ikigai-tui` also exists but is **C** and is only a conceptual reference. The
directly-portable Go engine is `ikigai-cli`, and it will be **discarded
upstream** once borrowed — so prompts owns its copy outright (no sync-back).

---

## 2. Module layout

```
prompts/
├── cmd/prompts/main.go            # composition root  (← ledger, renamed)
├── go.mod   (module prompts)      # engine is stdlib-only, so no new deps
├── Makefile · bin/* · etc/*       # PORT=3004, MOUNT=/srv/prompts/, PROMPTS_* env
└── internal/
    ├── db/   ids/   logging/   server/    # chassis, ~verbatim from ledger
    ├── mcp/                               # chassis, EXTENDED: holds a Service, 11 tools
    │                                      #   Handler{ svc *session.Service }
    ├── session/        # NEW — the domain
    │     ├── model.go  #   Session, Run, Config, status enums
    │     ├── store.go  #   SQLite queries (sessions + runs tables)
    │     └── service.go#   CRUD + status + single-flight gate; entry to runner
    ├── sandbox/        # NEW — per-session folder: Create / List / Read + path confinement
    ├── runner/         # NEW — run lifecycle: spawn goroutine, TTL ctx, cancel,
    │                   #   crash-recovery sweep, output-log sink
    └── engine/         # COPIED from ikigai-cli (import paths rewritten)
          ├── provider/{provider.go, anthropic/}   # seam kept; only anthropic wired
          ├── agent/    # loop.go — the tool-use iteration, unchanged
          ├── tools/    # bash/read/write/edit/glob/grep  (+ confinement at dispatch)
          ├── model/    # opus/sonnet/haiku registry
          └── wire/     # stream-json event sink
```

Rename points (full list in the ledger exploration): module `ledger`→`agent`,
`cmd/ledger`→`cmd/agent`, env prefix `LEDGER_`→`AGENT_`, port `3002`→`3004`,
mount `/srv/ledger/`→`/srv/agent/`, tool prefix → `ikigenba_agent_`, db
`ledger.db`→`agent.db`, app user / `/opt/ledger` / systemd unit → `agent`.

---

## 3. Chassis (from ledger — what carries over unchanged)

- **`cmd/prompts/main.go`** — composition root. Reads `PROMPTS_*` env, opens SQLite
  (`db.Open` — WAL, FK, single writer), runs embedded migrations
  (`db.Migrate`), builds the MCP handler, builds the HTTP server
  (`server.New`), serves with graceful shutdown. prompts adds two wiring steps
  here: (a) construct the `session.Service` (with store, sandbox root, runner)
  and inject it into the MCP handler; (b) run the **crash-recovery sweep**
  (§5.3) right after migrate.
- **`internal/server`** — `POST /mcp` behind `requireIdentityHeaders` (reads
  injected `X-Owner-Email` / `X-Client-Id`, 401 + `WWW-Authenticate` if absent);
  the ungated `GET /health` liveness route and
  `GET /.well-known/oauth-protected-resource` are open. nginx is the sole trust
  boundary; prompts trusts the headers blindly.
- **`internal/db`** — SQLite open + embedded `migrations/NNN_*.sql` runner
  (idempotent, downgrade-refusing). prompts adds `002_prompts.sql` (§4).
- **`internal/ids`** — ULID generation (session ids, run ids).
- **`internal/logging`** — slog JSON + request-id middleware.
- **`internal/mcp`** — JSON-RPC 2.0 dispatch (`initialize`, `tools/list`,
  `tools/call`). EXTENDED for prompts: the skeleton's `Handler struct{}` becomes
  `Handler{ svc *session.Service }`; `toolDescriptors()` lists the 11 tools (§7);
  `dispatchTool` routes each `ikigenba_prompts_*` name to a `Service` method,
  marshals the result to MCP content. `ikigenba_prompts_health` stays as the
  chassis proof.

---

## 4. Data model (`002_prompts.sql`)

```sql
CREATE TABLE sessions (
    id            TEXT PRIMARY KEY,        -- ULID
    owner_email   TEXT NOT NULL,           -- from X-Owner-Email at create
    name          TEXT,
    prompt        TEXT NOT NULL,
    system_prompt TEXT,
    config_json   TEXT NOT NULL,           -- normalized {provider, model, effort?, max_tokens?, temperature?}
    status        TEXT NOT NULL,           -- 'idle' | 'running'
    created_at    TEXT NOT NULL,
    updated_at    TEXT NOT NULL
);

CREATE TABLE runs (
    id          TEXT PRIMARY KEY,          -- ULID
    session_id  TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    status      TEXT NOT NULL,             -- 'running' | 'succeeded' | 'failed' | 'cancelled'
    started_at  TEXT NOT NULL,
    ended_at    TEXT,
    usage_json  TEXT,                      -- token/usage totals from the engine
    error       TEXT,
    log_path    TEXT NOT NULL              -- data/runs/<session_id>/<run_id>.jsonl
);
CREATE INDEX idx_runs_session ON runs(session_id, started_at);
```

- **Config is a normalized blob**, validated at create time (§5.1). Storing it
  as JSON keeps the schema stable as config fields grow.
- **`last_run`** in the MCP surface = the newest `runs` row for the session.
- **Ownership** is recorded per session; every MCP call is scoped to the
  caller's `X-Owner-Email`.

On-disk state lives under `data/` (never touched by deploy):
```
data/
├── prompts.db                     # the SQLite file
├── sandboxes/<session_id>/        # the agent's persistent work folder
└── runs/<session_id>/<run_id>.jsonl   # the run's stream-json output log
```

---

## 5. The three new components

### 5.1 `session` — the domain service

`Service` is the only thing the MCP handler talks to, and the only thing that
mutates session/run state. Responsibilities:

- **Create** — validate config (`model.Resolve` recognizes the model alias;
  the model's provider is `anthropic`; `ANTHROPIC_API_KEY` is present in env —
  "validated per session"), insert the session (`idle`), and
  `sandbox.Create(id)` to make the empty folder. Records `owner_email`.
- **List / Get** — owner-scoped reads; `Get` joins the latest run for
  `last_run` (status, started/ended, usage, error).
- **Update** — edit prompt / system_prompt / config / name. **Rejected while
  `running`.** Re-validates config on change.
- **Delete** — remove session row (cascade runs), the sandbox folder, and the
  run logs. **Rejected while `running`** (cancel first).
- **Run** — the single-flight gate: if the session is already `running`, return
  `busy`; else insert a `run` row (`running`), flip session to `running`, and
  hand off to `runner.Spawn`. Returns immediately.
- **Cancel** — signal the in-flight run's context; folder is kept.

### 5.2 `sandbox` — confined per-session folder

Wraps `data/sandboxes/<session_id>/`. Two jobs:

1. **Read surface** for MCP: `List(path)` and `Read(path, offset, limit)`,
   both rejecting any path that escapes the folder (clean + `..` checks against
   the resolved root).
2. **Confinement helpers for the engine toolset** — the borrowed tools have
   *no* confinement (this is the retrofit, see §6):
   - `bash` → set `cmd.Dir = sandboxRoot` (runs in the folder).
   - `read` / `write` / `edit` (already absolute-path-only) → validate the path
     resolves under `sandboxRoot` before touching disk.
   - `glob` / `grep` (default to `os.Getwd()`) → inject `sandboxRoot` as the
     search root.

**Draft-1 confinement is Go-level path checks + bash `cmd.Dir` only.** There is
**no** network isolation and **no** OS sandbox — `bash` can still reach the
network and, with effort, escape. Real isolation (bubblewrap / rootless podman
`--network none` + bind-mounted volume + python image) is **deferred** and is
likely an ikigenba *platform* concern. This is a known, accepted gap for draft 1.

The agent's `python` is reached *through* `bash` (`python3 ...`), so the only
runtime requirement is a `python3` interpreter on the box's `PATH` (§8).

### 5.3 `runner` — async run lifecycle

`Spawn(session, sandbox, run)` starts a goroutine and returns. The goroutine:

1. `ctx, cancel := context.WithTimeout(parent, PROMPTS_RUN_TTL)` — the TTL is the
   runaway-goroutine backstop. (Idle-network watchdog is **deferred**.)
2. Open the log sink: a file `data/runs/<session>/<run>.jsonl`, wrapped as the
   engine's `wire.Session(writer)`. The engine already emits **stream-json**
   (one JSON event per line: assistant / user / result) — append-only and
   line-addressable, so `ikigenba_prompts_session_output` is a cheap line-slice with no
   transform.
3. Build the Anthropic client (`anthropic.New(os.Getenv("ANTHROPIC_API_KEY"),
   model)`) and the `provider.Request`: system_prompt + framing, the user
   `prompt`, the confined toolset (bash/read/write/edit/glob/grep), model, and
   effort→provider params from the model registry.
4. `agent.Run(ctx, client, sess, req, …)` — drives the tool-use loop to a
   terminal stop, tools confined to the sandbox.
5. On exit: write the run's terminal `status` (`succeeded` / `failed` /
   `cancelled`), `ended_at`, `usage_json`, `error`; flip the session back to
   `idle`. The folder persists regardless.

**Single-flight** is enforced by `session.Service.Run` (one in-flight run per
session; shared mutable folder ⇒ no overlap). The runner tracks the active
run's `cancel` func (in-memory map keyed by session id) so `Cancel` can reach it.

**Crash recovery** — on boot, after migrate, sweep `runs WHERE status='running'`:
mark them `failed` (orphaned by the crash) and flip their sessions to `idle`.
The sandbox folder is left as-is (forward-only on disk); a fresh run inherits
whatever files the crashed run wrote. "Fresh run" means fresh *conversation*,
not fresh disk.

---

## 6. Engine copy-in procedure

ikigai-cli's engine lives entirely under `internal/`, which Go forbids importing
from another module. So we **copy**, not depend:

1. Copy these packages from `~/projects/ikigai-cli/app-root/internal/` into
   `prompts/internal/engine/`: `provider/` (incl. `provider.go` and
   `anthropic/`), `agent/`, `tools/`, `model/`, `wire/`, plus whatever small
   support packages they import (e.g. `schema`, `trace` if used by `agent.Run` —
   resolve by compiling).
2. Rewrite import paths: `github.com/ai4mgreenly/ikigai-cli/internal/… →
   prompts/internal/engine/…` (mechanical sed across the copied tree).
3. **Drop** the non-Anthropic providers (`openai/`, `google/`) — or keep them
   behind the seam, unwired. Draft 1 only constructs the Anthropic client.
   Keeping the `provider.Client` interface intact preserves the seam for later.
4. **Add confinement at the tool dispatch boundary** (`tools/dispatch.go`):
   thread a `sandboxRoot` through dispatch and apply the §5.2 rules. This is the
   one substantive engine modification.
5. The engine is **stdlib-only** (hand-rolled HTTP, no Anthropic SDK), so no new
   `go.mod` requires beyond what ledger already has (`modernc.org/sqlite`).

Key borrowed types (for reference when wiring):
- `provider.Client` — `Stream(ctx, Request) (<-chan Event, error)`.
- `provider.Request{ Model, Effort, SystemPrompt, Messages, Tools, ResponseSchema }`.
- Blocks: `TextBlock`, `ToolUseBlock`, `ToolResultBlock`, `ThinkingBlock`.
- Events: `EventTextDelta`, `EventToolUse`, `EventThinking`, `EventUsage`,
  `EventDone{StopReason}`.
- `agent.Run(ctx, client, sess, req, sch, tracer)` — the iteration loop.
- `model.Resolve(alias)` — `opus`/`sonnet`/`haiku` → concrete ids + metadata.

---

## 7. MCP tool surface (11 tools) → Service mapping

All reads are owner-scoped and session-scoped to the **latest run** (no `run_id`
in the surface — run-history browsing is deferred). The sandbox is **read-only**
from the foreground. The toolset is **fixed**.

| MCP tool | Service entry | Notes |
|---|---|---|
| `ikigenba_prompts_health` | (chassis) | identity proof; no side effects |
| `ikigenba_prompts_session_create` | `Service.Create` | validates config; makes sandbox; → `{session_id, status:"idle"}` |
| `ikigenba_prompts_session_list` | `Service.List` | owner-scoped enumeration |
| `ikigenba_prompts_session_get` | `Service.Get` | full detail incl. `last_run` |
| `ikigenba_prompts_session_update` | `Service.Update` | rejected while `running` |
| `ikigenba_prompts_session_delete` | `Service.Delete` | rejected while `running`; removes folder + logs |
| `ikigenba_prompts_session_run` | `Service.Run` | async; `busy` if in-flight; → `{status:"running", started_at}` |
| `ikigenba_prompts_session_cancel` | `Service.Cancel` | terminate in-flight run; folder kept |
| `ikigenba_prompts_session_output` | reads `runs.log_path` | latest run's jsonl, by line range; tailable |
| `ikigenba_prompts_session_fs_list` | `sandbox.List` | path-escape rejected |
| `ikigenba_prompts_session_fs_read` | `sandbox.Read` | path-escape / not-a-file rejected |

---

## 8. Lifecycle scripts (delta from the ledger skeleton)

prompts ships **setup / deploy / start / stop / secrets / backup / restore /
teardown** (ledger had only build/deploy/setup/start/stop). prompts is the first
service whose *runtime*
shells out beyond its own Go binary — the agent's `bash` tool runs `python3` —
so the box must have a Python interpreter.

- **`bin/setup`** — adds idempotent Python provisioning after the app-user /
  `/opt/prompts` tree and before the nginx fragment:
  ```sh
  ssh … 'command -v python3 >/dev/null || sudo dnf install -y python3 python3-pip'
  ```
  Plain `python3` as shipped by **Amazon Linux 2023** (no version pin). Just the
  interpreter + pip — no persistent package set (draft-1 sandboxes aren't
  isolated, and pip installs are meant to evaporate per run).
- **`bin/secrets`** — new. One secret: `ANTHROPIC_API_KEY`. Non-destructive
  read-modify-write of prompts' own key in SSM `/ikigenba/<account>/app-config`,
  value from `~/.secrets/ANTHROPIC_API_KEY`. Must be seeded **before first
  start** (launcher hard-fails if `.["prompts"]` is missing). Locally the same
  secret arrives via `.envrc` → env; the engine reads `os.Getenv("ANTHROPIC_API_KEY")`.
- **`bin/backup` / `bin/restore`** — new. prompts holds durable state the run-once
  slice can lose on box replacement: the SQLite DB (`data/prompts.db` — sessions +
  run history), the **sandbox folders** (`data/sandboxes/` — the agents' actual
  work product), and the run logs (`data/runs/`). Backup is `aws s3 sync` of
  `data/` to the per-account backup bucket (`--profile ${ACCOUNT} --region
  us-east-2`, live SSO session required); restore syncs back. One wrinkle: the
  WAL-mode DB isn't consistent under a live `sync`, so `backup` takes a
  consistent DB snapshot first (`VACUUM INTO` / the SQLite backup API) and syncs
  the snapshot alongside the folders, rather than copying the hot `prompts.db`.
  Sandbox/run folders sync as-is (forward-only files).
- **`bin/teardown`** — new. Reverse of setup: stop+disable the unit, remove
  `/opt/prompts` (incl. `data/` — DB **and** sandbox folders), drop the nginx
  fragment, `nginx -t` + reload. Does **not** uninstall python3 (shared box
  resource prompts didn't exclusively own).
- **`bin/start` / `bin/stop`** (local dev) — add a soft `python3` preflight
  warning; non-fatal (prompts boots fine without it; only the agent toolset needs it).
- **`bin/build` / `bin/deploy`** — ledger shape, renamed (3002→3004,
  ledger→agent). The launcher injects `ANTHROPIC_API_KEY` from app-config at
  start; the build wrapper composes `PROMPTS_*` public config from
  `IKIGENBA_DOMAIN` / `PORT`.

**Registering with the dashboard:** add
`https://${IKIGENBA_DOMAIN}/srv/prompts/mcp` to `dashboard/bin/build →
DASHBOARD_RESOURCES` and redeploy the dashboard, or `/internal/authn` won't
issue a PRM challenge for prompts and connector OAuth can't discover it.

---

## 9. End-to-end flow (the fusion example)

1. `ikigenba_prompts_session_create {prompt, config}` → `Service.Create` → validate config
   → insert session (`idle`) → `sandbox.Create(id)` → `{session_id, "idle"}`.
2. `ikigenba_prompts_session_run {session_id}` → single-flight check (`busy` if not) →
   insert run (`running`), session→`running` → `runner.Spawn` → return
   `{running, started_at}` immediately.
3. goroutine: `agent.Run` drives Anthropic; tools confined to the sandbox;
   narration streams into `<run>.jsonl`; on finish → run `succeeded`,
   session→`idle`.
4. `ikigenba_prompts_session_output {session_id, offset, limit}` → slice the jsonl.
   `ikigenba_prompts_session_fs_list` / `_read` → confined sandbox reads. status/usage ride
   on `ikigenba_prompts_session_get.last_run`.

---

## 10. Config & secrets

- **Public config** (`PROMPTS_*` env, composed by the build wrapper from
  `IKIGENBA_DOMAIN`/`PORT`): port 3004, resource id, auth server, db path, log
  level, plus `PROMPTS_RUN_TTL` (the run backstop, e.g. `30m`).
- **Secret**: `ANTHROPIC_API_KEY` — the only one. On the box: SSM app-config →
  launcher → process env. Locally: `~/.secrets/ANTHROPIC_API_KEY` → `.envrc` →
  env. Read once via `os.Getenv` in the runner's client construction; never on
  disk, never logged.

---

## 11. Deferred / known gaps (draft 1)

- **Sandbox isolation** — no bwrap/podman, no network isolation; bash can reach
  the network. Go-level path confinement only. Likely a platform concern.
- **`websearch` / `webfetch`** — the agent-mediated network tools are **not** in
  draft 1 (the engine ships none; left out by decision).
- **Triggers beyond run-once** — loop / schedule / event-subscribe and their
  overlap policy.
- **Event plane** — `publish_event`, multi-tenant `eventplane`, the consumer
  cursor guarantees.
- **Delivery beyond pull** — push-to-owner, talking back into the conversation.
- **Idle-network watchdog** — detecting a silently-stalled LLM stream (TTL only
  for now).
- **Run-history browsing** — `run_id`-addressed reads; the surface is
  latest-run-only.

## 12. Open items to confirm

- *(none currently — backup/restore resolved: in scope, `aws s3 sync` of `data/`
  to the per-account bucket with a consistent DB snapshot; see §8.)*
