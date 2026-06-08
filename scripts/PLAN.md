# scripts — implementation plan (orchestrated, parallel-subagent build)

**Status:** design settled (`README.md` + `ARCHITECTURE.md`); this is the build
plan. It is written for an **orchestrator** that dispatches **subagents**, each
completing one phase. Phases are sized for a single subagent and partitioned so
that agents in the same wave **own disjoint files** and never coordinate at
runtime — they coordinate only through the **frozen contracts in Part A**.

How to read this:
- **Part 0** — drift corrections the whole build depends on. Read first.
- **Part A** — the frozen shared contracts (data model, signatures, payloads,
  layout, env). Every subagent reads Part A and codes against it verbatim. These
  do not change during the build; if one *must* change, it goes back through the
  orchestrator, not agent-to-agent.
- **Part B** — the phases. Each phase lists: goal, wave, files **owned**, files
  **read-only**, inputs (contracts honored), steps, acceptance, and a hard
  "do not touch" list.
- **Part C** — the wave/dependency graph and the final integration gate.

All paths are in the `scripts` worktree: `/mnt/projects/ikigai/scripts/scripts/`
(the service dir). The clone source is read-only at
`/mnt/projects/ikigai/main/prompts/`. Never `cd`; drive git with
`git -C /mnt/projects/ikigai ...`. Never edit under `/mnt/projects/ikigai/main`.

---

## Part 0 — Drift corrections (MUST apply; the design docs predate these)

1. **The clone base is `prompts/`, not `agent/`.** The service the design docs
   call `agent` (port 3004, runner + triggers + outcome events) has been renamed
   to **`prompts`** (module `prompts`, tool prefix `ikigenba_prompts_`,
   `mcp__ikigenba_prompts__*`). Everywhere `ARCHITECTURE.md`/`README.md` say
   "clone from agent", read **"clone from `prompts`"**.

2. **`scripts` is NOT an LLM service — strip everything LLM.** `prompts` depends
   on `agentkit` + the Anthropic provider, has an `internal/sandbox/` package, a
   persistent per-session sandbox tree, a `bin/secrets`, and an
   `ANTHROPIC_API_KEY`. `scripts` has **none** of these. During the clone:
   - Drop the `agentkit` require + replace from `go.mod`.
   - Delete `internal/sandbox/` entirely (scripts has **no** persistent
     per-script sandbox; it uses a per-run `data/runs/<run_id>/` tree, owned by
     the runner and **persisted** — see Part A §A4 and Part 0 §7B).
   - Delete `bin/secrets`. Remove any `ANTHROPIC_API_KEY` read/seed.
   - The runner replaces the `agentkit/agent.Run(...)` loop with a `python3`
     subprocess exec (Part A §A6).
   - Remove the old **session-scoped** `fs_list`/`fs_read` MCP tools (they read
     the persistent sandbox, which is gone). scripts instead exposes
     **run-scoped** `run_fs_list`/`run_fs_read` over the persisted run dir (Part 0
     §7D), plus `run_output` over the run logs.

3. **`scripts` is a producer AND a multi-upstream consumer.** `prompts` consumes
   **only** `cron` (one subscription, one worker). `scripts` consumes **five**
   upstreams (`cron`, `crm`, `ledger`, `dropbox`, `agent`/`prompts` — see note
   below) and is also a producer. So: `Feed: "/feed"`, a static `Events`
   registry, a `Producer` hook, `Consumes: [...5...]`, a `Subscriptions`
   provider returning N subs, and **N consumer workers** (the `notify`
   multi-cursor pattern). Confirm `notify/` for the multi-upstream shape.

4. **The five upstreams are RESOLVED** (`agent` is now `prompts`; there is no
   `agent/` dir). All five candidates are live producers (`Feed:"/feed"`). Use
   these exact source ids / ports / type sets — no hardcoded "agent":

   | source id | port | event types published |
   |---|---|---|
   | `cron`    | 3007 | `cron.<name>` (DYNAMIC — `cron.` prefix, names from crontab) |
   | `crm`     | 3001 | `contact.created`, `contact.updated`, `contact.tagged`, `contact.untagged` |
   | `ledger`  | 3002 | `transaction.recorded` |
   | `dropbox` | 3005 | `file.created`, `file.modified`, `file.deleted` |
   | `prompts` | 3004 | `run.succeeded`, `run.failed` (dual producer+consumer) |

   `prompts` (the renamed `agent`) IS a producer, so it stays in scripts' day-one
   upstream set. The source string is `appkit.Spec.App` for every service; the
   feed URL env var per upstream is `SCRIPTS_<SRC>_FEED_URL` with the loopback
   default `http://127.0.0.1:<port>/feed` (Part A §A11). The README's
   `{agent, "run.failed"}` example → `{prompts, "run.failed"}`.

