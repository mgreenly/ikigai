# REDESIGN-PLAN — prompts: async runs + MCP surface redesign

Implementation plan for the redesign settled in `REDESIGN-DECISIONS.md`. That
document is the **why** and the ratified external scheme; this is the **how**:
the frozen target contracts plus a strictly-sequential phase chain to transform
the existing code into them.

> This **supersedes** `PLAN.md` (the original "run-once slice" build plan that
> produced the current code). `PLAN.md` is kept for history; do not execute it.

This is **not** a greenfield build. The prompts service already implements the
whole machinery (`store` / `service` / `runner` / `sandbox` / `consume` /
`outcome` / `mcp`). Every phase below is a **transformation** of code that
already exists. The sibling `scripts` service is **design-docs only** (no Go); it
is the *design template* for the target shape, not code to copy.

prompts and scripts are **two sides of one coin** — same surface and lifecycle,
differing only in the input field (`user_prompt` vs `body`) and the executor
(non-deterministic agent loop vs deterministic `python3`/`bash`). One place they
diverge today is **triggers**: prompts is cron-only with one trigger per prompt,
while scripts (PLAN §A9/§A12) has a **general multi-source** model — N triggers
per object across N upstreams. This plan brings prompts onto that same model
(Phase 6), closing the divergence the scripts PLAN already booked as "implied
prompts work" (scripts PLAN §7C(E)).

## How this plan is executed

An orchestrator runs subagents **strictly sequentially** — one phase per
subagent, in order, never in parallel. Therefore:

- **Each phase must leave the tree green.** The gate at the end of every phase is
  the same and is non-negotiable:

  ```
  cd /mnt/projects/ikigai/prompts_refactor/prompts
  go build ./... && go vet ./... && go test ./...
  ```

  (Local dev resolves modules via the repo-root `go.work`. Confirm the three
  commands are green once at the start of Phase 1; if the workspace needs to be
  named explicitly, prefix with
  `GOWORK=/mnt/projects/ikigai/prompts_refactor/go.work`.)

- **Part A is frozen.** Every phase codes toward the Part A contracts verbatim. A
  phase never invents a name or shape that contradicts Part A. If a phase finds
  Part A wrong, it **stops and reports** rather than diverging.

- **Phases are coherent vertical deltas**, each small enough for one subagent to
  finish in a sitting. A phase may touch several files/layers (store + service +
  runner) — that is fine under sequential execution because no two subagents
  ever run at once. What a phase must **not** do is leave the build red or tests
  failing for the next subagent.

- **Migrations are rewritten in place.** prompts is **not deployed** anywhere
  (no release, no DB on `int`), so there is no data to preserve. Phases edit
  `002`/`003` SQL directly toward the Part A schema — no `ALTER` chains, no
  backfill. Migrations `004_feed_offset.sql` / `005_outbox.sql` are
  library-owned and are **not touched**.

- **Tests travel with the code.** A phase that changes behavior updates the
  `_test.go` files in the same phase so the gate passes. Do not defer test fixes
  to a later phase.

---

# Part A — Frozen Contracts (the target end-state)

These describe prompts **after** Phase 8. Earlier phases converge on them.

## A1 — Identity (unchanged)

- Go module `prompts`; internal domain package renamed `session` → **`prompt`**
  (`internal/prompt/`).
- MCP tool prefix `ikigenba_prompts_` (unchanged).
- Loopback service, `X-Owner-Email` / `X-Client-Id` trusted headers, appkit
  chassis, SQLite — all unchanged.

## A2 — On-disk layout (settles the layout deferred in DECISIONS §"Materialize")

`dataDir` = `dirname(PROMPTS_DB_PATH)/data` (as today). The persistent
per-session `sandboxes/` tree is **gone**. The unit on disk is the **run
directory**, keyed by `run_id`:

```
data/
  prompts.db
  runs/
    <run_id>/
      input/
        user_prompt.txt       # the pinned user_prompt for THIS run
        system_prompt.txt     # the pinned system_prompt (empty file if none)
        config.json           # the pinned {provider, model, effort?, max_tokens?, temperature?}
      output.jsonl            # append-only stream-json run log
      sandbox/                # the agent's per-run workspace (produced files)
```

- `input/` is written by the service **at spawn** from the prompt's current
  definition, then **persisted with the run** forever. It *is* the record of
  exactly what executed.
- The runner reads its execution inputs from `input/` — never from the DB or the
  live `Prompt` object.
- `sandbox/` starts empty every run (no seeding / carry-over — explicit
  non-goal) and is persisted after the run for the `run_fs_*` readers.
- Retention / GC of run directories is **deferred** (not in this plan).

## A3 — Schema (final `002` / `003`)

`internal/db/migrations/002_prompts.sql`:

```sql
CREATE TABLE prompts (
    id            TEXT PRIMARY KEY,        -- ULID
    owner_email   TEXT NOT NULL,           -- from X-Owner-Email at create
    name          TEXT,                    -- optional label; NOT unique, NOT a key
    user_prompt   TEXT NOT NULL,           -- the user-role prompt
    system_prompt TEXT,                    -- the system prompt (nullable)
    config_json   TEXT NOT NULL,           -- {provider, model, effort?, max_tokens?, temperature?}
    created_at    TEXT NOT NULL,
    updated_at    TEXT NOT NULL
);

CREATE TABLE runs (
    id            TEXT PRIMARY KEY,        -- ULID (the run, first-class & addressable)
    prompt_id     TEXT NOT NULL,           -- NO foreign key / NO cascade; may dangle after tombstone delete
    owner_email   TEXT NOT NULL,           -- denormalized: run stays owner-addressable after its prompt is gone
    prompt_name   TEXT,                    -- captured at run start, for the outcome event
    status        TEXT NOT NULL,           -- 'running' | 'succeeded' | 'failed' | 'cancelled'
    started_at    TEXT NOT NULL,
    ended_at      TEXT,
    usage_json       TEXT,
    error            TEXT,
    trigger_source   TEXT,                 -- '' for a manual run; else producer source id (cron|crm|ledger|dropbox|scripts|prompts)
    trigger_type     TEXT,                 -- the fired event type, e.g. "cron.nightly", "file.created", "scripts.succeeded"
    trigger_event_id TEXT,                 -- the upstream event id that fired this run
    log_path         TEXT NOT NULL         -- data/runs/<run_id>/output.jsonl
);

CREATE INDEX idx_runs_prompt ON runs(prompt_id, started_at);
CREATE INDEX idx_runs_status ON runs(status);
```

