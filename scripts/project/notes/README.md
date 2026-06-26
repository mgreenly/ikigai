# scripts

**Status: draft 1 — design settled; not yet built.** This folder holds the
design note. The architecture is the outcome of an exploration session; unlike
`agent` (which narrowed its first slice to run-once and deferred triggers), the
`scripts` slice includes **run-once, general event triggers, and completion
events** from the start, because those are the point of the service. See
Deferred for what is genuinely later.

## What scripts is

scripts is a ikigenba suite **service** (path-routed under `/srv/scripts/`,
loopback, behind the dashboard's nginx auth — same chassis as every other
service) whose **domain is deterministic Python scripts**. It is the sibling of
`agent`: where `agent` runs a non-deterministic LLM loop, `scripts` runs a
**deterministic `python3` process**. It lets the owner create, edit, run, and
supervise **scripts**, and to wire scripts to fire on **suite events**.

Under `/srv/scripts/` scripts serves two doors: an **MCP surface for agents**
(bearer-gated) and a **human web landing page** (dashboard-session-cookie-gated)
that shows the service name and version, alongside its event-plane
producer/consumer feeds. It still runs **no token logic** — nginx remains the
sole trust boundary for both doors.

scripts is the suite's **deterministic glue**. A script is a small Python
program that reacts to an event (a contact created, a ledger entry posted, a
cron tick) or runs on demand, does something exact and repeatable, and reports a
**success/failure completion event carrying its output** — which another script,
or `notify`, can in turn react to.

scripts is **non-interactive background work.** Interactivity lives in the
calling harness (Claude web / Cowork / Claude Code) that connects over MCP. You
*delegate* a script to scripts and either trigger it or subscribe it to events;
you do not chat with a run.

## Core concepts

- **Script** — a durable, owner-created definition: a `name`, the **`body`** (the
  literal Python source text), and a small `config` blob. The body and config
  are editable after creation. Stored in scripts' DB; the body is **not** a file
  on disk — it is a column, exactly as `agent` stores a session's `prompt`.
- **Run** — one execution of a script. A run is a **first-class, addressable
  instance** (ULID id): it is listable, inspectable, and cancellable while in
  flight. Each run materializes the script `body` (and its `config`) into a
  **fresh per-run dir**, executes `python3` there, and captures stdout/stderr.
  The whole run dir **persists** after it stops — its materialized inputs, output
  logs, and any files the script wrote are the durable record of exactly what
  ran. (Inputs that can change are pinned on disk per run, so editing or deleting
  the script later never alters a finished run.)
- **Trigger** — a `{source, event_filter}` binding, managed by MCP, that
  subscribes a script to a suite feed. When a matching event arrives, scripts
  spawns a run for that script, **passing the event to it**. A script may also be
  run on demand with no trigger.
- **Completion event** — every run that reaches a natural terminal state emits
  `scripts.succeeded` (exit 0) or `scripts.failed` (non-zero / TTL / spawn error)
  to scripts' own `/feed`, atomically with the run's terminal DB write. The
  payload carries the exit code, trigger context, and a **tail of the script's
  output**. A *cancelled* run emits **nothing**.

## The execution model

A run is deliberately simple — no LLM, no provider, no agent loop:

1. Materialize the script `body` (+ pinned `config`) into a fresh per-run dir
   (`data/runs/<run_id>/`).
2. Spawn `python3` in that dir, with the triggering event delivered **two ways**:
   on **stdin**, and mirrored in the **`$EVENT_JSON`** environment variable. For a
   manual run, stdin is empty and `$EVENT_JSON` is `{}` (always present, so the
   script never branches on existence).
3. Stream the process's **stdout** and **stderr** to live log files
   (`data/runs/<run_id>/{stdout,stderr}.log`), tailable mid-flight.
4. On exit: record terminal `status`, `exit_code`, `ended_at`; emit the
   completion event. The run dir persists whole — inputs, logs, and produced
   files — readable afterward via `run_output` and `run_fs_list`/`run_fs_read`.

The canonical script:

```python
import sys, os, json
ev = json.loads(os.environ["EVENT_JSON"])     # or: json.load(sys.stdin)
result = do_work(ev.get("payload", {}))
print(result)                                  # captured into the completion event
# exit 0 = scripts.succeeded ; non-zero = scripts.failed
```