5. **Python3.11 provisioning: scripts owns it independently via a NEW opsctl
   hook.** Findings: services have **no `bin/setup`** anymore; box provisioning is
   `opsctl init-box` (once per box: installs `nginx`+`certbot`) and
   `opsctl setup <app>` (once per service: app user, `/opt/<app>` tree, systemd
   unit, nginx fragment). `opsctl setup` currently installs **no** OS packages.
   **Nothing installs python today** — `prompts/bin/start` only soft-*warns* if
   `python3` is missing, and `prompts/PLAN.md`'s claim that its `setup` installs
   python3 was never wired. Per the user's call, `scripts` must request its own
   `python3.11` so it is present whether or not `prompts` is deployed (idempotent
   `dnf` ⇒ both services may declare it, neither depends on the other). The hook
   does not exist yet, so **Phase 8 adds it**: a `Packages []string` field on
   `opsctl`'s `SetupOptions` + an `InstallPackages(ctx, opts.Packages...)` call in
   `opsctl/internal/opsctl/setup.go` (modeled on the `initbox.go:58`
   `InstallPackages(ctx,"nginx","certbot")` call), wired through `runSetup` in
   `opsctl/cmd/opsctl/main.go` as a `--packages` flag; `scripts`' deploy path then
   passes `python3.11`. **Do not invent a service `bin/setup`.** (The same hook
   later lets `prompts` declare `python3` too — out of scope here, just noted.)

6. **Migration numbering follows the chassis convention.** The clone brings
   `001_schema_migrations.sql`. `prompts` then has `002_prompts`,
   `003_session_triggers`, `004_feed_offset`, `005_outbox`. scripts mirrors that
   ordering (domain first, then feed_offset, then outbox) — see Part A §A3. The
   `ARCHITECTURE.md` numbering (`002/003/004`) is indicative; match the chassis
   ordering actually present after the clone.