`internal/db/migrations/003_prompt_triggers.sql` (rename file from
`003_session_triggers.sql`):

```sql
CREATE TABLE prompt_triggers (
    prompt_id    TEXT NOT NULL,            -- NO cascade (tombstone delete removes a prompt's triggers explicitly)
    source       TEXT NOT NULL,            -- producer source id: cron|crm|ledger|dropbox|scripts|prompts
    event_filter TEXT NOT NULL,            -- the event type/glob this prompt listens for, e.g. "cron.nightly", "file.created"
    created_at   TEXT NOT NULL,
    PRIMARY KEY (prompt_id, source, event_filter)   -- N triggers per prompt, across N sources
);

CREATE INDEX idx_prompt_triggers_lookup ON prompt_triggers(source, event_filter);
```

A trigger is one `(prompt, source, event_filter)` binding; a prompt may hold
many — one per upstream event it reacts to. The old cron-only knobs
(`max_staleness_secs`, `max_attempts`, the single `updated_at`) are **gone**:
fire-and-forget, symmetric with scripts.

Key differences from today: `prompts` has **no `status` column** (no
single-flight lifecycle on the prompt); `runs` has **no FK/cascade**, gains
`owner_email` / `prompt_name` and the trigger context
`trigger_source` / `trigger_type` / `trigger_event_id`, and `log_path` points at
`runs/<run_id>/output.jsonl` (keyed by `run_id`, not `session_id`); the trigger
table loses its FK cascade and becomes a **composite-key, multi-source** table
(was 1:1 cron-only).

## A4 — Domain types (`internal/prompt/model.go`)

```go
package prompt

const (
    RunRunning   = "running"
    RunSucceeded = "succeeded"
    RunFailed    = "failed"
    RunCancelled = "cancelled"
)

var ErrNotFound = errors.New("prompt: not found")
// ErrBusy and ErrRunning are DELETED — full concurrency, no single-flight,
// no "rejected while running".

type Config struct {
    Provider    string   `json:"provider"`
    Model       string   `json:"model"`
    Effort      string   `json:"effort,omitempty"`
    MaxTokens   int      `json:"max_tokens,omitempty"`
    Temperature *float64 `json:"temperature,omitempty"`
}

type Prompt struct {
    ID           string `json:"id"`
    OwnerEmail   string `json:"owner_email"`
    Name         string `json:"name,omitempty"`
    UserPrompt   string `json:"user_prompt"`
    SystemPrompt string `json:"system_prompt,omitempty"`
    Config       Config `json:"config"`
    CreatedAt    string `json:"created_at"`
    UpdatedAt    string `json:"updated_at"`
    // NOTE: no Status field.
}

type Run struct {
    ID           string `json:"id"`
    PromptID     string `json:"prompt_id"`
    OwnerEmail   string `json:"owner_email"`
    PromptName   string `json:"prompt_name,omitempty"`
    Status       string `json:"status"`
    StartedAt    string `json:"started_at"`
    EndedAt      string `json:"ended_at,omitempty"`
    UsageJSON    string `json:"usage_json,omitempty"`
    Error        string `json:"error,omitempty"`
    TriggerSource  string `json:"trigger_source,omitempty"`   // "" for a manual run; else producer source id
    TriggerType    string `json:"trigger_type,omitempty"`     // the fired event type
    TriggerEventID string `json:"trigger_event_id,omitempty"` // the upstream event id
    LogPath        string `json:"log_path"`
}

type PromptDetail struct {
    Prompt
    RunningCount int  `json:"running_count"` // derived COUNT(runs WHERE prompt_id=? AND status='running')
    LastRun      *Run `json:"last_run"`      // nil if never run
}

// Trigger is one (prompt, source, event_filter) binding. A prompt may hold
// many — one per upstream event it reacts to. The cron-only staleness/retry
// knobs are GONE (fire-and-forget, symmetric with scripts).
type Trigger struct {
    PromptID    string `json:"prompt_id"`
    Source      string `json:"source"`       // producer source id (cron|crm|ledger|dropbox|scripts|prompts)
    EventFilter string `json:"event_filter"` // the event type/glob, e.g. "cron.nightly"
    CreatedAt   string `json:"created_at"`
}

// TriggerSpec is the create-time sugar: triggers passed to Create are applied
// via SetTrigger after the prompt row is inserted (same validation).
type TriggerSpec struct {
    Source      string `json:"source"`
    EventFilter string `json:"event_filter"`
}
```

## A5 — Store (`internal/prompt/store.go`, `trigger.go`)

Final method set (renamed + reshaped from today):

