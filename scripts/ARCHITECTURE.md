# scripts — architecture (draft 1)

**Status:** settled in design discussion; not yet built. This is the durable
plan the code is written against — module layout, components, data model, and
lifecycle procedures. Companion to `README.md` (the *why* / design ledger); this
is *how the code is shaped*.

Scope: run-once **and** general event triggers **and** completion events — the
full glue-engine slice. Deferred: backpressure/overlap policy, run retention/GC,
third-party packages, OS sandbox isolation (see README → Deferred).

---

## 1. Where the parts come from

scripts is assembled from two sources (no borrowed engine — that was `agent`'s
hard part, and scripts doesn't have one):

| Layer | Source | Disposition |
|---|---|---|
| **Chassis** — loopback HTTP, nginx identity gate, PRM doc, MCP JSON-RPC, SQLite + migrations, `bin/*` lifecycle, outbox `/feed`, consumer engine | `../agent` (itself cloned from `../ledger`) | clone + rename `agent`→`scripts` |
| **Orchestration** — scripts CRUD, the `python3`-exec runner, general triggers, completion events | new (adapted from `agent`'s session/runner/trigger/consume/outcome) | the genuinely-new code (§5) |

`agent` is the better clone base than `ledger`: it already carries the runner,
trigger table, consumer fan-in, and outcome-event wiring. scripts **adapts** them
(cron-only → general source; single-flight → unbounded; LLM run → `python3` run;
outcome-without-output → completion-with-output) rather than building from the
health-only skeleton.

Rename points: module `agent`→`scripts`, `cmd/agent`→`cmd/scripts`, env prefix
`AGENT_`→`SCRIPTS_`, port `3004`→**`3009`**, mount `/srv/agent/`→`/srv/scripts/`,
tool prefix `ikigenba_agent_`→`ikigenba_scripts_`, db `agent.db`→`scripts.db`,
app user / `/opt/agent` / systemd unit → `scripts`.

---

## 2. Module layout

```
scripts/
├── cmd/scripts/main.go            # composition root  (← agent, renamed)
├── go.mod   (module scripts)      # stdlib + modernc.org/sqlite + eventplane (replace)
├── Makefile · bin/* · etc/*       # PORT=3009, MOUNT=/srv/scripts/, SCRIPTS_* env
└── internal/
    ├── db/   ids/   logging/   server/    # chassis, ~verbatim from agent
    ├── mcp/                               # chassis, EXTENDED: Handler{ svc *script.Service }
    │                                      #   toolPrefix "ikigenba_scripts_"; 16 tools (§7)
    ├── script/        # NEW — the domain
    │     ├── model.go #   Script, Run, Config, status enums
    │     ├── store.go #   SQLite queries (scripts + runs + script_triggers)
    │     ├── service.go#  CRUD + run + run-instance ops + trigger ops
    │     ├── trigger.go#  {source,event_filter} rows; fan-out lookup
    │     └── outcome.go#  completion-event build (scripts.succeeded/failed)
    ├── runner/        # NEW — run lifecycle: spawn python3, TTL ctx, cancel-by-run_id,
    │                  #   crash-recovery sweep, stdout/stderr log sinks
    └── consume/       # NEW — one consumer loop per upstream; (source,type) → fire scripts
```

No `sandbox/` package (no persistent per-script folder) and no `engine/` package
(no LLM). The runner is a thin process-exec wrapper, not an agent loop.

---

## 3. Chassis (from agent — carries over ~unchanged)

- **`cmd/scripts/main.go`** — composition root. Reads `SCRIPTS_*` env, opens
  SQLite (`db.Open` — WAL, FK, single writer), runs embedded migrations, builds
  the MCP handler, builds the HTTP server (`appkit`/`server.New`) with the outbox
  `/feed` mounted, starts the consumer loops as workers, serves with graceful
  shutdown. scripts adds: (a) construct `script.Service` (store, runner, work +
  runs dirs) and inject into the MCP handler; (b) the **crash-recovery sweep**
  after migrate; (c) one `consumer.Run` worker per upstream producer.
- **`internal/server`** (via appkit) — `POST /mcp` behind `requireIdentityHeaders`;
  ungated `GET /health` and `GET /feed` and the PRM doc. nginx is the sole trust
  boundary.
- **`internal/db`** — SQLite open + embedded migration runner. scripts adds its
  domain migration + the eventplane outbox/feed_offset migrations (§4).
- **`internal/ids`** — ULID generation (script ids, run ids).
- **`internal/mcp`** — JSON-RPC 2.0 dispatch. `Handler{ svc *script.Service }`;
  `toolPrefix = "ikigenba_scripts_"`; `toolDescriptors()` lists the §7 tools;
  `dispatchTool` routes each name to a `Service` method.

---

## 4. Data model

### `002_scripts.sql` (domain)

```sql
CREATE TABLE scripts (
    id          TEXT PRIMARY KEY,        -- ULID
    owner_email TEXT NOT NULL,           -- from X-Owner-Email at create
    name        TEXT,
    body        TEXT NOT NULL,           -- the Python source text
    config_json TEXT NOT NULL,           -- normalized {interpreter?, timeout_secs?, ...}; minimal day-one
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL
);

CREATE TABLE runs (
    id          TEXT PRIMARY KEY,        -- ULID — the run/instance id
    script_id   TEXT NOT NULL REFERENCES scripts(id),  -- NO cascade: runs are append-only history (tombstone delete); script_id may dangle after the script is deleted
    status      TEXT NOT NULL,           -- 'running' | 'succeeded' | 'failed' | 'cancelled'
    exit_code   INTEGER,                 -- null while running / never-started
    started_at  TEXT NOT NULL,
    ended_at    TEXT,                    -- null while running
    error       TEXT,                    -- failure / TTL / spawn reason
    trigger_source   TEXT,               -- '' for a manual run
    trigger_type     TEXT,
    trigger_event_id TEXT,
    stdout_path TEXT NOT NULL,           -- data/runs/<run_id>/stdout.log
    stderr_path TEXT NOT NULL            -- data/runs/<run_id>/stderr.log; the run dir also persists main.py + config.json + any produced files
);
CREATE INDEX idx_runs_script ON runs(script_id, started_at);
CREATE INDEX idx_runs_status ON runs(status);

CREATE TABLE script_triggers (
    script_id    TEXT NOT NULL REFERENCES scripts(id) ON DELETE CASCADE,
    source       TEXT NOT NULL,          -- upstream producer, e.g. "crm"
    event_filter TEXT NOT NULL,          -- glob: "contact.created", "contact.*", "cron.nightly"
    created_at   TEXT NOT NULL,
    PRIMARY KEY (script_id, source, event_filter)
);
CREATE INDEX idx_script_triggers_source ON script_triggers(source);
```

Notes:
- **No `status` column on `scripts`** — unbounded model, no idle/running lifecycle.
  `running_count` is **derived**: `SELECT COUNT(*) FROM runs WHERE script_id=? AND status='running'`.
- A script may hold **many** triggers (unlike `agent`'s 1:1) — the PK is the triple.
- `config_json` is a normalized blob, validated at create time; starts minimal.

### eventplane migrations (owned by the library, applied by scripts' runner)

- `003_outbox.sql` — the producer outbox (byte-identical to `outbox.SchemaSQL`).
- `004_feed_offset.sql` — the consumer cursor table (`source` PK), **one row per
  upstream** scripts consumes.

### On-disk state (under `data/`, never touched by deploy)

```
data/
├── scripts.db                     # the SQLite file
└── runs/<run_id>/                 # PERSISTENT — the runner's working dir, kept whole after the run stops
    ├── main.py                    #   materialized body (the exact source that ran)
    ├── config.json                #   pinned config for this run
    ├── {stdout,stderr}.log        #   captured output streams
    └── <files the script wrote>   #   produced artifacts (read via run_fs_list/run_fs_read)
```

There is no ephemeral `work/` dir: the run dir *is* the working dir, materialized
at spawn and persisted as the record of exactly what ran ("inputs that can change
live on disk per run").

---

## 5. The new components

### 5.1 `script` — the domain service

`Service` is the only thing the MCP handler talks to and the only thing that
mutates script/run/trigger state.

- **Create / List / Get / Update / Delete** — owner-scoped CRUD. `Get`/`List`
  attach `running_count` (derived) and `last_run`. `Update`/`Delete` proceed
  freely (no lifecycle gate — an in-flight run already holds its own materialized
  copy of the body). `Delete` is a **tombstone**: it removes the script row and
  its triggers (FK cascade), but runs + their on-disk artifacts **survive** as
  append-only history (matches ledger's immutable journal / the append-only
  outbox). `script_id` on a surviving run becomes a possibly-dangling label.
- **Run** — insert a `run` row (`running`, manual: empty trigger ctx), flip
  nothing (no single-flight), hand to `runner.Spawn`. Returns `{run_id, started_at}`
  immediately. **Always accepted.**
- **RunForEvent(scriptID, source, type, eventID, payload)** — the event path:
  insert a `run` row carrying trigger ctx, spawn with the event payload as the
  run's stdin/`$EVENT_JSON` input. No ownership scoping (the trigger linkage is
  authority; a consumer loop has no caller).
- **Run-instance ops** — `RunList(owner, scriptID?, status?)`, `RunGet(owner,
  runID)` (with computed `elapsed_secs`), `RunOutput(owner, runID, stream,
  offset, limit)` (line-slice the persisted logs, tailable), `RunCancel(owner,
  runID)` (→ runner cancel-by-run_id).
- **Trigger ops** — `SetTrigger(owner, scriptID, source, filter)` (validates
  `source`/`type` against known producer registries; rejects the unsatisfiable),
  `ClearTrigger`, and `ScriptsForEvent(source, type)` (fan-out lookup the consumer
  uses; **not** owner-scoped).

### 5.2 `runner` — the python3-exec run lifecycle

`Spawn(run, input)` starts a goroutine and returns. The goroutine:

1. `ctx, cancel := context.WithTimeout(context.Background(), SCRIPTS_RUN_TTL)` —
   the runaway backstop.
2. Register `cancel` in an in-memory map **keyed by `run_id`** (not script id —
   many concurrent runs per script, each independently cancellable). The map also
   records user-cancelled run ids so the goroutine classifies terminal status.
3. Materialize `body` into `data/runs/<run_id>/main.py` and pinned config into
   `data/runs/<run_id>/config.json`.
4. Open `data/runs/<run_id>/{stdout,stderr}.log`. Build the command:
   `python3 main.py`, `cmd.Dir = data/runs/<run_id>/`, `cmd.Env += EVENT_JSON=<input or {}>`,
   `cmd.Stdin = <input or empty>`, `cmd.Stdout`/`cmd.Stderr` → live log files.
   Put the child in its **own process group** (so cancel kills children too).
5. `cmd.Run()` under `ctx`.
6. On exit, classify: user-cancelled → `cancelled`; `ctx` deadline → `failed`
   ("run TTL exceeded"); non-zero exit → `failed`; spawn error → `failed`; else
   `succeeded`. Record `status`, `exit_code`, `ended_at`, `error`.
7. **Emit the completion event** (except for `cancelled`) — atomically with the
   terminal `runs` write, append `scripts.succeeded`/`scripts.failed` to the
   outbox on the same tx, commit, then `Ring()`. Payload built by
   `outcome.go` from the run row + the **8 KB tails** of the two log files.
8. **Do not delete the run dir.** body, config, logs, and any produced files
   persist as the record of the run (retention/GC deferred).

**Cancel** — `RunCancel` looks up the run_id in the cancel map, marks it
user-cancelled, calls `cancel()` → the process group is killed → terminal status
`cancelled`, no event.

**Crash recovery** — `Recover(ctx)` on boot sweeps `runs WHERE status='running'`,
marks them `failed` ("interrupted by restart"). Run dirs are **not** touched —
they persist as history; a crashed run keeps whatever partial tree it had.

### 5.3 `consume` — general trigger fan-in

One `consumer.Run` worker per upstream producer (`cron`, `crm`, `ledger`,
`dropbox`, `agent`), each reading `SCRIPTS_<SRC>_FEED_URL` with its own
`feed_offset` cursor. The shared handler, per event:

1. `ScriptsForEvent(source, ev.Type)` — DB lookup of subscribed scripts (glob
   match on `event_filter`).
2. For each, `RunForEvent(...)` on a goroutine (non-blocking; unbounded — one run
   per script per event).
3. Return `nil` for matched events (fire-and-forget; never stall the feed);
   `consumer.ErrSkip` only for a malformed envelope (poison → advance).

The cursor advances for **every** event (matched or not), per the consumer
contract.

---

## 6. MCP tool surface → Service mapping (§7 of README, 16 tools)

| MCP tool | Service entry |
|---|---|
| `ikigenba_scripts_health` | (chassis) — `details` = runtime contract |
| `ikigenba_scripts_describe` | (chassis) — overview + authoring contract |
| `ikigenba_scripts_create` | `Service.Create` |
| `ikigenba_scripts_list` | `Service.List` (+ `running_count`, `last_run`) |
| `ikigenba_scripts_get` | `Service.Get` |
| `ikigenba_scripts_update` | `Service.Update` |
| `ikigenba_scripts_delete` | `Service.Delete` |
| `ikigenba_scripts_set_trigger` | `Service.SetTrigger` (validated) |
| `ikigenba_scripts_clear_trigger` | `Service.ClearTrigger` |
| `ikigenba_scripts_run` | `Service.Run` → `{run_id, ...}` |
| `ikigenba_scripts_run_list` | `Service.RunList` |
| `ikigenba_scripts_run_get` | `Service.RunGet` (+ `elapsed_secs`) |
| `ikigenba_scripts_run_output` | `Service.RunOutput` (stdout/stderr, tailable) |
| `ikigenba_scripts_run_cancel` | `Service.RunCancel` → runner cancel-by-run_id |
| `ikigenba_scripts_run_fs_list` | `Service.RunFsList` — list the run's persisted dir tree |
| `ikigenba_scripts_run_fs_read` | `Service.RunFsRead` — read a file in the run dir |

The runtime contract in `health.details` / `describe` is **static declared**
strings (`python_version`, `bash_version`, `network`, `packages`), not probed.

---

## 7. Lifecycle scripts (delta from the agent skeleton)

scripts ships **setup / deploy / start / stop / backup / restore / teardown** —
**no `bin/secrets`** (the service holds no secret).

- **`bin/setup`** — adds idempotent Python provisioning: install **`python3.11`**
  (the advertised `>=3.11` floor) on Amazon Linux 2023, plus the app-user /
  `/opt/scripts` tree and the nginx fragment. No persistent package set
  (stdlib-only day-one). bash 5.x is already present on AL2023.
- **`bin/backup` / `bin/restore`** — durable state is the SQLite DB
  (`data/scripts.db` — scripts + run history + triggers + cursors) and the run
  per-run trees (`data/runs/` — each run's materialized inputs, logs, and
  produced files). `aws s3 sync` of `data/` to the per-account backup bucket,
  with a consistent DB snapshot (`VACUUM INTO`) synced alongside. There is no
  ephemeral `data/work/` to exclude.
- **`bin/start` / `bin/stop`** (local dev) — soft `python3.11` preflight warning;
  non-fatal.
- **`bin/teardown`** — reverse of setup; removes `/opt/scripts` incl. `data/`;
  drops the nginx fragment. Does **not** uninstall python3.11 (shared box resource).
- **`bin/build` / `bin/deploy`** — agent shape, renamed (3004→3009,
  agent→scripts). No secret injection. Composes `SCRIPTS_*` public config from
  `IKIGENBA_DOMAIN` / `PORT`, plus the per-upstream `SCRIPTS_<SRC>_FEED_URL` values
  for the consumer loops.

**Registering with the dashboard:** add `https://${IKIGENBA_DOMAIN}/srv/scripts/mcp`
to `dashboard/bin/build → DASHBOARD_RESOURCES` and redeploy the dashboard, or
connector OAuth can't discover scripts. After deploy, restart the dashboard so it
re-reads manifests.

---

## 8. End-to-end flows

**Manual run:**
1. `ikigenba_scripts_create {name, body}` → `Service.Create` → `{script_id}`.
2. `ikigenba_scripts_run {script_id}` → insert run (`running`, empty trigger ctx)
   → `runner.Spawn(run, emptyInput)` → `{run_id, "running", started_at}`.
3. goroutine: materialize body+config into `runs/<run_id>/` → `python3` (stdin
   empty, `$EVENT_JSON={}`, `cmd.Dir` = the run dir) → stream stdout/stderr to
   logs → exit → record terminal status → emit
   `scripts.succeeded`/`scripts.failed` (8 KB tails) → run dir persists.
4. `ikigenba_scripts_run_output {run_id}` → tail the logs;
   `ikigenba_scripts_run_fs_list`/`run_fs_read` → inspect files the run wrote.
   `ikigenba_scripts_run_get` → status/exit_code/elapsed.

**Event-triggered run (the glue case):**
1. `ikigenba_scripts_set_trigger {script_id, source:"crm", event_filter:"contact.created"}`
   — validated, inserted.
2. crm emits `contact.created` → scripts' crm consumer loop handler →
   `ScriptsForEvent("crm","contact.created")` → `RunForEvent(...)` per subscribed
   script, on a goroutine (unbounded).
3. runner: `python3` with the **contact snapshot** on stdin + `$EVENT_JSON` →
   does its work → emits `scripts.succeeded` carrying the output tail + the
   `{crm, contact.created, event_id}` trigger context.
4. Another script subscribed to `{scripts, "scripts.succeeded"}` can chain off it.

---

## 9. Config & secrets

- **Public config** (`SCRIPTS_*` env, composed by the build wrapper): port 3009,
  resource id, auth server, db path, log level, `SCRIPTS_RUN_TTL` (the run
  backstop, e.g. `30m`), and one `SCRIPTS_<SRC>_FEED_URL` per consumed upstream.
- **Secrets: none.** No provider, open network → the service holds no secret and
  ships no `bin/secrets`.

---

## 10. Deferred / known gaps (draft 1)

- **Backpressure** — unbounded concurrency, per-run TTL only; bounded pool / queue
  is later.
- **Run retention / GC** — runs accumulate unbounded; MCP-driven sweep is later.
- **Third-party packages** — stdlib-only; per-script deps later.
- **Sandbox isolation** — plain `python3` with network, no OS sandbox (platform
  concern).
- **Runtime feed discovery** — new producers need a `scripts` redeploy.
- **Non-Python interpreters** — `config.interpreter` reserved; `python3` only.
</content>
