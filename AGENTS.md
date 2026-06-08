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
verb set (`serve`/`version`/`manifest`/`migrate`/`backup`/`restore`); deploys
land in versioned release directories with an atomic swap and one-command
rollback. Services don't call each other as private API chains — they publish
facts to an append-only outbox and consume each other's `/feed` over SSE (the
event plane). The **infrastructure** half (Terraform: AWS accounts, DNS, the
box itself) lives in the sibling repo `../metaspot`, whose specs are
authoritative — on any conflict, read `../metaspot/AGENTS.md` and
`../metaspot/docs/` and let them win. This file is the introduction; the
specifics live in the folders below and in `docs/`.

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
| **appkit** | Shared Go **chassis** library: config-from-env, migration runner + downgrade guard, loopback server, `/feed`, manifest emit/parse, the verb dispatcher (consumed via a committed `replace`). |
| **eventplane** | Shared Go **library** — the event-plane producer/consumer plumbing (committed `replace`). |
| **agentkit** | Shared Go **library** for the agent-backed services (prompts/dropbox/wiki). |
| **opsctl** | The **on-box CLI** (Go) that owns every box-side operation — stage, deploy, rollback, prune, status, provisioning; built here and installed to `/usr/local/bin/opsctl`. |
| **bin** | Shared repo-root **operator scripts** — the off-box build/version tooling (`ship`, `bump`, …). |
| **nginx** | Local-dev front door on **:8080** mirroring the prod `/srv/<svc>/` routing (`./run`). |
| **docs** | Suite-level docs: the deployment ADR, versioning how-to, runbooks, and the event-plane protocol write-ups. |

The seven deployable apps are **dashboard, crm, ledger, notify, dropbox, prompts,
wiki**; `appkit`/`eventplane`/`agentkit` and `opsctl` are libraries/tooling and
are **not** versioned. The root `go.work` wires the modules for local dev; the
production build forces `GOWORK=off`.

## Deploying — bump → ship → stage → deploy

We deploy to **`int.ikigenba.com`** (the first and only account, `int`). Your
`~/.ssh/config` already has a `Host int.ikigenba.com` (alias `int`) entry pinning
the right key, so `ssh int` and the deploy scripts connect with the correct
identity automatically — no `-i` flag needed.

Deploy ships one static binary into a versioned release dir — **not** `git push`
and **not** an in-place overwrite. Four steps:

1. **`bin/bump <app> <major|minor|patch>`** — advance the committed bare-SemVer
   `<app>/VERSION` on `main` (the single source of truth) and push it. Skip if
   the version is already where you want it.
2. **`bin/ship <app>`** — the off-box half. Builds current `main` (HEAD) as a
   static `linux/amd64` binary in a throwaway git worktree, `scp`s the artifact
   to the box `/tmp`, then **prints the two box commands and stops** — it makes
   no other change on the box.
3. **`ssh int sudo opsctl stage <app> v<ver> --artifact /tmp/<app>-v<ver>`** —
   preflight + SHA collision guard, place the binary into `releases/<ver>/`, and
   delete the `/tmp` artifact on success. Stages a release without making it live.
4. **`ssh int sudo opsctl deploy <app> v<ver>`** — regenerate `etc/manifest.env`
   from the new binary, back up the DB if the schema advances, `migrate`, atomic
   swap `current`, restart the unit, and prune old releases.

Roll back with **`ssh int sudo opsctl rollback <app> [ver]`**. Inspect with
`opsctl status` / `opsctl releases <app>`; follow logs with `opsctl tail <app>`.
A brand-new service needs `opsctl setup <app>` first (and `opsctl init-box` once
per box). After deploying a new MCP service, restart the dashboard so it
re-reads the manifests.

The full deploy model is `docs/adr-deployment-redesign.md`; versioning is
`docs/versioning.md`; per-service details live under each service's own
directory and `CLAUDE.md`.

## Migrations — timestamped and immutable

Each service owns its schema as ordered SQL files under
`<service>/internal/db/migrations/`, applied forward-only by the appkit runner
and tracked individually in `schema_migrations`. Two hard rules:

- **Never hand-pick a migration number, and never write one by hand.** Run
  `bin/new-migration <service> <name>`; it stamps a UTC timestamp version
  (`YYYYMMDDHHMMSS_name.sql`). Timestamps are why two agents on two branches
  don't collide — sequential integers did, and the clash only surfaced at
  deploy. (Legacy `NNN_*.sql` files predate this and stay frozen; they sort
  before any timestamp, so the two coexist.)
- **Never modify or delete a committed migration.** Once a migration is on
  `main` it is immutable — the runner keys on its version and will silently skip
  an edited body, so the change reaches new databases but not existing ones.
  Change schema by adding a *new* migration. `bin/check-migrations` enforces
  both rules in CI (no duplicate versions, no edits to existing files).

See `docs/adr-migration-timestamps.md`.