```go
// prompts
InsertPrompt(ctx, Prompt) error
GetPrompt(ctx, owner, id string) (Prompt, error)         // owner-scoped
GetPromptByID(ctx, id string) (Prompt, error)            // unscoped (event path)
ListPrompts(ctx, owner string) ([]Prompt, error)
UpdatePrompt(ctx, owner string, p Prompt) error
DeletePrompt(ctx, owner, id string) error                // removes ONLY the prompt row
// (SetPromptStatus is DELETED — no status column)

// runs (first-class, run_id-addressed)
InsertRun(ctx, Run) error                                // writes owner_email, prompt_name, trigger ctx
GetRun(ctx, runID string) (Run, error)                   // by run_id (no owner scope here; service scopes)
ListRunsByPrompt(ctx, promptID string) ([]Run, error)    // newest first
GetLatestRun(ctx, promptID string) (*Run, error)         // for PromptDetail.LastRun
RunningCount(ctx, promptID string) (int, error)          // for PromptDetail.RunningCount
FinishRun(ctx, FinishRunInput) error                     // terminal run write + outcome event in ONE tx; reads run row for event fields; touches NO prompt row
SweepRunning(ctx) (int, error)                           // marks orphaned running runs failed; touches RUNS ONLY

// triggers (multi-source: keyed by (prompt_id, source, event_filter))
SetTrigger(ctx, Trigger) error                                // upsert one (prompt, source, event_filter) binding
ClearTrigger(ctx, promptID, source, eventFilter string) error // remove one binding; ErrNotFound if absent
DeleteTriggers(ctx, promptID string) error                    // remove ALL of a prompt's bindings (tombstone Delete); no error if none
ListTriggers(ctx, promptID string) ([]Trigger, error)         // a prompt's bindings
PromptsForEvent(ctx, source, evType string) ([]string, error) // (source, type) -> prompt ids; the consumer fan-out
```

`FinishRunInput` carries `RunID`, `Status`, `EndedAt`, `UsageJSON`, `ErrMsg`
only — the outcome-event fields (`prompt_id`, `prompt_name`, `run_id`,
`trigger_source`, `trigger_type`, `trigger_event_id`) are read **from the run
row** inside the transaction, so the runner no longer threads them in.

## A6 — Runner (`internal/runner/runner.go`)

```go
type Runner interface {
    Spawn(run Run)            // NOTE: no Prompt/Session param — runner reads input/ from disk
    Cancel(runID string) bool
}
// plus Recover(ctx) (int, error) on the concrete type
```

- `Spawn(run Run)` derives `runDir = runsDir/<run.ID>`, reads
  `input/config.json` (→ resolve model, effort, max_tokens),
  `input/user_prompt.txt`, `input/system_prompt.txt`; executes the agent loop
  with `sandboxRoot = runDir/sandbox`; streams to `runDir/output.jsonl`.
- The cancel map and `userCancelled` map are keyed by **`run_id`**.
- `Cancel(runID)` signals that run; idempotent; returns whether one was in
  flight.
- The terminal write calls `store.FinishRun` (which sources the event fields from
  the run row). **No prompt-status flip** (there is none).

## A7 — Service (`internal/prompt/service.go`)

```go
Create(ctx, owner string, CreateInput) (Prompt, error)            // CreateInput{Name, UserPrompt, SystemPrompt, Config, Triggers []TriggerSpec}; NO sandbox here. Optional Triggers applied via SetTrigger after insert (same validation; reject the whole create if any is invalid)
List(ctx, owner string) ([]PromptDetail, error)                   // with RunningCount + LastRun
Get(ctx, owner, promptID string) (PromptDetail, error)
Update(ctx, owner, promptID string, UpdateInput) (Prompt, error)  // ALWAYS allowed (no ErrRunning)
Delete(ctx, owner, promptID string) error                         // tombstone: prompt row + its trigger only; runs + artifacts survive; ALWAYS allowed

Run(ctx, owner, promptID string) (Run, error)                     // ALWAYS accepted; materializes input/, makes sandbox/, inserts run row, spawns
RunByEvent(ctx, promptID, source, evType, eventID string, payload []byte) (Run, error) // event path; unscoped; run row records (trigger_source, trigger_type, trigger_event_id)

RunList(ctx, owner, promptID string) ([]Run, error)               // owner via prompt
RunGet(ctx, owner, runID string) (Run, error)                     // owner via run.owner_email (works post-tombstone)
RunOutput(ctx, owner, runID string, offset, limit int) (string, error)
RunCancel(ctx, owner, runID string) error                         // idempotent
RunFsList(ctx, owner, runID, path string) ([]sandbox.Entry, error)
RunFsRead(ctx, owner, runID, path string, offset, limit int) (string, error)

SetTrigger(ctx, owner, promptID, source, eventFilter string) (Trigger, error) // validates source ∈ TriggerSources() and event_filter plausible for it (A12); upsert
ClearTrigger(ctx, owner, promptID, source, eventFilter string) error          // remove one binding
PromptsForEvent(ctx, source, evType string) ([]string, error)                 // consumer path; NOT owner-scoped
TriggerSources() []string                                                     // static known-producer set (A12), for set_trigger validation
```

Owner-scoping for `run_*` methods is via the **run's** `owner_email` column, so a
run remains readable after its prompt is tombstoned. `SetTrigger` rejects an
unknown source or an `event_filter` that cannot match the producer's registry
(A12) with `ErrValidation`.

## A8 — MCP tool surface (`internal/mcp/tools.go`, `describe.go`)

Prefix `ikigenba_prompts_`. **Airtight rule:** keyed by `prompt_id` → bare verb;
keyed by `run_id` → `run_*`.

