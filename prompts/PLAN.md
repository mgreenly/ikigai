# agent — build plan (run-once slice)

**Status:** orchestration plan for the run-once slice defined in `ARCHITECTURE.md`
/ `README.md`. Not yet executed. This is the document a sequence of subagents is
driven against, one phase at a time, with a human review gate between phases.

## How this plan is executed

- **Phased and strictly sequential.** One subagent per phase, dispatched only
  after the previous phase's gate passes and is reviewed. No parallelization.
- **Every phase ends at a gate.** A gate is a mechanical check (`go build ./...`
  + `go test ./...` green, plus the phase-specific check). The orchestrator does
  not advance until the gate is green and the diff is reviewed.
- **Each subagent prompt is self-contained:** it carries the relevant
  `ARCHITECTURE.md` section(s), the interface contract it must hit, the file
  manifest it may touch, and its acceptance criteria. Subagents do not re-derive
  scope from scratch.

## Settled decisions (from the design discussion)

1. **Deliverable of the build:** a **verified local end-to-end run** (the fusion
   example over dev nginx :8080 → agent :3004, real Anthropic key). Box-deploy
   scripts are *written and reviewed* but a real box deploy is a **manual
   post-step**, out of scope for the orchestrated build.
2. **Adapt, don't freeze-copy.** ikigai-cli's engine is *fitted* to agent, not
   mirrored. Editing the borrowed code is expected and fine — agent owns its copy.
3. **Agent loop:** adapt `agent.Run` **in place** — make the schema optional, add
   a **freeform terminal mode** (on a non-tool stop, emit the assistant's final
   text as the result; no JSON parse/validate), swap the JSON-answer
   `FramingPrompt` for a agent framing prompt, and thread `sandboxRoot` + `ctx`
   through `Dispatch`. The structured-output path stays intact behind the
   optional schema. Preserve `drainTurn`/`dispatchTools` behavior (signed-thinking
   round-trip, usage accounting, per-tool sidecars).
4. **Testing bar:** prototype-grade, production-quality on a best-effort basis.
   Tests **verify assumptions**, not coverage. Priority: the **sandbox path
   confinement** (the security boundary). Copied engine gets no net-new tests
   (kept green if it ships its own); chassis tests kept green post-rename.
5. **Provider seam kept, only Anthropic wired.** Copy `provider/` + `anthropic/`;
   drop `google/`/`openai/` and adapt whatever factory references them to
   anthropic-only. The `provider.Client` interface is preserved.
6. **Service location:** the service lives in this `agent/` directory (Go module
   `agent`), beside `README.md` / `ARCHITECTURE.md` / `.envrc`. Added to the root
   `go.work`.
7. **websearch/webfetch are out** of draft 1 (the engine ships neither; deferred
   per `ARCHITECTURE.md` §11). Toolset is bash/read/write/edit/glob/grep only.

## Source facts (verified during planning)

- **Chassis source:** `../ledger` (resolves to repo-root `ledger/`): module
  `ledger`, dep `modernc.org/sqlite`. Packages `db ids logging mcp server`,
  `cmd/ledger/main.go`, `bin/{build,deploy,setup,start,stop}`,
  `etc/{deploy.env,manifest.env,nginx.conf}`, `Makefile`. Tests: `db_test.go`,
  `server_test.go`. PORT 3002, MOUNT `/srv/ledger/`.
- **Engine source:** `~/projects/ikigai-cli/app-root/internal/`, module
  `github.com/ai4mgreenly/ikigai-cli`. Self-contained closure:
  `provider(+anthropic,+google,+openai) agent tools(+bash,edit,glob,grep,read,write)
  wire model scope schema trace`. **No non-Go data files.**
- **`agent.Run(ctx, client, sess, req, sch, tracer)`** terminates on a non-tool
  stop by parsing the final text as JSON and validating against `sch` (retry 3×).
  `agent.FramingPrompt` instructs the model to emit a bare JSON value. **Both must
  change** (decision 3).
- **`tools.Dispatch(ctx, block)`** is the single dispatch seam but currently
  **drops `ctx`** and takes **no sandbox root**. `bash` already uses
  `exec.CommandContext(ctx,…)` and honors `Cmd.Dir`; `glob`/`grep` default their
  search root to `os.Getwd()`. Confinement retrofit = thread `ctx`+`root` through
  `Dispatch`; set `cmd.Dir=root` for bash; inject `root` for glob/grep; validate
  read/write/edit paths resolve under `root`.
- **`nginx/`** dev front door (`run` includes `locations/*.conf`); clone
  `locations/ledger.conf` → `locations/agent.conf`.

---

## Phase 0 — Recon & copy/adapt manifest

**Goal:** produce the exact, compile-validated manifest the later phases consume —
no code changes yet.

**Subagent work:**
- Read `ikigai-cli` engine packages; produce the **copy manifest**: every file to
  copy into `agent/internal/engine/`, the import-rewrite map
  (`github.com/ai4mgreenly/ikigai-cli/internal/… → agent/internal/engine/…`), and
  the **support-package closure** (confirm `schema scope trace` are needed; flag
  anything else `agent.Run`/`tools`/`provider`/`wire`/`model` transitively pull).
