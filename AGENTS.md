# suite

The **suite** is the deployable **application suite** for ikigenba — one **dashboard**
plus N small **services** that run together on a single box, one box per
customer, answering on the apex `<account>.ikigenba.com`. It is a **single
mono-repo** (one `.git`): the dashboard, every service, the shared libraries,
the local-dev nginx front door, and the docs are all subdirectories of this one
repo, not independent projects. The dashboard owns identity (OAuth authorization
server, IAM, grants, install landing, service inventory); each service owns one
domain, its own SQLite database, and a loopback HTTP server. nginx is the sole
trust boundary: it introspects every `/srv/<svc>/` request against the
dashboard, strips the prefix, and forwards it to the loopback service with
trusted `X-Owner-Email` / `X-Client-Id` headers that services accept blindly —
so services run **no UI and no token logic**. The product surface is **MCP**:
users connect an agent and work through tools, not screens.

The operating bet is that business software which can accept short, scheduled
downtime gets to be cheaper, simpler, and easier to recover — no cluster, no
broker, no zero-downtime machinery. Every app is **one static `linux/amd64` Go
binary** built on the shared `appkit` chassis over SQLite, implementing a fixed
verb set (`serve`/`version`/`manifest`/`migrate`/`schema`); deploys
land in versioned release directories with an atomic swap and one-command
rollback. Backup and restore are **not** binary verbs — they are box-level
operations owned by `opsctl` (S3 snapshots of `state/`). Services don't call each other as private API chains — they publish
facts to an append-only outbox and consume each other's `/feed` over SSE (the
event plane). The **infrastructure** (Terraform for the `int.ikigenba.com` box —
AWS accounts, DNS, the box itself) is managed separately in `~/projects/metaspot`
and is not part of this repo. This file is the introduction; the specifics live
in the folders below and in `docs/`.

## Top-level layout

| dir | what's in it |
|---|---|
| **dashboard** | The apex/`DEFAULT` app: OAuth authorization server, IAM, grants, push, install landing, and service inventory (Go, SQLite). |
| **crm** | Path-routed service `/srv/crm/` — contacts domain + MCP; event-plane **producer** (`/feed` outbox). |
| **ledger** | Path-routed service `/srv/ledger/` — double-entry bookkeeping (immutable journal, fixed-verb MCP); event-plane **producer**. |
| **notify** | Path-routed service `/srv/notify/` — event-plane **consumer** (push) and the worked example for bringing up a new consumer. |
| **dropbox** | Path-routed service `/srv/dropbox/` — keeps a private local mirror in sync with a single Dropbox app folder; loopback daemon + event-plane **producer**. |
| **prompts** | Path-routed service `/srv/prompts/` — runs sandboxed Claude agent sessions on the owner's behalf, exposed as MCP; event-plane **producer** + **consumer** (self-chaining). |
| **wiki** | Path-routed service `/srv/wiki/` — knowledge base: ingest / search / ask (RAG) + MCP. |
| **cron** | Path-routed service `/srv/cron/` — loopback scheduled-event emitter; event-plane **producer** (emits scheduled tick events). |
| **gmail** | Path-routed service `/srv/gmail/` — loopback Gmail connector + MCP; event-plane **producer**. |
| **scripts** | Path-routed service `/srv/scripts/` — runs deterministic Python scripts wired to suite events; event-plane **consumer** + **producer** (completion events). |
| **sites** | Path-routed service `/srv/sites/` — loopback static-website host (file-backed) + MCP. |
| **webhooks** | Path-routed service `/srv/webhooks/` — loopback inbound-webhook receiver: owner-facing MCP (create/list/delete/rotate) plus a public `POST /in/<name>` ingress self-guarded by a per-webhook secret; event-plane **producer** (`/feed` outbox). |
| **appkit** | Shared Go **chassis** library: config-from-env, migration runner + downgrade guard, loopback server, `/feed`, manifest emit/parse, the verb dispatcher (consumed via a committed `replace`). |
| **eventplane** | Shared Go **library** — the event-plane producer/consumer plumbing (committed `replace`). |
| **opsctl** | The **on-box CLI** (Go) that owns every box-side operation — stage, deploy, rollback, prune, status, provisioning; built here and installed to `/usr/local/bin/opsctl`. |
| **bin** | Shared repo-root **operator scripts** — the off-box build/version tooling (`ship`, `bump`, …). |
| **nginx** | Local-dev front door on **:8080** mirroring the prod `/srv/<svc>/` routing (`./run`). |
| **docs** | Suite-level docs: the deployment ADR, versioning how-to, runbooks, and the event-plane protocol write-ups. |

The twelve deployable apps are **dashboard, crm, ledger, notify, dropbox,
prompts, wiki, cron, gmail, scripts, sites, webhooks**; `appkit`/`eventplane`
and `opsctl` are libraries/tooling and are **not** versioned. The agent chassis
lives in its own repo, `github.com/ikigenba/agentkit`, consumed as a tagged
module. The root `go.work` wires the modules for local dev; the
production build forces `GOWORK=off`.