| tool | key | input schema (** = required) | service call |
|---|---|---|---|
| `health` | — | `{}` | — |
| `describe` | — | `{}` | — |
| `create` | returns `prompt_id` | `{user_prompt**, config**, name, system_prompt, triggers}` | `Create` |
| `list` | — | `{}` | `List` |
| `get` | `prompt_id` | `{prompt_id**}` | `Get` |
| `update` | `prompt_id` | `{prompt_id**, user_prompt, system_prompt, config, name}` | `Update` |
| `delete` | `prompt_id` | `{prompt_id**}` | `Delete` |
| `set_trigger` | `prompt_id` | `{prompt_id**, source**, event_filter**}` | `SetTrigger` |
| `clear_trigger` | `prompt_id` | `{prompt_id**, source**, event_filter**}` | `ClearTrigger` |
| `run` | `prompt_id` → returns `run_id` | `{prompt_id**}` | `Run` |
| `run_list` | `prompt_id` | `{prompt_id**}` | `RunList` |
| `run_get` | `run_id` | `{run_id**}` | `RunGet` |
| `run_output` | `run_id` | `{run_id**, offset, limit}` | `RunOutput` |
| `run_cancel` | `run_id` | `{run_id**}` | `RunCancel` |
| `run_fs_list` | `run_id` | `{run_id**, path}` | `RunFsList` |
| `run_fs_read` | `run_id` | `{run_id**, path**, offset, limit}` | `RunFsRead` |

The wire input field for the user-role prompt is **`user_prompt`** (was
`prompt`). The `session_` subprefix is dropped everywhere. `create` returns
`{prompt_id, ...}`; `run` returns `{run_id, status:"running", started_at}`.

Triggers are **multi-source**: `set_trigger` attaches one `(source,
event_filter)` binding and may be called repeatedly to attach several;
`clear_trigger` removes one binding by the same key. `source` must be a known
producer and `event_filter` a type it publishes (A12). `create`'s optional
`triggers` is an array of `{source, event_filter}` applied at create time.

## A9 — Outcome events (`internal/prompt/outcome.go`)

Two static types, unchanged names: `run.succeeded`, `run.failed` (cancelled
emits nothing). Payload (renamed + `run_id` added):

```go
type outcomePayload struct {
    PromptID       string `json:"prompt_id"`
    PromptName     string `json:"prompt_name"`
    RunID          string `json:"run_id"`
    TriggerSource  string `json:"trigger_source"`   // "" for a manual run
    TriggerType    string `json:"trigger_type"`
    TriggerEventID string `json:"trigger_event_id"`
    Error          string `json:"error,omitempty"`
}
```

## A10 — Consume (`internal/consume/consume.go`) — multi-upstream fan-in

prompts subscribes to **N upstreams**, not just cron (symmetric with scripts
§A9). One `Subscription` per upstream with `Filter:"*"`; prompts fires on any
type and decides via `PromptsForEvent`.

```go
func Subscriptions(sources []string) []consumer.Subscription
// Handler: per event -> PromptsForEvent(source, ev.Type) -> RunByEvent per
// matching prompt on a goroutine (unbounded, non-blocking, fire-and-forget).
// Returns nil for a matched event (never stall); ErrSkip only on a malformed
// envelope. Cursor advances for every event.
func Handler(fire FireFunc, lookup LookupFunc, source string, logger *slog.Logger) consumer.Handler

type FireFunc   func(ctx context.Context, promptID, source, evType, eventID string, payload []byte) error
type LookupFunc func(ctx context.Context, source, evType string) ([]string, error)
```

The old single `cron.*` subscription, `fireWithRetry`, the staleness guard, and
the fixed-delay retry are all **gone** — replaced by per-event
fire-and-forget (the `max_staleness_secs` / `max_attempts` knobs were dropped
with the trigger). cmd wires one consumer worker per upstream (the `notify` /
scripts multi-cursor pattern).

## A11 — Consumed upstreams & feed URLs (`PROMPTS_*`)

prompts is a producer (`run.succeeded` / `run.failed`) **and** a multi-upstream
consumer. Day-one upstream set mirrors scripts': the four external producers
plus scripts. One `PROMPTS_<SRC>_FEED_URL` per upstream with a loopback dev
fallback, plus `PROMPTS_<SRC>_FROM` (default `tail`):

| env var | dev fallback | source |
|---|---|---|
| `PROMPTS_CRON_FEED_URL`    | `http://127.0.0.1:3007/feed` | cron |
| `PROMPTS_CRM_FEED_URL`     | `http://127.0.0.1:3001/feed` | crm |
| `PROMPTS_LEDGER_FEED_URL`  | `http://127.0.0.1:3002/feed` | ledger |
| `PROMPTS_DROPBOX_FEED_URL` | `http://127.0.0.1:3005/feed` | dropbox |
| `PROMPTS_SCRIPTS_FEED_URL` | `http://127.0.0.1:3009/feed` | scripts |