## The runtime contract (advertised, not probed)

Discovery tells the script author — usually an LLM writing the `body` — exactly
what the box provides. These are **declared** static facts (surfaced in
`describe` and `health.details`), and `bin/setup` provisions the box to keep the
promise:

```
python_version: ">=3.11"     # 3.11+ : match/, exception groups, tomllib, modern typing
bash_version:   ">=5.0"      # 5.x   : reachable only via subprocess
network:        true         # scripts may call the network — no isolation
packages:       stdlib       # standard library only; no per-run pip install
```

The author codes against: **Python 3.11+, bash 5.x via `subprocess`, network
available, standard library only.** Because the network is open and there is no
provider, scripts has **no secret of any kind** — no `ANTHROPIC_API_KEY`, no
`bin/secrets`.

## Triggers — subscribe a script to any suite event

A trigger is a `{source, event_filter}` row, created/removed via MCP
(`set_trigger` / `clear_trigger`). `event_filter` is a glob:

- `{cron, "cron.nightly"}` — a clock.
- `{crm, "contact.created"}` — react to a new contact (its snapshot arrives in
  `$EVENT_JSON`).
- `{crm, "contact.*"}` — any contact change.
- `{agent, "run.failed"}` — react to an agent run failing.
- `{scripts, "scripts.succeeded"}` — chain off another script.

scripts boots **one consumer loop per upstream producer** (the `notify`
multi-cursor pattern, generalized), each with its own `feed_offset` cursor. A
loop's handler looks up which scripts subscribe to the incoming `(source, type)`
and fires them.

**Subscriptions are dynamic** (DB rows, MCP-managed); the **set of consumable
sources is static** (fixed at deploy via env-injected `SCRIPTS_<SRC>_FEED_URL`).
"Any event" means any event from any producer wired at build time — day-one:
`cron`, `crm`, `ledger`, `dropbox`, `agent`. A brand-new producer service later
needs a `scripts` redeploy to inject its feed URL (the suite defers runtime feed
discovery). `set_trigger` **validates** `source`/`type` against the known
producers' published registries and rejects a subscription nothing can satisfy.

## Concurrency — unbounded

Because runs use fresh ephemeral dirs, two runs of the same script cannot corrupt
each other, so scripts is **not** single-flight. **Every trigger spawns its own
run immediately**, N in flight at once: **one event → exactly one run**, no queue,
no collapse, no dropped records. `run` is always accepted; a script has no
`idle`/`running` lifecycle.

The only backstop is a **per-run TTL** (`SCRIPTS_RUN_TTL`) that kills a runaway
run → `scripts.failed` with "TTL exceeded." There is **no backpressure** — a
chatty feed × a slow script can spawn many concurrent `python3` processes. This
is a **known, accepted gap** (see Deferred); a bounded pool / queue is a later
refinement if it ever actually bites.

## Runs are first-class — observe and control instances

Unbounded concurrency means many live runs per script, so a run is individually
addressable (the opposite of `agent`, which is latest-run-only):

- **`run`** returns the spawned **`run_id`**.
- **`run_list {script_id?, status?}`** — the live roster (omit `script_id` for all
  the owner's in-flight runs); each entry carries `started_at`, **`elapsed_secs`**,
  status, and trigger context.
- **`run_get {run_id}`** — full detail: `started_at`, `ended_at`, `elapsed_secs`
  (computed), `exit_code`, trigger context.
- **`run_output {run_id, stream?, offset?, limit?}`** — tail **stdout/stderr as it
  runs** (`stream` = `stdout|stderr|both`); the same raw bytes the completion-event
  tail is cut from.
- **`run_cancel {run_id}`** — abort mid-flight: kill the `python3` process group →
  status `cancelled`, **no event emitted**.
- **`run_fs_list {run_id, subpath?}`** / **`run_fs_read {run_id, path, offset?,
  limit?}`** — list and read the run's persisted dir tree (the materialized
  inputs plus any files the script wrote). Run-scoped; path-guarded to the run
  root.