- **Pin the provider factory:** find what imports `provider/google` + `openai`,
  and specify the minimal anthropic-only edit.
- Enumerate the exact **adaptation points** in `loop.go` / `prompt.go` /
  `tools/dispatch.go` (+ bash/glob/grep) with line refs.
- Read `ledger` chassis; produce the **rename map** (module, `cmd/`, env prefix,
  port, mount, tool prefix, db name, `/opt`, systemd unit) and the file inventory.

**Output artifact:** `agent/.plan/manifest.md` (copy list, import map, closure,
adaptation points, rename map).

**Gate:** manifest reviewed by the orchestrator for completeness (no build yet).

## Phase 1 — Chassis clone + rename

**Goal:** agent stands up as a renamed ledger: builds, boots, `agent_whoami` works.

**Subagent work (per the Phase 0 rename map):** copy `ledger/`'s code into
`agent/` (merging with the existing docs/`.envrc`); rename module `ledger`→`agent`,
`cmd/ledger`→`cmd/agent`, `LEDGER_`→`AGENT_`, port `3002`→`3004`,
`/srv/ledger/`→`/srv/agent/`, tool prefix `ledger_`→`agent_`, `ledger.db`→`agent.db`,
`/opt/ledger`→`/opt/agent`, systemd unit `ledger`→`agent`. Update
`etc/{manifest.env,deploy.env,nginx.conf}`, `Makefile`, `bin/*`. Add `agent` to the
root `go.work`. Keep `db_test.go`/`server_test.go` green.

**Gate:** `cd agent && go build ./... && go test ./...` green; binary boots on
`:3004`; `GET /whoami` + `tools/list` over `/mcp` returns `agent_whoami`.

## Phase 2 — Engine copy-in (verbatim, compiling)

**Goal:** the borrowed engine lives under `agent/internal/engine/` and compiles
in-tree, **before** any behavior change.

**Subagent work:** copy the Phase 0 manifest's files into `internal/engine/`;
apply the import-rewrite map mechanically; **drop `google/`+`openai/`** and apply
the anthropic-only factory edit; resolve the support closure until it compiles.
No logic changes yet (prompt/loop/dispatch untouched in this phase).

**Gate:** `go build ./...` green (engine compiles under `agent`); any copied
engine tests green.

## Phase 3 — Engine adaptation (freeform loop + framing + confinement seam)

**Goal:** the engine behaves the way agent needs.

**Subagent work:**
- `loop.go`: make `sch` optional; add the **freeform terminal mode** (non-tool
  stop → emit final assistant text as the result event; skip JSON parse/validate
  when no schema). Keep usage/turn accounting and signed-thinking round-trip.
- `prompt.go`: replace `FramingPrompt` with a **agent framing prompt** — agent
  works inside a persistent folder that is its only durable memory; toolset is
  bash/read/write/edit/glob/grep confined to the folder; **no network from bash**;
  leave deliverables as files; prior runs' files are readable (the Ralph pattern).
- `tools/dispatch.go` (+ bash/glob/grep): thread `ctx` + `sandboxRoot` through
  `Dispatch`; bash `cmd.Dir=root` (ctx already honored); glob/grep search root =
  `root`; read/write/edit validate the resolved path is under `root`.

**Tests (assumption-verifying):** freeform stop emits text (no JSON required);
bash honors `ctx` cancellation; **path confinement** — `..` escape, absolute path
outside root, symlink escape are all rejected; bash runs with `cwd==root`.

**Gate:** `go build ./... && go test ./...` green.

## Phase 4 — Data model + migration

**Goal:** the `sessions` + `runs` schema exists and migrates.

**Subagent work:** add `internal/db/migrations/002_agent.sql` exactly per
`ARCHITECTURE.md` §4 (`sessions`, `runs`, `idx_runs_session`). Create the `data/`
layout convention (`agent.db`, `sandboxes/<id>/`, `runs/<id>/<run>.jsonl`).

**Gate:** migration runs idempotently on a fresh DB; `go build/test ./...` green.

## Phase 5 — `sandbox` package

**Goal:** the confined per-session folder + its read surface, integrated with the
Phase 3 engine confinement helpers.

**Subagent work (`internal/sandbox/`):** `Create(id)`, `List(path)`,
`Read(path,offset,limit)` over `data/sandboxes/<id>/`, all rejecting path escape
(clean + resolved-root check). Expose the confinement root the engine's `Dispatch`
consumes (single source of truth for "what is the root for session X").

**Tests:** path-escape rejection is the priority — `..`, absolute, symlink,
non-existent; `List`/`Read` happy paths; not-a-file error.

**Gate:** `go build/test ./...` green.

## Phase 6 — `session` package (domain service)

**Goal:** the only thing the MCP handler talks to; the only mutator of
session/run state.