`ManifestExtras` lists `CONSUMES` = these five sources. **Self-chaining**
(prompts consuming its OWN `/feed` at `:3004`, so a prompt fires on another
prompt's `run.succeeded`) is a one-line fast-follow — wire the five external
upstreams day-one and leave self-consumption as a flagged TODO (it adds a sixth
consumer loop pointed at the local feed).

## A12 — Known-producer registry (`SetTrigger` validation)

`SetTrigger` validates `(source, event_filter)` against this static map; reject
the unsatisfiable with `ErrValidation`. `TriggerSources()` returns the keys.

```
cron    -> dynamic; accept any event_filter matching "cron.*"
crm     -> contact.created | contact.updated | contact.tagged | contact.untagged
ledger  -> transaction.recorded
dropbox -> file.created | file.modified | file.deleted
scripts -> scripts.succeeded | scripts.failed
prompts -> run.succeeded | run.failed   (self-chaining; prompts' OWN feed — fast-follow)
```

This is the mirror image of scripts §A12 (which lists prompts as an upstream):
the two services are symmetric event-plane peers.

---

# Part B — Sequential phases

Eight phases, executed in order. Each ends with the Part A-aligned change for its
scope and a **green gate**.

## Phase 1 — Rename `session` → `prompt` (mechanical, behavior-preserving)

**Goal:** eliminate the word `session` as the object name, with **zero**
behavioral or external-surface change. Single-flight, the prompt `status`
column, the per-session sandbox, and the current `session_*` tool names all stay
exactly as they are. This phase is pure renaming; it is the largest by
line-count but the lowest in judgement — execute it as scripted global replaces
followed by a compile-fix loop.

**Rename mapping (apply across all `.go`, `_test.go`, `.sql`):**

- Directory/package: `internal/session/` → `internal/prompt/`; `package session`
  → `package prompt`; import `prompts/internal/session` → `prompts/internal/prompt`;
  qualified refs `session.X` → `prompt.X`.
- Types: `Session` → `Prompt`, `SessionDetail` → `PromptDetail`.
- Struct field + JSON tag: `Session.Prompt` (`json:"prompt"`) → `Prompt.UserPrompt`
  (`json:"user_prompt"`); `CreateInput.Prompt`/`UpdateInput.Prompt` →
  `.UserPrompt`; `Run.SessionID` (`json:"session_id"`) → `Run.PromptID`
  (`json:"prompt_id"`).
- Store methods: `InsertSession`→`InsertPrompt`, `GetSession`→`GetPrompt`,
  `GetSessionByID`→`GetPromptByID`, `ListSessions`→`ListPrompts`,
  `UpdateSession`→`UpdatePrompt`, `DeleteSession`→`DeletePrompt`,
  `SetSessionStatus`→`SetPromptStatus`, `scanSession`→`scanPrompt`.
  `FinishRunInput.SessionID`→`PromptID`, `.SessionName`→`.PromptName`.
- DB (in `store.go`, `trigger.go`, migrations): table `sessions`→`prompts`,
  `session_triggers`→`prompt_triggers`; column `prompt`→`user_prompt`,
  `session_id`→`prompt_id`; index `idx_runs_session`→`idx_runs_prompt`,
  `idx_session_triggers_event`→`idx_prompt_triggers_event`. **Rename the file**
  `003_session_triggers.sql` → `003_prompt_triggers.sql` (update the embed/glob if
  the filename is referenced anywhere; check `internal/db` + its tests).
- `Trigger.SessionID` → `PromptID`; trigger store methods' params/SQL.
- Runner: param/locals `sess Session`→`p Prompt`; `sess.Prompt`→`p.UserPrompt`;
  cancel maps keep their string key but rename the param `sessionID`→`promptID`
  (still keyed by the prompt id at this stage).
- Sandbox: cosmetic `session`→`prompt` in comments/locals; `sessionRoot`→
  `promptRoot`. Behavior identical (still keyed by the prompt id).
- Consume: `FireFunc` param `sessionID`→`promptID`; `triggerLookup` returns
  `[]prompt.Trigger`; comments.
- Outcome: `outcomePayload` `session_id`→`prompt_id`, `session_name`→
  `prompt_name`; sample vars; `Events` descriptions; `outcomeEvent` params.
- Sentinel error text: `"session: ..."` → `"prompt: ..."`.
- **Keep unchanged this phase:** `StatusIdle`/`StatusRunning`, `ErrBusy`,
  `ErrRunning`, the `prompts.status` column, the MCP tool **name strings**
  (`ikigenba_prompts_session_create`, …) and the wire JSON key `prompt` in the
  `create`/`update` dispatch structs (map `in.Prompt` → `CreateInput{UserPrompt:
  in.Prompt}` so the surface is byte-for-byte the same). Those are reshaped in
  Phase 6.

**Files:** essentially all of `internal/prompt/` (renamed dir),
`internal/runner/`, `internal/sandbox/`, `internal/consume/`, `internal/mcp/`,
`internal/db/migrations/002,003`, `cmd/prompts/main.go`, and every `_test.go`.

**Done when:** no `session` Go identifier or SQL name remains (grep `-rn 'session'
--include=*.go --include=*.sql` shows only incidental prose/strings Phase 6 will
revisit), and the gate is green. External MCP behavior is identical.

## Phase 2 — Per-run directory & run-scoped sandbox

**Goal:** make the **run** the on-disk unit (A2 layout), keyed by `run_id`, with
the sandbox per-run. Single-flight is still in place, so at most one run per
prompt is live — there is no concurrent-sandbox collision yet (that safety is
why this precedes Phase 3).

**Changes:**

- `sandbox.Manager`: re-root at `runsDir`; `Create/Root/List/Read` keyed by
  `run_id`, resolving to `runsDir/<run_id>/sandbox`. (`validateID`/`confine`
  unchanged.)
- `service.Create`: **stop creating a sandbox** (no per-prompt sandbox anymore).
- `service.Run`/`RunByID`: set `run.LogPath = runsDir/<run_id>/output.jsonl`;
  create the run-scoped sandbox (`sandbox.Create(run.ID)`) before spawn.
- `runner`: `sandboxRoot = sandbox.Root(run.ID)` (= `runsDir/<run_id>/sandbox`);
  open the log at `run.LogPath`; cancel/`userCancelled` maps keyed by `run.ID`;
  `Cancel(runID)`.
- `service.Cancel`: keep the prompt-keyed signature for now, but resolve the
  prompt's latest running run and call `runner.Cancel(thatRunID)`. (Replaced by
  `RunCancel(run_id)` in Phase 5/6.)