7. **Coin reconciliation with `prompts` REDESIGN-DECISIONS (newer; wins on
   conflict).** `prompts` and `scripts` are "two sides of one coin" — identical
   in surface, lifecycle, full concurrency, materialize→exec→persist, and
   tombstone-delete. Four settled deltas amend the contracts below; apply them
   *before* Wave 1 freezes Part A:
   - **(A) Delete is a tombstone for runs.** Drop `ON DELETE CASCADE` on
     `runs.script_id`; delete removes the script row only — runs + their on-disk
     artifacts survive as append-only history (suite grain: ledger journal,
     append-only outbox). **Triggers still cascade** (a trigger is live
     definition, not history; a tombstoned script must not keep a forward-looking
     binding). See §A3/§A5/§A6/§A8.
   - **(B) Persist the executed body+config per run.** No ephemeral `work/` dir.
     Materialize `body` + pinned `config` into the persistent `runs/<run_id>/`
     tree and exec from there; the runner reads inputs from disk, not the live DB
     row, so a finished run shows exactly what it ran. See §A4/§A6.
   - **(D) scripts gains `run_fs_list` / `run_fs_read`.** Because B makes the run
     dir persistent, a script that writes files leaves a readable tree — expose
     it (run-scoped, keyed by `run_id`). stdout/stderr are still auto-captured to
     logs and read via `run_output`. This **dissolves** former coin-difference #3
     (now both services have `run_fs_*`); only the input field (#1) and executor
     (#2) differ. Tool count 14 → **16**. See §A8/§A10.
   - **(E) Completion verb aligns to the status enum.** `scripts.completed` →
     **`scripts.succeeded`** (matches the `RunSucceeded` enum and prompts'
     `run.succeeded`). `scripts.failed` unchanged. See §A7/§A12.
   - **Not a scripts edit — implied `prompts` work:** `prompts` grows from
     cron-only to scripts' **general multi-source trigger** model (§A9/§A12).
     Committed prompts backlog; does not block this build. (The "prompts is
     cron-only today" premise is from the REDESIGN doc, not yet verified against
     prompts code.)

---

## Part A — Frozen shared contracts (every subagent codes against these verbatim)

These are the interfaces that let agents work in parallel without talking. An
agent implementing `service.go` codes against the **store** and **runner**
signatures here; it does not need to read those agents' code, only honor these.

### A1. Module & identity

| thing | value |
|---|---|
| Go module | `scripts` |
| command | `cmd/scripts/main.go` |
| port | **3009** |
| mount | `/srv/scripts/` |
| db file | `scripts.db` (`SCRIPTS_DB_PATH`, default `./tmp/scripts.db`) |
| MCP tool prefix | `ikigenba_scripts_` |
| consumer id (X-Consumer-Id) | `scripts` |
| env prefix | `SCRIPTS_` |
| `go.work` | add `./scripts` to `/mnt/projects/ikigai/go.work` use-block |
| go.mod requires | `appkit`, `eventplane`, `modernc.org/sqlite` — **no `agentkit`** |
| go.mod replaces | `appkit => ../appkit`, `eventplane => ../eventplane` |

### A2. Domain types — `internal/script/model.go` (the canonical type contract)

```go
package script

// Run status values.
const (
    RunRunning   = "running"
    RunSucceeded = "succeeded"
    RunFailed    = "failed"
    RunCancelled = "cancelled"
)

type Config struct {
    // Day-one minimal. Reserved for later; validated/normalized at create.
    Interpreter string `json:"interpreter,omitempty"` // reserved; "python3" only day-one
    TimeoutSecs int    `json:"timeout_secs,omitempty"` // reserved; SCRIPTS_RUN_TTL is the backstop
}

type Script struct {
    ID, OwnerEmail, Name, Body string
    Config                     Config
    CreatedAt, UpdatedAt       string
}

type Run struct {
    ID, ScriptID, Status string
    ExitCode             *int   // nil while running / never-started
    StartedAt            string
    EndedAt              string // "" while running
    Error                string // failure / TTL / spawn reason
    TriggerSource        string // "" for a manual run
    TriggerType          string
    TriggerEventID       string
    StdoutPath, StderrPath string
    ElapsedSecs          int    // computed in service, not stored
}

type Trigger struct {
    ScriptID, Source, EventFilter, CreatedAt string
}

// FileEntry is one node in a run's persisted dir tree (run_fs_list).
type FileEntry struct {
    Path  string `json:"path"`  // relative to the run dir root
    IsDir bool   `json:"is_dir"`
    Size  int64  `json:"size"`  // bytes; 0 for dirs
}

type ScriptDetail struct {
    Script
    RunningCount int  // derived: COUNT(runs WHERE status='running')
    LastRun      *Run
}

// Sentinel errors. ErrNotFound on missing/foreign owner.
var (
    ErrNotFound  = errors.New("script: not found")
    ErrValidation = errors.New("script: validation")
)
```

### A3. Schema — `internal/db/migrations/`

After the clone, **replace `prompts`' domain migrations** with scripts' three.
Final migration set:

- `001_schema_migrations.sql` — from clone, unchanged.
- `002_scripts.sql` — domain: `scripts`, `runs`, `script_triggers` (verbatim DDL
  from `ARCHITECTURE.md §4`). Key points: `runs.script_id` `REFERENCES
  scripts(id)` with **NO `ON DELETE CASCADE`** (tombstone — runs are append-only
  history, §7A); `script_triggers.script_id REFERENCES scripts(id) ON DELETE
  CASCADE` (triggers are live definition, cascade with the script);
  `script_triggers` PK is `(script_id, source, event_filter)`; indexes
  `idx_runs_script(script_id, started_at)`, `idx_runs_status(status)`,
  `idx_script_triggers_source(source)`. **No `status` column on `scripts`.**
- `003_feed_offset.sql` — `eventplane/consumer` cursor table
  (`consumer.SchemaSQL`), one row per upstream. Copy byte-for-byte from the
  consumer library (as `prompts/004_feed_offset.sql` does).
- `004_outbox.sql` — `eventplane/outbox` producer table (`outbox.SchemaSQL`),
  byte-identical to the library constant (as `prompts/005_outbox.sql` does).

### A4. On-disk layout (owned by the runner; service reads paths from `Run`)

```
<dataDir>/                         # dataDir = dirname(SCRIPTS_DB_PATH)/data
└── runs/<run_id>/                 # PERSISTENT — created at spawn, never deleted (retention deferred)
    ├── main.py                    #   materialized body — the exact source this run executed (§7B)
    ├── config.json                #   pinned config for this run
    ├── stdout.log  stderr.log     #   captured streams (run_output reads these)
    └── <files the script wrote>   #   produced artifacts (run_fs_list/run_fs_read read the tree, §7D)
```

There is **no ephemeral `work/` dir** and **no `sandboxes/` dir** (that was
prompts). The run dir is the runner's working directory (`cmd.Dir`) and is
persisted whole; `Run.StdoutPath`/`StderrPath` are
`runs/<run_id>/{stdout,stderr}.log`.

### A5. `script.Store` — the persistence contract (`internal/script/store.go`)

```go
type Store struct {
    db     *sql.DB
    Outbox *outbox.Outbox // injected by Producer hook; may be nil in unit tests
}
func NewStore(db *sql.DB) *Store

// Script CRUD (owner-scoped).
func (s *Store) InsertScript(ctx context.Context, sc Script) error
func (s *Store) GetScript(ctx context.Context, owner, id string) (Script, error) // ErrNotFound
func (s *Store) UpdateScript(ctx context.Context, owner string, sc Script) error
func (s *Store) DeleteScript(ctx context.Context, owner, id string) error        // TOMBSTONE: deletes script row + triggers (FK cascade); runs + on-disk artifacts SURVIVE as history
func (s *Store) ListScripts(ctx context.Context, owner string) ([]Script, error)
func (s *Store) RunningCount(ctx context.Context, scriptID string) (int, error)  // COUNT status='running'
func (s *Store) LastRun(ctx context.Context, scriptID string) (*Run, error)

// Runs.
func (s *Store) InsertRun(ctx context.Context, r Run) error
func (s *Store) GetRun(ctx context.Context, owner, runID string) (Run, error)    // joins scripts for owner scope
func (s *Store) ListRuns(ctx context.Context, owner, scriptID, status string) ([]Run, error) // scriptID/status "" = no filter
func (s *Store) SweepRunning(ctx context.Context) (ids []string, err error)      // crash recovery: flip running->failed, return swept run_ids

// FinishRun is the ATOMIC terminal write + completion-event emit (see A7).
func (s *Store) FinishRun(ctx context.Context, in FinishRunInput) error

// Triggers.
func (s *Store) SetTrigger(ctx context.Context, owner string, t Trigger) error   // ErrNotFound if script not owner's
func (s *Store) ClearTrigger(ctx context.Context, owner string, t Trigger) error
func (s *Store) ScriptsForEvent(ctx context.Context, source, eventType string) ([]string, error) // glob-match event_filter; NOT owner-scoped; returns script_ids

type FinishRunInput struct {
    RunID, ScriptID, ScriptName string
    Status      string // succeeded|failed (NEVER cancelled — cancelled emits no event, see A7)
    ExitCode    *int
    EndedAt     string
    ErrMsg      string
    Trigger     Trigger // source/type/event_id for the payload; empty for manual
    StdoutTail  string  // last 8KB, already read+truncated by runner
    StderrTail  string
    StdoutTrunc bool
    StderrTrunc bool
}
```

`FinishRun` transaction (mirror `prompts` `session.Store.FinishRun`): `BEGIN` →
`UPDATE runs SET status,exit_code,ended_at,error WHERE id=?` → if `Outbox != nil`
**and status != cancelled** `outbox.Append(tx, completionEvent)` → `COMMIT` →
`Outbox.Ring()`. The event is built by `outcome.go` (A7).

### A6. `runner` — the python3-exec lifecycle (`internal/runner/`)

```go
type Runner struct { /* store, dataDir, ttl, mu, cancels map[string]context.CancelFunc, userCancelled map[string]bool */ }
func New(store *script.Store, dataDir string, ttl time.Duration) *Runner

// Spawn starts a goroutine and returns immediately. input is the event bytes
// ("{}" for a manual run). The goroutine owns the full lifecycle in A6-steps.
func (r *Runner) Spawn(run script.Run, input []byte)

// Cancel marks run_id user-cancelled and cancels its context (kills the process
// group). Returns false if the run_id is not in flight. Terminal status =
// cancelled, NO event.
func (r *Runner) Cancel(runID string) bool

// Recover sweeps running->failed on boot (delegates SweepRunning). Returns count
// swept. It does NOT delete run dirs — they are persistent history (§7A/§7B); a
// crashed run keeps whatever partial tree it had.
func (r *Runner) Recover(ctx context.Context) (int, error)
```

Goroutine steps (from `ARCHITECTURE.md §5.2`): timeout ctx from `ttl`; register
cancel keyed by **run_id**; create `runs/<run_id>/`; materialize `body` →
`runs/<run_id>/main.py` and pinned config → `runs/<run_id>/config.json` (§7B);
open `runs/<run_id>/{stdout,stderr}.log`; `cmd = python3 main.py`,
`cmd.Dir=runs/<run_id>/`, `cmd.Env += EVENT_JSON=<input>`, `cmd.Stdin=<input>`,
stdout/stderr → log files; **own process group** (`Setpgid`, kill `-pgid` on
cancel); `cmd.Run()`; classify terminal status (userCancelled → cancelled; ctx
deadline → failed "run TTL exceeded"; non-zero → failed; spawn error → failed;
else succeeded); read 8KB tails; call `store.FinishRun` (which emits the event
except for cancelled). **The run dir is NOT deleted** — body, config, logs, and
any produced files persist as the record of the run. The runner reads the
`python3` binary name as a constant day-one.

### A7. Completion events — `internal/script/outcome.go`

```go
const (
    EventSucceeded = "scripts.succeeded" // exit 0
    EventFailed    = "scripts.failed"    // non-zero / TTL / spawn error
)
var Events outbox.Registry // {EventSucceeded,...}, {EventFailed,...} with Sample payloads

// completionEvent builds the outbox.Event from a FinishRunInput. Returns
// (event, shouldEmit). shouldEmit=false ONLY for status==cancelled.
func completionEvent(in FinishRunInput) (outbox.Event, bool, error)
```

Payload shape is `README.md`'s JSON block verbatim: `script_id`, `script_name`,
`run_id`, `status`, `exit_code`, `trigger{source,type,event_id}`, `stdout`,
`stdout_truncated`, `stderr`, `stderr_truncated`, `error` (omitempty; only on
failed). For a manual run the `trigger` fields are empty.

### A8. `script.Service` — the only thing MCP talks to (`internal/script/service.go`)

```go
type Runner interface {  // runner.Runner satisfies this; service depends on the interface
    Spawn(run script.Run, input []byte)
    Cancel(runID string) bool
}
type Service struct { /* store *Store; runner Runner; dataDir string */ }
func NewService(store *Store, runsDir string, runner Runner) *Service

func (s *Service) Create(ctx, owner string, in CreateInput) (Script, error)
func (s *Service) Update(ctx, owner, id string, in UpdateInput) (Script, error)
func (s *Service) Delete(ctx, owner, id string) error
func (s *Service) List(ctx, owner string) ([]ScriptDetail, error)   // each w/ RunningCount,LastRun
func (s *Service) Get(ctx, owner, id string) (ScriptDetail, error)

func (s *Service) Run(ctx, owner, scriptID string) (Run, error)     // insert running run (empty trigger), Spawn({}), return run; ALWAYS accepted
func (s *Service) RunForEvent(ctx, scriptID, source, evType, eventID string, payload []byte) error // consumer path; NOT owner-scoped

func (s *Service) RunList(ctx, owner, scriptID, status string) ([]Run, error) // w/ ElapsedSecs
func (s *Service) RunGet(ctx, owner, runID string) (Run, error)               // w/ ElapsedSecs
func (s *Service) RunOutput(ctx, owner, runID, stream string, offset, limit int) (string, error) // line-slice logs; stream stdout|stderr|both
func (s *Service) RunCancel(ctx, owner, runID string) error                   // verify owner, runner.Cancel(runID)
func (s *Service) RunFsList(ctx, owner, runID, subpath string) ([]FileEntry, error)        // list the run's persisted dir tree (run-scoped); owner-verified
func (s *Service) RunFsRead(ctx, owner, runID, path string, offset, limit int) (string, error) // read one file in the run dir; path-traversal guarded to the run root

func (s *Service) SetTrigger(ctx, owner, scriptID, source, eventFilter string) (Trigger, error) // validates source/type vs known producers
func (s *Service) ClearTrigger(ctx, owner, scriptID, source, eventFilter string) error
func (s *Service) ScriptsForEvent(ctx, source, evType string) ([]string, error) // delegates store; consumer uses it
func (s *Service) TriggerSources() []string // the static known-producer set, for set_trigger validation

type CreateInput struct { Name, Body string; Config Config }
type UpdateInput struct { Name, Body *string; Config *Config }
```

`SetTrigger` validates `source` is in the static consumable set (A1/§A9) and the
`event_filter` is plausible for that producer's published registry; rejects the
unsatisfiable with `ErrValidation`.

### A9. `consume` — multi-upstream fan-in (`internal/consume/`)

```go
// One Subscription per upstream; Filter "*" (scripts fires on any type and
// decides via ScriptsForEvent). Source is the producer's source id.
func Subscriptions(sources []string) []consumer.Subscription

// Handler: per event -> ScriptsForEvent(source, ev.Type) -> RunForEvent per
// script on a goroutine (unbounded, non-blocking). Returns nil for matched
// (fire-and-forget, never stall); ErrSkip only on a malformed envelope. Cursor
// advances for every event.
func Handler(fire FireFunc, lookup LookupFunc, source string, logger *slog.Logger) consumer.Handler

type FireFunc   func(ctx context.Context, scriptID, source, evType, eventID string, payload []byte) error
type LookupFunc func(ctx context.Context, source, evType string) ([]string, error)
```

### A10. MCP tool surface — `internal/mcp/tools.go` (16 tools, prefix `ikigenba_scripts_`)

Map verbatim from `ARCHITECTURE.md §6` / `README.md` "MCP tool surface". Each
tool → one `Service` method. `health` returns the chassis envelope with `details`
= the **static runtime contract** (`python_version:">=3.11"`, `bash_version:
">=5.0"`, `network:true`, `packages:"stdlib"`). `describe` returns the authoring
contract markdown. Tools: `health`, `describe`, `create`, `list`, `get`,
`update`, `delete`, `set_trigger`, `clear_trigger`, `run`, `run_list`,
`run_get`, `run_output`, `run_cancel`, `run_fs_list`, `run_fs_read`. The two
`run_fs_*` tools are **run-scoped** (keyed by `run_id`) readers of the persisted
run dir (§7D) — **not** the old session-scoped `fs_list`/`fs_read`, which are
removed.

### A11. Env / config (`SCRIPTS_*`, composed by the deploy wrapper — no secrets)

`SCRIPTS_DB_PATH` (default `./tmp/scripts.db`), `SCRIPTS_RUN_TTL` (Go duration,
default `30m`), and one `SCRIPTS_<SRC>_FEED_URL` per consumed upstream with the
loopback dev fallback (mirror `prompts`' `config.EnvOr` pattern):

| env var | dev fallback |
|---|---|
| `SCRIPTS_CRON_FEED_URL`    | `http://127.0.0.1:3007/feed` |
| `SCRIPTS_CRM_FEED_URL`     | `http://127.0.0.1:3001/feed` |
| `SCRIPTS_LEDGER_FEED_URL`  | `http://127.0.0.1:3002/feed` |
| `SCRIPTS_DROPBOX_FEED_URL` | `http://127.0.0.1:3005/feed` |
| `SCRIPTS_PROMPTS_FEED_URL` | `http://127.0.0.1:3004/feed` |

Each upstream also takes a `SCRIPTS_<SRC>_FROM` (default `tail`). `ManifestExtras`:
`OUTBOX_RETENTION_DAYS=7`, `OUTBOX_RETENTION_MAX_ROWS=1000000`, plus `CONSUMES`
listing the five sources. **No `ANTHROPIC_API_KEY`, no `bin/secrets`.**

### A12. Known producer registries (for `SetTrigger` validation — §A8)

`SetTrigger` validates `(source, event_filter)` against this static map (the
glob in `event_filter` must be able to match at least one type, except `cron`
which is dynamic and accepts any `cron.*`):

```
cron    -> dynamic; accept event_filter matching "cron.*"
crm     -> contact.created | contact.updated | contact.tagged | contact.untagged
ledger  -> transaction.recorded
dropbox -> file.created | file.modified | file.deleted
prompts -> run.succeeded | run.failed
scripts -> scripts.succeeded | scripts.failed   (self-chaining; scripts' OWN /feed)
```

Note `scripts` may subscribe to **its own** feed (`{scripts, "scripts.succeeded"}`)
for chaining — so its consumer set is the five upstreams **plus optionally
itself**; day-one, keep the five external upstreams and treat self-chaining as a
fast-follow only if trivial (it adds a sixth consumer loop pointed at the local
`/feed`). Phase 1 wires the five; flag self-chaining as a one-line TODO.

---

## Part B — Phases

### Wave 1 — foundation (1 subagent, BLOCKING; everything else waits on it)

#### Phase 1 — Scaffold, rename, strip, and freeze the skeleton

**Goal:** produce a **compiling** `scripts` service that is `prompts` cloned,
fully renamed, with all LLM parts stripped, all migrations in place, and **every
type/method signature from Part A present as a stub** (empty body returning
`ErrNotFound`/`panic("todo: <phase>")`). `go build ./...` and `go vet ./...`
pass. No real logic yet — this phase exists so Wave-2 agents have a frozen,
compiling surface to fill in disjoint files.

**Owns (creates):** the entire `scripts/` tree —
`cmd/scripts/main.go`, `go.mod`, `Makefile`, `etc/`, `internal/db/` (+ the four
migrations per A3), `internal/ids/`, `internal/logging/` (if present in clone),
`internal/server/` (if present), `internal/mcp/{mcp.go, tools.go (stub
descriptors), describe.go}`, `internal/script/{model.go, store.go, service.go,
trigger.go, outcome.go}`, `internal/runner/runner.go`, `internal/consume/
consume.go`, `bin/{backup,restore,start,stop,teardown}`, `VERSION` (set `0.1.0`).
Also edits `/mnt/projects/ikigai/go.work` to add `./scripts`.

**Read-only:** all of `/mnt/projects/ikigai/main/prompts/` and `/notify/`,
`/mnt/projects/ikigai/main/{appkit,eventplane}/`, and this PLAN.

**Steps:**
1. Copy `prompts/` → `scripts/`. Apply the rename map (`ARCHITECTURE.md §1`):
   module/import paths `prompts`→`scripts`, `cmd/prompts`→`cmd/scripts`, env
   `PROMPTS_`→`SCRIPTS_`, port `3004`→`3009`, mount, tool prefix, db name, the
   `prompts/internal/session` package → `scripts/internal/script` (note the
   **package rename** `session`→`script`).
2. Apply **all Part 0 strips**: drop `agentkit` from go.mod; delete
   `internal/sandbox/`; delete `bin/secrets`; remove `ANTHROPIC_API_KEY` and the
   old session-scoped `fs_list`/`fs_read` tools + their service methods. (The new
   run-scoped `run_fs_list`/`run_fs_read` are added as stubs per Part A §A8/§A10,
   filled in Phase 7.)
3. Replace the domain types in `model.go` with **Part A §A2** verbatim. Replace
   `store.go`/`service.go`/`trigger.go`/`outcome.go`/`runner.go`/`consume.go`
   public surfaces with the **Part A signatures**, bodies stubbed.
4. Replace migrations with the A3 set (keep `001`; write `002_scripts.sql` with
   the A3/ARCHITECTURE DDL; copy `feed_offset` + `outbox` library SQL).
5. Rewrite `cmd/scripts/main.go` Spec for the **producer + multi-consumer**
   shape (Part 0 §3, mirror `notify` for N workers): `Feed`, `Events =
   script.Events`, `Producer` injecting `storeRef.Outbox`, `Consumes =
   {"cron","crm","ledger","dropbox","prompts"}` (Part 0 §4 — the five resolved
   producers), `Subscriptions = consume.Subscriptions(sources)`, and **one worker
   per upstream** each running `consumer.Run` with that upstream's
   `SCRIPTS_<SRC>_FEED_URL` (A11 defaults) and `ConsumerID="scripts"`. Wire the
   crash-recovery sweep after migrate (as `prompts` does). Service construction
   drops `sandbox`; runner takes `(store, dataDir, ttl)`. Leave self-chaining
   (a sixth loop on the local `/feed`) as a one-line TODO (A12).
6. `go build ./...`, `go vet ./...` from `/mnt/projects/ikigai/scripts/scripts`
   (workspace resolves siblings). Fix until green.

**Acceptance:** `go build ./... && go vet ./...` clean. Every Part A signature
exists. `grep -ri 'agentkit\|anthropic\|sandbox\|ANTHROPIC'` over `scripts/`
returns nothing; the only fs tools present are run-scoped `run_fs_list`/
`run_fs_read` (no bare `fs_list`/`fs_read`). `./scripts version` and `./scripts
migrate` (against a temp db) run. Stubs may `panic`/return `ErrNotFound`.

---

### Wave 2 — implementations (parallel; agents own disjoint files, all build on Phase 1's frozen stubs)

Each Wave-2 agent fills bodies in **its** files only, against Part A. Because
signatures are frozen, the package keeps compiling regardless of sibling
progress. Each writes focused unit tests for its own package using fakes for
dependencies it doesn't own (e.g. service tests use a fake `Runner`; consume
tests use a fake `FireFunc`/`LookupFunc`).

#### Phase 2 — `script/store.go` (persistence)
- **Owns:** `internal/script/store.go` (+ `store_test.go`).
- **Implements:** every A5 method against SQLite (real `*sql.DB` in tests, temp
  file + the A3 migrations). The `FinishRun` atomic tx (BEGIN → update run →
  conditional `outbox.Append` → COMMIT → `Ring`) — model on
  `prompts/internal/session/store.go FinishRun`. Glob match in `ScriptsForEvent`
  (SQL `GLOB`/`LIKE` or in-Go `path.Match` over fetched filters — match how
  prompts' `TriggersForEvent` does it).
- **Acceptance:** `go test ./internal/script -run Store` green; CRUD round-trips;
  `SweepRunning` flips + returns ids; `FinishRun` writes terminal row and (with a
  fake/real outbox) appends exactly one event for succeeded/failed and **zero**
  for cancelled.

#### Phase 3 — `script/service.go` (orchestration)
- **Owns:** `internal/script/service.go` (+ `service_test.go`).
- **Implements:** A8. Owner scoping; `Run`/`RunForEvent` insert the run row then
  `runner.Spawn`; `Run*` instance ops incl. `ElapsedSecs` compute and
  `RunOutput` line-slicing; trigger ops with the A8 validation. Codes against the
  **A5 store** and **A8 Runner interface** (uses a fake runner in tests).
- **Acceptance:** `go test ./internal/script -run Service` green; `Run` always
  accepted; cancel/owner-mismatch → `ErrNotFound`; `SetTrigger` rejects unknown
  source with `ErrValidation`.

#### Phase 4 — `runner/` (python3 exec)
- **Owns:** `internal/runner/runner.go` (+ `runner_test.go`).
- **Implements:** A6 — spawn/cancel/recover, process-group kill, TTL ctx, 8KB
  tail read, run-dir materialize (body+config) and exec-from-disk, calls
  `store.FinishRun`. Tests use a real `python3` (skip if absent) for a trivial
  body: assert succeeded (exit 0), failed (non-zero), TTL (a sleeper + short ttl
  → failed "run TTL exceeded"), cancel (no event), and that after stop the run
  dir **persists** with `main.py` + `config.json` + logs (and a file the body
  wrote, to back `run_fs_*`).
- **Acceptance:** `go test ./internal/runner` green (or skip-with-reason if no
  python3 in the agent env — but it must compile and the logic be reviewable).

#### Phase 5 — `script/outcome.go` + `script/trigger.go`
- **Owns:** `internal/script/outcome.go`, `internal/script/trigger.go` (+ tests).
- **Implements:** A7 `Events` registry + `completionEvent` (payload exactly per
  README; `shouldEmit=false` for cancelled; `error` omitempty). `trigger.go`
  holds any trigger-row helpers/glob-validation used by service (keep the split
  matching prompts). 
- **Acceptance:** `go test` green; payload JSON matches the README block field
  names; cancelled → no event.

#### Phase 6 — `consume/` (fan-in)
- **Owns:** `internal/consume/consume.go` (+ `consume_test.go`).
- **Implements:** A9 — `Subscriptions(sources)` and `Handler(...)`. Per event:
  lookup → fire per script on a goroutine; always `nil` for well-formed,
  `ErrSkip` for malformed. Model on `prompts/internal/consume` + `notify`'s
  multi-source handler. Tests use fake `fire`/`lookup`, assert fan-out count and
  the never-stall contract.
- **Acceptance:** `go test ./internal/consume` green.

#### Phase 7 — `mcp/tools.go` + `mcp/describe.go`
- **Owns:** `internal/mcp/tools.go`, `internal/mcp/describe.go` (+ tests).
- **Implements:** A10 — the 16 tool descriptors (input schemas, incl. run-scoped
  `run_fs_list`/`run_fs_read`) and `dispatchTool` routing each to the **A8
  Service** method; `health.details` = the static runtime contract; `describe`
  markdown. Codes against the Service interface only.
- **Acceptance:** `go test ./internal/mcp` green; tool list has exactly the 16
  names with `ikigenba_scripts_` prefix; `run_fs_list`/`run_fs_read` present, no
  bare `fs_list`/`fs_read`.

#### Phase 8 — packaging, lifecycle, provisioning, registration (no Go-compile coupling to Wave 2)
- **Owns:** `scripts/{Makefile, etc/manifest.env, VERSION}`,
  `scripts/bin/{backup,restore,start,stop,teardown}`, an **opsctl change**
  (`opsctl/internal/opsctl/setup.go` + `opsctl/cmd/opsctl/main.go`), and a
  **dashboard registration diff** (proposed, not applied).
- **Implements:**
  1. **Lifecycle `bin/*`** — rename/adjust the `prompts` set for scripts (3009,
     scripts paths, **drop secrets**). `bin/start`/`stop` keep the soft `python3`
     preflight *warning* (non-fatal) as `prompts` does. `bin/backup` includes
     `data/runs/` (now the full per-run tree: inputs + logs + produced files) and
     snapshots the DB via `VACUUM INTO` (ARCHITECTURE §7). There is no
     `data/work/` to exclude.
  2. **Python3.11 provisioning hook (Part 0 §5)** — add `Packages []string` to
     opsctl's `SetupOptions`; in `setup.go`, early (after `layout`, before user
     creation), `if len(opts.Packages)>0 { o.System.InstallPackages(ctx,
     opts.Packages...) }` modeled on `initbox.go:58`; expose a `--packages`
     comma-list flag in `runSetup` (`opsctl/cmd/opsctl/main.go`). Wire scripts'
     deploy path to call `opsctl setup scripts --packages python3.11`. This makes
     scripts install its own python independent of prompts. Add/extend the
     `stubSystem` opsctl test to assert the packages reach `InstallPackages`.
  3. **`etc/manifest.env`** — `App=scripts`, `Port=3009`, `Mount=/srv/scripts/`,
     `MCP=true`, `Feed=/feed`, `Consumes=cron,crm,ledger,dropbox,prompts`, +
     `OUTBOX_RETENTION_*` extras (A11).
  4. **Dashboard registration** — write up the diff adding
     `https://${IKIGENBA_DOMAIN}/srv/scripts/mcp` to `dashboard/bin/build →
     DASHBOARD_RESOURCES` (propose; editing `dashboard/` + redeploy is the
     operator's step).
- **Acceptance:** `bin/*` shellcheck-clean, reference scripts paths/port, no
  secret refs; `opsctl` builds + its tests pass and assert `python3.11` flows to
  `InstallPackages`; `etc/manifest.env` correct; registration diff written up.

---

### Wave 3 — integration & verification (1 subagent, after Wave 2)

#### Phase 9 — wire, build, test, smoke
- **Owns:** `cmd/scripts/main.go` final composition (un-stub anything Phase 1
  left provisional now that real packages exist), plus any cross-package seam
  fixes and an end-to-end test.
- **Steps:** `go build ./... && go vet ./... && go test ./...` from the service
  dir — all green. Run `./scripts migrate` then `./scripts serve` against a temp
  db on 3009; over loopback `POST /mcp` (with `X-Owner-Email`/`X-Client-Id`
  headers) exercise: `create` → `run` → poll `run_get`/`run_output` → confirm a
  `scripts.succeeded` event appears on `GET /feed`; `run_fs_list`/`run_fs_read`
  return a file the body wrote; `set_trigger` then simulate
  an upstream event (or unit-level fire) → triggered run; `run_cancel` → status
  cancelled + **no** event. Confirm crash-recovery: insert a `running` row, boot,
  see it swept to `failed`.
- **Acceptance:** full build/vet/test green; the smoke sequence behaves per the
  ARCHITECTURE §8 flows; completion events match A7; cancel emits nothing.

---

## Part C — wave graph, conflict rules, open items

**Dependency graph:**
```
Wave 1:  [Phase 1 foundation]            (blocking)
              |
Wave 2:  [2 store][3 service][4 runner][5 outcome/trigger][6 consume][7 mcp][8 packaging]   (parallel)
              |
Wave 3:  [Phase 9 integration + smoke]   (after all of Wave 2)
```

**Conflict rules (what keeps parallel safe):**
- File ownership is disjoint (table above). No two Wave-2 agents write the same
  file. `model.go` is owned by Phase 1 and is **read-only** for all of Wave 2 —
  if an agent thinks a type must change, it stops and routes the change through
  the orchestrator, who updates Part A and re-broadcasts.
- Wave-2 agents may add **new** unexported helpers in their own files but must
  not change any Part A signature. The package compiles continuously because the
  stubs Phase 1 left are only replaced in-place, file by file.
- `go.mod`/`go.work`/`migrations` are frozen by Phase 1; Wave 2 does not touch
  them (Phase 8 touches only `Makefile`/`etc`/`bin`/`VERSION`/the dashboard diff,
  none of which the Go packages import).

**Open items — status:**
1. ~~Upstream producer source ids~~ — **RESOLVED** (Part 0 §4): the five are
   `cron`(3007), `crm`(3001), `ledger`(3002), `dropbox`(3005), `prompts`(3004),
   all live producers. `agent`→`prompts`. Self-chaining on scripts' own feed is a
   noted fast-follow (A12).
2. ~~Python3.11 provisioning seam~~ — **RESOLVED** (Part 0 §5): no per-service
   `bin/setup` exists and nothing installs python today; Phase 8 adds a
   `Packages` hook to `opsctl setup` and has scripts request `python3.11`
   independently, so it works with or without `prompts`.
3. **Commit decision (open, orchestrator's call):** whether to commit the two
   design docs + this plan on the `scripts` branch before Phase 1. Recommended:
   yes, so the build starts from a clean tracked base.

**Not in scope (deferred per README):** backpressure/bounded pool, run
retention/GC, third-party python packages, OS sandbox isolation, runtime feed
discovery, non-python interpreters.