A script's `get`/`list` surface a live **`running_count`** — **derived** from the
`runs` table (`COUNT(*) WHERE status='running'`), never a stored counter, so it
**self-heals**: the boot crash-recovery sweep flips orphaned `running` runs →
`failed`, and any inflated count corrects itself with no reconciliation code.

## State and persistence

When a run stops, two things persist with distinct roles:

| thing | where | lifetime |
|---|---|---|
| run record (status, exit_code, times, trigger) | **DB** `runs` row | **persists** |
| run dir (materialized `body` + `config`, stdout/stderr logs, files the script wrote) | disk `data/runs/<run_id>/` | **persists** |

The DB row is the queryable index; the run dir is the payload — the exact inputs
that ran plus the outputs they produced. The row points at the logs
(`stdout_path`/`stderr_path`). Neither is redundant. (Pinning the inputs on disk
per run is why editing or deleting the script later can't alter a finished run.)

**Crash recovery** — on boot, after migrate, sweep `runs WHERE status='running'`,
mark them `failed` ("interrupted by restart"). Run dirs are **not** touched — a
crashed run keeps whatever partial tree it had.

**Retention is deferred.** Finished runs (row + logs) accumulate **unbounded** for
now. A GC sweep with its own MCP tools is a later, unplanned effort (see
Deferred), not in this build.

## Completion events — the contract scripts publishes

Two event types on scripts' `/feed`, emitted atomically with the run's terminal
write (then `Ring()`):

- `scripts.succeeded` — exit code 0.
- `scripts.failed` — non-zero exit, TTL expiry, or spawn error.

Payload:

```json
{
  "script_id":         "01J...",
  "script_name":       "enrich-contact",
  "run_id":            "01J...",
  "status":            "succeeded",
  "exit_code":         0,
  "trigger":           { "source": "crm", "type": "contact.created", "event_id": "01J..." },
  "stdout":            "...last 8 KB...",
  "stdout_truncated":  false,
  "stderr":            "...last 8 KB...",
  "stderr_truncated":  false,
  "error":             "run TTL exceeded"
}
```

- Each stream is capped to its **last 8 KB** with a `*_truncated` flag; the
  **full** output stays pullable via `run_output`. The event carries a digest; the
  log carries everything.
- `error` is present only on `scripts.failed`.
- For a manual run, `trigger` fields are empty.
- A **cancelled** run emits **no event** — an operator abort is not a script
  outcome and must not fire downstream chains.

## Delivery — pull and push

- **Pull** — the foreground agent surfaces nothing automatically. When *you* ask,
  it reads a run's status (`run_get`) or output (`run_output`) over MCP.
- **Push (events)** — a completion event on `/feed` lets *other* services react
  without anyone polling: another script chained on `scripts.succeeded`, or
  `notify` turning a `scripts.failed` into a push.

## Decided (the draft-1 ledger)

1. **Script** = durable definition: `name` + **`body` (Python source, in the DB)** +
   `config` blob. Edited via MCP, exactly as `agent` stores a `prompt`.
2. **Run** = one execution in a **fresh per-run dir** that **persists** (its
   materialized `body`+`config`, output logs, and any produced files); the run
   **record** (DB) persists alongside it. Runs are **first-class, addressable
   instances**, their dir readable via `run_fs_list`/`run_fs_read`.
3. **Event in, two ways** — the triggering event is delivered on **stdin** and in
   **`$EVENT_JSON`** (`{}` for a manual run).
4. **Triggers are general** — `{source, event_filter}` over **any deployed
   producer**, MCP-managed; one consumer loop per upstream. `set_trigger`
   validates against known registries.
5. **Concurrency is unbounded** — one event → one run, no single-flight, no queue;
   per-run TTL is the only backstop. Backpressure deferred.
6. **Completion events carry output** — `scripts.succeeded`/`scripts.failed` with
   exit code, trigger context, and an 8 KB output tail. Cancelled → no event.
7. **Runtime is advertised, not probed** — `python>=3.11`, `bash>=5.0`,
   `network:true`, `packages:stdlib`. `setup` provisions to match.
8. **No secrets** — network is open, no provider, so the service holds none.
9. **Retention deferred** — runs accumulate unbounded; GC is later, unplanned.
10. **Delete is a tombstone** — `delete` removes the script row + its triggers
    only; runs and their on-disk artifacts survive as append-only history (suite
    grain: ledger journal, append-only outbox). Reclaimed only by future GC.

## MCP tool surface (draft 1)

**Identity / discovery**
- `health` — chassis proof; `{status, version, service, owner_email, client_id, details}` where `details` carries the runtime contract.
- `describe` — service overview + the authoring contract (runtime, event delivery, output capture rules).

**Script lifecycle**
- `create` — `{name?, body, config?}` → `{script_id}`.
- `list` — owner's scripts, each with `running_count`, `last_run`.
- `get` — `{script_id}` → full detail incl. `body`, `running_count`, `last_run`.
- `update` — `{script_id, name?, body?, config?}`.
- `delete` — `{script_id}` → `{ok}`. **Tombstone:** removes the
  script row and its triggers; runs + their on-disk artifacts survive as history.

**Triggers**
- `set_trigger` — `{script_id, source, event_filter}`; validated against known producers.
- `clear_trigger` — `{script_id, source, event_filter}`.

**Run (on demand)**
- `run` — `{script_id}` → `{run_id, status:"running", started_at}`. Always accepted.

**Run instances (observe + control)**
- `run_list` — `{script_id?, status?}` → live roster with `elapsed_secs`, trigger.
- `run_get` — `{run_id}` → status, exit_code, times, `elapsed_secs`, trigger.
- `run_output` — `{run_id, stream?, offset?, limit?}` → stdout/stderr by line range, tailable.
- `run_cancel` — `{run_id}` → status `cancelled` (no event).
- `run_fs_list` — `{run_id, subpath?}` → the run dir's file tree.
- `run_fs_read` — `{run_id, path, offset?, limit?}` → a file in the run dir.

## Deferred (explicitly later)

- **Backpressure / overlap policy** — bounded concurrency pool or queue-one-deep;
  day-one is unbounded with a per-run TTL only.
- **Run retention / GC** — MCP-driven sweep of old run rows + logs.
- **Third-party packages** — a per-script dependency set (vendored or pip);
  day-one is stdlib-only.
- **Sandbox isolation hardening** — bwrap / rootless podman; day-one is a plain
  `python3` with network, no OS sandbox (a platform concern, as for `agent`).
- **Runtime feed discovery** — dynamically wiring a new producer without a
  `scripts` redeploy.
- **Non-Python interpreters** — `config.interpreter` is reserved but day-one is
  `python3` only.

## Reuses vs new

**Reuses (clone/borrow):**
- ikigenba **chassis** from `../agent` / `../ledger` (loopback bind, nginx
  fragment, identity gate, PRM doc, the `health` MCP tool, `bin/*` lifecycle,
  SQLite + migrations, MCP JSON-RPC).
- **runner / trigger / consumer / outcome** shapes from `../agent` (the single
  service that already does triggers + completion events) — adapted from
  cron-only + single-flight to general-source + unbounded.
- **event plane** library `../eventplane` (producer `outbox` + consumer engine) —
  used directly, both as producer (completion events) and consumer (triggers).

**New (what scripts forces on the system):**
- **First-class run instances** — addressable, listable, cancellable, with live
  output tailing. No other service exposes runs this way.
- **General `{source, filter}` triggers** — `agent` consumes cron only; scripts
  fans in over all deployed producers.
- **A `python3`-exec runner** — no LLM, no provider; just process spawn, pipe
  capture, TTL, and process-group cancel.

## Sources (read before coding)

**Suite (this repo):**
- `../agent/README.md` + `../agent/ARCHITECTURE.md` — the template: sessions,
  runner, triggers, outcome events.
- `../notify/` — the multi-upstream **consumer** pattern (one cursor per source).
- `../cron/` — a dynamic **producer** (events computed at runtime).
- `../crm/` — the static **producer** (outbox wiring).
- `../eventplane/` — the producer/consumer library.
- `../docs/event-protocol.md` — the normative event wire contract.

**Platform (sibling `metaspot` repo — authoritative; on conflict it wins):**
- `../../metaspot/AGENTS.md`, `../../metaspot/docs/path-routing-architecture.md`.
</content>
</invoke>