- `service.Output`/`FsList`/`FsRead`: keep their prompt-keyed signatures (the
  current `session_*`-named tools still call them) but **delegate to the latest
  run's directory** — resolve `GetLatestRun(promptID)` and read that run's
  `output.jsonl` / `sandbox/`. A prompt that has never run returns the existing
  "no run output yet" error.
- `service.Delete`: stop removing a per-prompt sandbox; remove the prompt's run
  directories (`runs/<run_id>` for each run of the prompt). (Tombstone in Phase 5
  removes this cleanup entirely.)
- `runner.Recover` / `store.SweepRunning`: unchanged behavior here (still flips
  the prompt status too — that line is removed in Phase 3).

**Files:** `internal/sandbox/sandbox.go`, `internal/runner/runner.go`,
`internal/prompt/service.go`, `internal/prompt/store.go` (path helpers if any),
`cmd/prompts/main.go` (`sandbox.New` base dir → `runsDir`), relevant `_test.go`.

**Done when:** runs live under `runs/<run_id>/{output.jsonl,sandbox/}`, no
`sandboxes/` tree is created, and the gate is green.

## Phase 3 — Full concurrency: drop prompt status & single-flight

**Goal:** remove the single-flight gate and the prompt lifecycle entirely. `run`
is always accepted; concurrent runs per prompt are allowed (safe now — each run
has its own sandbox from Phase 2).

**Changes:**

- Migration `002`: drop the `status` column from `prompts`.
- `model.go`: delete `StatusIdle`/`StatusRunning`; delete `ErrBusy`/`ErrRunning`;
  remove `Prompt.Status`.
- `store.go`: delete `SetPromptStatus`; `InsertPrompt`/`scanPrompt`/`UpdatePrompt`
  drop `status`; `FinishRun` drops the prompt-status `UPDATE` (it now writes only
  the run row + the outcome event); `SweepRunning` updates **runs only** (mark
  orphaned `running` → `failed`; no prompt touch).
- `service.go`: `Run`/`RunByID` drop the `status == running → ErrBusy` check and
  the `SetPromptStatus(running)` calls; `Update` and `Delete` drop the
  `ErrRunning` guard (both always allowed).
- `runner.go`: `finish` no longer implies any status flip (already handled by
  `FinishRun`); nothing else.
- `consume.go`: remove `isBusy`/`ErrBusy` handling from `fireWithRetry` (every
  matching trigger now starts a run).
- Tests: delete `ErrBusy`/`ErrRunning` assertions; add a two-concurrent-runs-on-
  one-prompt test proving sandbox isolation.

**Files:** `internal/db/migrations/002_prompts.sql`, `internal/prompt/model.go`,
`store.go`, `service.go`, `internal/runner/runner.go`,
`internal/consume/consume.go`, `_test.go`.

**Done when:** `run` always returns a `run_id`, two runs of one prompt execute in
isolated sandboxes, and the gate is green.

## Phase 4 — Materialize execution inputs to disk; runner reads from disk

**Goal:** implement the governing rule — the changeable execution inputs are
pinned per run on disk, and the runner executes from disk, not from the live
prompt.

**Changes:**

- Migration `002` (runs): add `owner_email`, `prompt_name`, and the **interim**
  cron trigger columns `trigger_event`, `scheduled_for`. Update
  `InsertRun`/`scanRun`/`Run` struct to carry them. (Phase 6 reshapes
  `trigger_event`/`scheduled_for` → `trigger_source`/`trigger_type`/
  `trigger_event_id` — the A3 final form — when triggers go multi-source. Until
  then the consumer is still cron-only, so the cron columns suffice.)
- `service.Run`/`RunByID`: before spawn, write `runs/<run_id>/input/`:
  `user_prompt.txt`, `system_prompt.txt` (empty file if none), `config.json`
  (the resolved `Config`). Populate the run row's `owner_email`, `prompt_name`,
  `trigger_event`, `scheduled_for` from the prompt + fire context. `RunByID`
  reads the owner from the prompt row (`GetPromptByID`).
- `runner.Spawn`: change signature to **`Spawn(run Run)`** (drop the
  `Prompt`/`Session` param). The runner reads `input/config.json` (→
  `model.Resolve`, effort, max_tokens), `input/user_prompt.txt`,
  `input/system_prompt.txt` from `runs/<run.ID>/input/` and builds the request
  from those. It must **not** reference any `Prompt` field for execution inputs.
- `runner` terminal write: `FinishRun` reads the event fields from the run row
  inside the tx (A5), so `FinishRunInput` shrinks to
  `{RunID, Status, EndedAt, UsageJSON, ErrMsg}`.
- `outcome.go`: add `run_id` to `outcomePayload` and the samples (A9); the
  `prompt_id`/`prompt_name` rename already landed in Phase 1.
- The `Runner` interface in `service.go` becomes `Spawn(Run)` / `Cancel(runID)
  bool`; `Service.Run` passes only the `Run`.

**Files:** `internal/db/migrations/002_prompts.sql`, `internal/prompt/model.go`,
`store.go`, `service.go`, `outcome.go`, `internal/runner/runner.go`, `_test.go`
(runner tests must stage `input/` on disk rather than pass a `Prompt`).

**Done when:** editing or deleting a prompt mid-run cannot change what the run
executes (it reads the pinned `input/`), `input/` persists with the run, and the
gate is green.

## Phase 5 — Tombstone delete + first-class run reads

**Goal:** delete becomes a tombstone; runs become independently addressable by
`run_id` with owner-scoped reads that survive their prompt.

**Changes:**

- Migration: confirm `runs` has **no** FK/cascade and `prompt_triggers` has no
  cascade (A3) — adjust if Phase 1 left a cascade in place.