**Subagent work (`internal/session/`):** `model.go` (Session, Run, Config, status
enums), `store.go` (SQLite queries), `service.go` (Create/List/Get/Update/Delete/
Run/Cancel per `ARCHITECTURE.md` §5.1). Create validates config
(`model.Resolve`, provider==anthropic, `ANTHROPIC_API_KEY` present) and calls
`sandbox.Create`. `Run` is the **single-flight gate** (busy if running). Update/
Delete rejected while running. All reads owner-scoped.

**Tests:** single-flight (`Run` while running → busy); config validation
(unknown model, missing key); Update/Delete-while-running rejection; owner-scoping.

**Gate:** `go build/test ./...` green.

## Phase 7 — `runner` package (async run lifecycle)

**Goal:** spawn the run, drive the adapted engine, sink output, recover on crash.

**Subagent work (`internal/runner/`):** `Spawn(session,sandbox,run)` →
goroutine: `context.WithTimeout(parent, AGENT_RUN_TTL)`; open
`data/runs/<id>/<run>.jsonl` as a `wire.Session` sink; build the Anthropic client
(`os.Getenv("ANTHROPIC_API_KEY")`) + `provider.Request` (agent framing + prompt +
confined toolset + model + effort→params); call adapted `agent.Run`; on exit write
terminal `status`/`ended_at`/`usage_json`/`error` and flip session→`idle`. Keep an
in-memory `session_id → cancel` map for `Cancel`. **Crash-recovery sweep**
(`runs WHERE status='running'` → failed, sessions → idle), invoked at boot.

**Tests:** crash-recovery sweep marks orphaned runs failed; `Cancel` cancels the
ctx; TTL fires; terminal state written. (Use a fake `provider.Client` — no real
API in unit tests.)

**Gate:** `go build/test ./...` green.

## Phase 8 — MCP surface (11 tools)

**Goal:** the full `agent_*` tool surface wired to the service.

**Subagent work (`internal/mcp/`):** `Handler{ svc *session.Service }`;
`toolDescriptors()` lists all 11 tools (`ARCHITECTURE.md` §7) with input schemas;
`dispatchTool` routes each `agent_*` name to a `Service` method (and
`session_output` → `runs.log_path` line-slice; `fs_list`/`fs_read` →
`sandbox`), marshaling results to MCP content. `agent_whoami` retained.

**Tests:** `tools/list` returns 11; dispatch routes each name; error mapping
(busy, rejected-while-running, path-escape) surfaces as MCP errors.

**Gate:** `go build/test ./...` green.

## Phase 9 — Composition root wiring

**Goal:** `cmd/agent/main.go` assembles everything and boots.

**Subagent work:** after `db.Migrate`, construct `store`, sandbox root,
`runner`, `session.Service`; inject the service into the MCP handler; **run the
crash-recovery sweep**; build/serve the HTTP server with graceful shutdown. Read
`AGENT_*` config incl. `AGENT_RUN_TTL`.

**Gate:** agent boots on `:3004`; `tools/list` returns all 11; `session_create`
makes a row + an empty sandbox folder.

## Phase 10 — Lifecycle scripts (written, not box-executed)

**Goal:** the full `bin/*` set exists and is correct; dev start/stop work locally.

**Subagent work (`ARCHITECTURE.md` §8):** `bin/{setup,deploy,start,stop,secrets,
backup,restore,teardown}` + `etc` updates; `setup` adds the idempotent `python3`
provisioning; `secrets` does the non-destructive RMW of agent's
`ANTHROPIC_API_KEY` key in SSM app-config; `backup`/`restore` `aws s3 sync` of
`data/` with a consistent DB snapshot; `start`/`stop` add the soft `python3`
preflight. Document the `DASHBOARD_RESOURCES` registration step (not executed).

**Gate:** `shellcheck` clean (or reviewed); `bin/start`/`bin/stop` drive agent
locally; **no box commands run.**

## Phase 11 — Local end-to-end (the finish line)

**Goal:** the fusion example, proven locally with a real Anthropic run.

**Subagent work (driven by the orchestrator):** add `nginx/locations/agent.conf`
(clone of `ledger.conf`); start dashboard (:3000) + agent (:3004) + nginx (:8080);
register agent in the dashboard's `DASHBOARD_RESOURCES` for local dev; run the
flow: `agent_session_create {prompt, config}` → `agent_session_run` → poll
`agent_session_get.last_run` until terminal → `agent_session_output` (the narrated
jsonl) → `agent_session_fs_list` / `agent_session_fs_read` the deliverable the
agent wrote. `.envrc` supplies `ANTHROPIC_API_KEY` (run `direnv allow agent/`).

**Gate (definition of done):** a session runs end-to-end, the run reaches
`succeeded`, the output log is readable, and a real file the agent wrote is
read back through the sandbox read surface — all through nginx, not loopback
shortcuts. Box deploy remains a separate manual step.

---

## Out of scope (deferred — `ARCHITECTURE.md` §11)

Real sandbox isolation (bwrap/podman, network isolation), websearch/webfetch,
triggers beyond run-once, the event plane, push delivery, idle-network watchdog,
run-history (`run_id`) browsing, and **executing** a real box deploy.