## Working locally

This is a **mono-repo**, and even when you're started at the repo root you are
**almost always working in exactly one subfolder** (one service or library) for
the entire unit of work. Stay inside it. **Everything that belongs to that unit
of work lives under that subfolder** — its code, its schema
(`<svc>/internal/db/migrations/`), its `.envrc`/`CLAUDE.md`, **and its docs,
including the design/plan/ralph workspace (`<svc>/project/`)**. Do not create or
edit files at the repo root for work that belongs to a service; the root
`docs/` is suite-level only.

**If you don't know which subfolder a piece of work belongs to, ask — do not
default to the repo root.** Changing the wrong directory, or scattering a
service's files across the root, causes real problems.

The suite-wide concerns below — running the suite, the nginx front door, deploy
tooling — are the only exceptions that legitimately live at the root.

Testing usually needs the **whole suite running together**, so that happens from
the root, not per-service:

- **`bin/start`** native-builds every service, launches each on its loopback
  port, and brings up the nginx front door on **:8080** so the full path-routed
  auth chain is reachable end to end. Logs land in `tmp/<svc>.log`.
- **`bin/stop`** tears the whole stack back down; **`bin/stop --clean`** also
  wipes the `tmp/` dev state (binaries, logs, SQLite dbs).

> ⚠️ **You may only stop services you started from the worktree you are working
> in.** The suite binds shared host ports (`:3000`–`:3006`, `:8080`), so a suite
> launched from a *different* worktree or clone can be occupying them. Stopping
> services — `bin/stop`, `kill`/`pkill`, freeing a port, or anything that
> terminates a process — is permitted **only** for the stack this worktree's own
> `bin/start` launched. **Any** action against a process you did not start from
> this worktree — including one merely holding a port you want (e.g. a stale
> nginx on `:8080`) — requires **explicit, specific operator approval first**.
> When a needed port is occupied by something this worktree didn't start, do
> **not** kill it to clear the way: identify the owning process (`ss -ltnp`,
> check the binary path), **stop, and surface it** — name the process and the
> owning directory and ask how to proceed. "It's in my way" is never permission;
> a port conflict is a question for the operator, not an obstacle to bulldoze.

With the suite up, you should normally have the local **`ikigenba_<svc>` MCP
tools** available and reachable against the running services. If those tools are
missing from your toolset, or a service's `health` check fails, **complain
prominently** — don't proceed as if testing succeeded — then continue with
whatever parts of the work are still doable. A missing MCP usually just means the
suite isn't up (run `bin/start`).

## Deploying

> ⚠️ **`int.ikigenba.com` is the live account.** Do **not** `ssh int` / invoke
> `opsctl` against the box, even read-only, unless you've been **explicitly told
> to deploy**. The default workflow is local-only (`bin/start`); deploying is a
> separate, explicit request.

> ⚠️ **TEMPORARY (migration window only — REMOVE this note once migration is
> done): there is NO live/production customer data yet.** Until migration
> completes, every service's `state/` is disposable — no real customers, no data
> to protect. Do **not** reason about deploy, `convert`, migrations, or `opsctl`
> as if they must guard against data loss; do **not** justify anything by
> "protecting customer data." State can be wiped and rebuilt at will.
> **This flips the moment migration finishes** — after that there WILL be real
> customer data and this whole note must be deleted.

The full deploy runbook — the `bump → ship → stage → deploy` sequence, rollback,
and inspection commands — lives in **`deploy.md`** at the repo root.

## Migrations — timestamped and immutable

Each service owns its schema as ordered SQL files under
`<service>/internal/db/migrations/`, applied forward-only by the appkit runner
and tracked individually in `schema_migrations`. Two hard rules:

- **Never hand-pick a migration number, and never write one by hand.** Run
  `bin/create-migration <service> <name>`; it stamps a UTC timestamp version
  (`YYYYMMDDHHMMSS_name.sql`). Timestamps are why two agents on two branches
  don't collide — sequential integers did, and the clash only surfaced at
  deploy. (Legacy `NNN_*.sql` files predate this and stay frozen; they sort
  before any timestamp, so the two coexist.)
- **Never modify or delete a committed migration.** Once a migration is on
  `main` it is immutable — the runner keys on its version and will silently skip
  an edited body, so the change reaches new databases but not existing ones.
  Change schema by adding a *new* migration.

## Designing and planning work

This is our process for designing and planning a piece of work before we build
it. In short: each piece of work moves through paired documents in
`docs/` that share one slug — **`<slug>-design.md`** (the how + decisions) then
**`<slug>-plan.md`** (ordered phases, each sized for one subagent, run
sequentially), after which a coordinator reads the plan in full and `/finish`es
it.

Two optional document types extend the pair:

- **`<slug>-research.md`** — when we do research *before* design, it lands here
  and feeds the design doc.
- **`<slug>-verification.md`** — an occasional special-case doc describing a
  post-work verification step, for work that warrants explicit validation after
  it's built.