- `store`: add `GetRun(runID)`, `ListRunsByPrompt(promptID)` (newest first),
  `RunningCount(promptID)`, and a trigger-cleanup delete (no error if absent). The
  trigger table is still 1:1 cron here, so this removes the prompt's single
  trigger; Phase 6 generalizes it to `DeleteTriggers(promptID)` (all bindings) —
  the A5 final name.
- `service.Delete`: tombstone — `DeletePrompt(owner,id)` then remove the prompt's
  trigger(s). **Do not** touch runs, run directories, or the outbox.
- New owner-scoped run methods (A7): `RunList(owner, promptID)`,
  `RunGet(owner, runID)`, `RunOutput(owner, runID, off, lim)`,
  `RunCancel(owner, runID)`, `RunFsList(owner, runID, path)`,
  `RunFsRead(owner, runID, path, off, lim)`. Owner check via the **run's**
  `owner_email` (for `RunGet`/`RunOutput`/`RunCancel`/`RunFs*`) and via the
  prompt for `RunList`. `RunCancel` calls `runner.Cancel(runID)`; idempotent.
- `Get`/`List` return `PromptDetail` with `RunningCount` + `LastRun`.
- Keep the Phase-2 prompt-keyed `Output`/`FsList`/`FsRead`/`Cancel` shims for now
  (the `session_*` tools still call them); Phase 6 removes them.

**Files:** `internal/prompt/store.go`, `service.go`, `model.go` (`PromptDetail`),
`_test.go` (a tombstone test: delete a prompt with runs → prompt gone, runs +
`run_fs_*` still readable by owner).

**Done when:** `delete` removes only the prompt row + its trigger; a run of a
deleted prompt is still returned by `RunGet`/`RunOutput`/`RunFs*`; the gate is
green.

## Phase 6 — Multi-source trigger model (symmetric with scripts)

**Goal:** generalize triggers from the cron-only, one-per-prompt model to
scripts' **general multi-source** model — a prompt declares N triggers across N
upstreams (cron, crm, ledger, dropbox, scripts) and the consumer fans in from
all of them. The per-trigger `max_staleness_secs` / `max_attempts` knobs are
**dropped** (fire-and-forget, matching scripts). This is the side of the coin the
scripts PLAN §7C(E) booked as "implied prompts work"; afterwards prompts and
scripts are symmetric event-plane peers.

**Changes:**

- **Migration `003`** — rewrite `prompt_triggers` to the A3 composite shape: PK
  `(prompt_id, source, event_filter)`, columns `prompt_id, source, event_filter,
  created_at`; drop `trigger_event`, `max_staleness_secs`, `max_attempts`,
  `updated_at`; index `(source, event_filter)`; no cascade.
- **Migration `002`** — reshape the runs trigger columns to A3 final:
  `trigger_event` / `scheduled_for` → `trigger_source`, `trigger_type`,
  `trigger_event_id`.
- **`model.go`** — `Trigger` → `{PromptID, Source, EventFilter, CreatedAt}` (A4);
  add `TriggerSpec{Source, EventFilter}`; `Run` trigger fields →
  `TriggerSource` / `TriggerType` / `TriggerEventID`; delete the
  `DefaultMaxStalenessSecs` / `DefaultMaxAttempts` consts.
- **`store.go` / `trigger.go`** — `SetTrigger(Trigger)` upsert on the composite
  key; `ClearTrigger(promptID, source, eventFilter)`; `DeleteTriggers(promptID)`
  (all bindings — replaces Phase 5's singular delete in `Delete`);
  `ListTriggers(promptID)`; `PromptsForEvent(source, evType)` → prompt ids.
  `InsertRun` / `scanRun` carry the new trigger columns.
- **`service.go`** — `SetTrigger(owner, promptID, source, eventFilter)` validates
  `source ∈ TriggerSources()` and `event_filter` plausible for that producer
  (A12), then upserts; `ClearTrigger(owner, promptID, source, eventFilter)`;
  `PromptsForEvent`; `TriggerSources()` (static A12 keys). `Create` accepts
  optional `Triggers []TriggerSpec`, applied via `SetTrigger` after insert (reject
  the whole create if any is invalid). `RunByID` → `RunByEvent(promptID, source,
  evType, eventID, payload)`; the run row records the trigger context. `Delete`
  calls `DeleteTriggers`.
- **`consume.go`** — replace the single `cron.*` subscription + `fireWithRetry`
  with the A10 multi-upstream fan-in: `Subscriptions(sources)` (one per upstream,
  `Filter:"*"`), `Handler(fire, lookup, source, logger)` doing per-event
  `PromptsForEvent(source, ev.Type)` → `RunByEvent` per match on a goroutine
  (fire-and-forget; cursor always advances; `ErrSkip` only on a malformed
  envelope). No staleness guard, no retry.
- **`outcome.go`** — `outcomePayload` trigger fields → `trigger_source`,
  `trigger_type`, `trigger_event_id` (A9).
- **`config` / `cmd/prompts/main.go`** — add the `PROMPTS_<SRC>_FEED_URL` /
  `PROMPTS_<SRC>_FROM` env (A11) for the five upstreams; build one consumer worker
  per upstream (the `notify` / scripts multi-cursor pattern); add `CONSUMES` to
  `ManifestExtras`. Self-consumption is a flagged one-line TODO, not wired.
- **MCP (minimal, to stay green)** — the existing `set_trigger` / `clear_trigger`
  tools still call the service, so update their wire schema + dispatch to
  `{source, event_filter}` here (their service signatures changed). The broader
  bare-verb / `run_*` / `user_prompt` reshape — and exposing `create`'s `triggers`
  field — stays in Phase 7.
- **Tests** — composite-key set/clear/list; `PromptsForEvent` fan-out; create with
  inline triggers; a fan-in test firing a **non-cron** event (e.g. `{dropbox,
  file.created}`) and asserting the matching prompt runs with the run row's
  `(trigger_source, trigger_type, trigger_event_id)` populated; validation rejects
  an unknown source / implausible `event_filter`.

**Files:** `internal/db/migrations/002_prompts.sql`,
`internal/db/migrations/003_prompt_triggers.sql`,
`internal/prompt/{model,store,trigger,service,outcome}.go`,
`internal/consume/consume.go`, `internal/config/*`, `cmd/prompts/main.go`,
`internal/mcp/tools.go` (set/clear-trigger schemas only), `_test.go`.

**Done when:** a prompt can declare triggers on multiple sources, a non-cron
upstream event starts a run whose row carries the trigger context, manual `run`
is unchanged, and the gate is green.

## Phase 7 — MCP surface reshape

**Goal:** swap the external surface to the A8 table — bare verbs + `run_*`
namespace, `user_prompt` field, `prompt_id`/`run_id` keys — and rewrite
`describe`.

**Changes:**

- `tools.go`: replace the descriptor list with the A8 set; drop the `session_`
  subprefix; add `run_list`/`run_get`/`run_output`/`run_cancel`/`run_fs_list`/
  `run_fs_read`; rename the `create`/`update` wire field `prompt` → `user_prompt`;
  add `create`'s optional `triggers` array (`{source, event_filter}`, wired to the
  Phase 6 `Create` sugar); rewire `dispatchTool` to the A7 service methods
  (`run_*` cases parse `run_id`, bare cases parse `prompt_id`). `create` returns
  `{prompt_id, ...}`; `run` returns `{run_id, status, started_at}`. The
  `set_trigger`/`clear_trigger` schemas already took their A8 `{source,
  event_filter}` shape in Phase 6 — here just drop their `session_` prefix with
  the rest.
- Remove the now-unused prompt-keyed `Output`/`FsList`/`FsRead`/`Cancel` shims
  from `service.go` (the surface calls the run-keyed methods directly).
- `describe.go`: rewrite the overview text for the new model — prompt vs run, full
  concurrency, materialize→exec→persist, per-run sandbox, the `run_fs_*` readers,
  tombstone delete, and a worked `create → run → run_output → run_fs_read`
  example. (DECISIONS defers final wording, but it must describe *this* surface.)
- `mcp_test.go`: update all tool names, the `user_prompt` field, and `run_*`
  flows.

**Files:** `internal/mcp/tools.go`, `internal/mcp/describe.go`,
`internal/prompt/service.go` (shim removal), `internal/mcp/mcp_test.go`.

**Done when:** the served tool list matches A8 exactly, no `session_`-named tool
remains, and the gate is green.

## Phase 8 — Docs + integration verify

**Goal:** bring the prose in line and prove the whole flow end-to-end.

**Changes:**

- Rewrite `prompts/ARCHITECTURE.md` and `prompts/README.md` to the new model
  (prompt + first-class concurrent runs, per-run sandbox, materialize→exec→persist,
  tombstone delete, **multi-source triggers** + the consumed-upstream set, the A8
  surface). Remove every "session" / single-flight / "idle|running" /
  "cron-only trigger" description; document prompts and scripts as symmetric
  event-plane peers (A11/A12).
- Final gate (`build` + `vet` + `test`) plus a manual smoke against a built
  binary: `create` a prompt → `run` (capture `run_id`) → `run_get` until
  terminal → `run_output` → `run_fs_*`; a second `run` of the same prompt
  concurrently to confirm isolation; `set_trigger` on a non-cron source (e.g.
  `{dropbox, file.created}`) and confirm it is accepted and listed; `delete` the
  prompt and confirm the runs remain readable.
- Note in the deploy checklist: after shipping, **restart the dashboard** so it
  re-reads the prompts manifest (the tool set changed).

**Done when:** docs contain no stale `session` model, the smoke flow passes, and
the gate is green.

---

# Part C — Orchestrator rules

- **Run phases in order, one subagent each.** Do not start phase N+1 until phase
  N's gate is green.
- **Each subagent's first and last action is the gate.** First: confirm the tree
  is green before starting (catch a bad hand-off). Last: confirm green before
  reporting done. A red gate is a failed phase — fix within the phase or report
  the blocker; never hand a red tree to the next subagent.
- **Part A is law.** Code toward the frozen names/shapes verbatim. If a phase
  discovers Part A is wrong or under-specified, **stop and report** for a plan
  amendment rather than improvising a divergent shape.
- **Scope discipline.** A subagent does its phase's changes and the tests that go
  with them — nothing from a later phase, no opportunistic refactors. Keeping
  phases disjoint in intent is what keeps each one completable.
- **Migrations are edited in place** (no deployment, no data). Never add an
  `ALTER`-style migration; rewrite `002`/`003` toward A3.

---

# Out of scope (follow-ups, not this plan)

- **Sibling `scripts` PLAN deltas.** DECISIONS §"Implied scripts PLAN deltas"
  (tombstone delete; persist the executed body) edits
  `../scripts/scripts/PLAN.md`, which lives in a **different worktree/branch** and
  is itself still design-stage. Track it separately; it is not part of this
  prompts code change.
- Retention / GC of run directories.
- Sandbox seeding / cross-run carry-over (explicit non-goal).
- Surface 2 (the in-run toolset) — unchanged by this work.
- Final ratified per-field description wording beyond making `describe` and the
  schemas accurate to the A8 surface.
</content>
