# ikigai

The deployable **application suite** for metaspot — the dashboard plus its
services that run *on* a metaspot box. `ikigai` is a **single mono-repo** (one
`.git`): the dashboard, the path-routed services, the shared `eventplane`
library, the local-dev `nginx` front door, and `docs` all live here as
**subdirectories of this one repo** — they are *not* independent repos. The only
related project that lives **outside** ikigai is `marketplace`
(`mgreenly/marketplace`, public, GitHub-hosted) — a plugin marketplace for
delivering skills/commands (and Claude Code MCP config) to Claude clients,
**not** an on-box service.

The **infrastructure** half of the system lives in the sibling repo
`../metaspot` (Terraform: AWS org/accounts, DNS, the one-box-per-customer
platform). metaspot builds the box; ikigai is what gets installed on it.

## Authoritative specs live in metaspot, not here

Do not re-derive the architecture — read these first; on conflict, they win:

- `../metaspot/AGENTS.md` — platform spec (servers, launcher, SSM `app-config`
  secrets, per-account backup buckets, the Service layer).
- `../metaspot/docs/path-routing-architecture.md` — server-side topology + the
  nginx `auth_request` → dashboard introspection **auth contract** every service
  lives under.
- `../metaspot/docs/connector-and-install.md` — the suite plugin + client-side
  install/connector layer.

## The model in one paragraph

One box per customer answers on the apex `<account>.metaspot.org`. The suite is
one **dashboard** (apex/`DEFAULT` app — OAuth authorization server, IAM, push,
install landing) plus N **services** (pure REST + MCP, **no UI**, **no token
logic**), all bound to loopback, routed by **path** under the reserved `/srv/`
prefix. nginx is the sole trust boundary: it introspects every `/srv/<svc>/`
request against the dashboard's `/internal/authn` and injects `X-Owner-Email` /
`X-Client-Id`, which services trust blindly. The `/srv/<svc>/` prefix is stripped
before proxying, so a service's internal routes stay `/contacts`, `/mcp`, etc.

## The projects

All of these are **subdirectories of this one repo**, except `marketplace`
(the lone separate GitHub repo, listed last for context).

| dir | role | runtime | loopback port |
|---|---|---|---|
| **dashboard** | apex/`DEFAULT` app: OAuth AS, IAM, push, install landing, service inventory | Go (`module dashboard`), SQLite (`dashboard.db`) | **3000** |
| **crm** | path-routed service `/srv/crm/` — contacts domain + MCP; event-plane **producer** (`/feed` outbox) | Go (`module crm`), SQLite | **3001** |
| **ledger** | path-routed service `/srv/ledger/` — double-entry bookkeeping (immutable journal, fixed 8-verb MCP) + event-plane **producer** (`/feed` outbox) | Go (`module ledger`), SQLite | **3002** |
| **notify** | path-routed service `/srv/notify/` — event-plane **consumer** (push on crm's `/feed`); the worked example for bringing up a new consumer | Go (`module notify`) | **3003** |
| **eventplane** | shared Go **library** — the event-plane producer/consumer plumbing, consumed via a committed `replace eventplane => ../eventplane` | Go (`module eventplane`) | — |
| **nginx** | local-dev front door (:8080) mirroring the prod `/srv/<svc>/` routing | nginx conf + `run` script | — |
| **docs** | suite-level docs (the event-plane protocol/decision write-ups) | Markdown | — |
| **marketplace** *(separate repo)* | plugin marketplace — delivers skills/commands (and Claude Code MCP config) to Claude clients; **not** an on-box service | GitHub-hosted (`mgreenly/marketplace`, public) | — |

The Go modules (`crm`, `dashboard`, `eventplane`, `ledger`, `notify`, plus
`ralph`, `dropbox`, `wiki` and the shared libs `agentkit`/`appkit`) are wired
together for local dev by the root `go.work`; the production build (the shared
repo-root `bin/deploy`) forces `GOWORK=off` and does **not** depend on it (see
Deployments).

Each service exposes a no-side-effect `<svc>_whoami` MCP tool (proves the
plugin → connector → dashboard OAuth → service chain end to end). Services keep
their own per-service audit store; the dashboard audits auth/token/grant events.

## Per-service conventions (from the metaspot Service layer)

- Install root `/opt/<app>/`, dedicated `--system` user, single entrypoint
  `/opt/<app>/bin/run` (a stable symlink → `current/<app>`, the active release's
  binary).
- **The app *is* one static binary** implementing the fixed appkit verb set
  (`<app>` serve, `version`, `manifest`, `migrate`, `schema`, `backup`,
  `restore`) — there is no per-service `bin/build`/`bin/deploy`/`bin/setup` clone
  anymore. Building and shipping is the *shared* repo-root `bin/deploy <app>
  [version]` → on-box `optctl`; provisioning is `optctl setup <app>` /
  `optctl init-box` (see Deployments). The only `bin/*` scripts a service still
  carries are operator-side glue with no optctl verb yet: `start`/`stop` (systemd
  control), `secrets` (SSM seeding), and `teardown` (dashboard/ralph).
- `etc/manifest.env` — flat `KEY=value`, carries `MOUNT=/srv/<svc>/` + `PORT`;
  dashboard is `DEFAULT=true`. **The binary is the source of truth: `<app>
  manifest` emits it, and `optctl install` regenerates `/opt/<app>/etc/
  manifest.env` from the new binary on every swap** (round-tripping the role keys
  `FEED`/`CONSUMES` and any service config the dashboard + `bin/registry` read).
- `etc/nginx.conf` — the service's `/srv/<svc>/` location fragment. The dashboard
  owns the apex `server{}` block, the cert, and the `/_authn` hook; `optctl setup
  <app>` drops the service's fragment into
  `/etc/nginx/conf.d/locations/<svc>.conf` (the box-global apex/cert/`/_authn`
  pieces are `optctl init-box`).
- Secrets via SSM `/metaspot/<env>/app-config` (never in Terraform/source).

## Local dev

`nginx/run` is the dev front door on **:8080** — it computes its own dir, proxies
the apex to dashboard (:3000), and includes `nginx/locations/*.conf` (the dev
mirror of each prod `/srv/<svc>/` fragment, currently crm, ledger + notify). Run
each Go service on its loopback port, then `cd nginx && ./run`.

## Deployments (how to ship to the box)

Production is the box at `<account>.metaspot.org` (first/only account: **ai**).
**Deploy ships one static binary into a versioned release dir, not `git push`
and not an in-place overwrite.** The repo has a GitHub remote (`origin` →
`mgreenly/ikigai`) for version-control backup, but pushing there ships nothing to
the box; shipping is the shared `bin/deploy` wrapper + on-box `optctl` below. Work
the services in dependency order and verify on the box after each step. The
canonical description of this model is `docs/adr-deployment-redesign.md`.

**The deploy model in one paragraph.** An ikigai app is **exactly one
self-contained static `linux/amd64` Go binary** implementing the fixed appkit verb
set: `<app>` (serve — the default), `version` (self-reports `<ver> (<sha>[-dirty]
)`), `manifest` (emit this app's `manifest.env`), `migrate`, `schema`, `backup`,
`restore`. There is **no `run` wrapper and no bundled `registry`** in the
artifact. The uniform chassis (config-from-env, the migration runner + downgrade
guard, the loopback server + PRM + identity gate, `/feed`, manifest emit/parse,
the verb dispatcher) lives in the shared `appkit` library, consumed via a
committed `replace appkit => ../appkit` (like `eventplane`/`agentkit`); **libs are
never tagged.**

**Versioning lives in a committed `<app>/VERSION` file, not git tags.** Each of
the seven deployable services carries a committed `<app>/VERSION` — a **bare**
SemVer number (e.g. `0.1.1`, no leading `v`), one line, the single source of
truth. `bin/bump <app> <major|minor|patch>` advances it: it reads the file,
increments the field, writes the new bare number back, makes a **path-limited**
commit of only that file directly to `main`, then `git push origin main`. Libraries
(`appkit`/`eventplane`/`agentkit`) and `optctl` are **not** versioned. **Git tags
are no longer the version mechanism** — no `git tag <app>/vX.Y.Z`, no
`git describe`; any surviving release tags are vestigial and drive nothing. The
version state lives in `main`'s commit history, made durable/immutable by branch
protection (a GitHub ruleset blocking force-push + branch deletion, requiring
linear history). The full how-to is `docs/versioning.md`.

**Build + ship is the *shared* repo-root `bin/deploy <app>`** (no version arg). It
owns the off-box build half only: it builds **current `main` (HEAD)** in a
throwaway detached `git worktree` (`CGO_ENABLED=0 GOOS=linux GOARCH=amd64
GOWORK=off -trimpath -buildvcs=false`, ldflags stamping
`appkit.version`/`appkit.commit`), reads the version from **that worktree's**
`<app>/VERSION` and prepends `v`, `scp`s the single artifact to the box `/tmp`,
then runs `ssh sudo optctl install <app> v<version> --artifact …`. No install logic
runs on the laptop. The commit SHA stamped into the binary (`appkit.commit`) pins
the app code and its committed `replace … => ../<lib>` library trees together —
one commit = one reproducible build (the job a tag used to do). `-buildvcs=false`
stays mandatory (the module is a subdir of a bare mono-repo `.git`, so Go's auto
VCS stamp would abort) and `GOWORK=off` keeps the prod build deterministic.

**`optctl` (on-box, `/usr/local/bin/optctl`, run via `sudo`) owns everything on
the box** — release dirs, atomic swap, migrate, restart, rollback, prune, and
box/app provisioning. Verbs: `install · rollback · prune · setup · init-box`
(`backup`/`restore` are folded into each app binary as verbs; a standalone
`optctl backup`/`restore` orchestration verb does **not** exist yet — see the gap
note below). Layout on the box:

```
/opt/<app>/
  releases/<version>/<app>      # the static binary for that version
  current -> releases/<version> # ln -sfn = ATOMIC swap
  bin/run -> ../current/<app>   # STABLE path metaspot-launch execs (unchanged)
  etc/manifest.env              # regenerated by `<app> manifest` on every swap
  data/<app>.db                 # state — NEVER touched by deploy
```

- **`optctl install <app> <ver> --artifact …`** — preflight (static? amd64?
  `<app> version` matches the arg? `<app> manifest` parses?) → place into
  `releases/<ver>/` → regenerate the stable `etc/manifest.env` from the new binary
  → **back up the DB if the schema advances** → `migrate` → atomic swap `current`
  → restart the unit → `is-active` → prune old releases. Never touches
  `data/<app>.db` except to migrate/snapshot it; migrations are forward-only.
- **`optctl rollback <app> [ver]`** — repoint `current` → the prior (or named)
  release → restart. If the rolled-back-from release advanced the schema, it
  **restores the pre-migration DB snapshot first** (the downgrade guard otherwise
  refuses to boot the older binary).
- **`optctl prune <app> [--keep N]`** — bound on-box release history; never
  deletes `current` or its rollback predecessor.
- **`optctl setup <app>`** — first-time per-app provisioning: the `--system` user,
  the `/opt/<app>` tree, the enabled-not-started systemd unit
  (`ExecStart=/usr/local/bin/metaspot-launch <app>`), the nginx fragment into
  `/etc/nginx/conf.d/locations/<app>.conf` (`nginx -t` + reload). Required once
  before a brand-new service's first `install`.
- **`optctl init-box`** — one-time box-global substrate (nginx + certbot + the one
  apex cert + renewal timer, the apex `server{}` block, `/_authn`, the
  `conf.d/locations/` include dir). The box-global half the dashboard's old
  overloaded `setup` used to bootstrap.

The stable paths `/opt/<app>/bin/run` (a symlink into `current`) and
`/opt/<app>/etc/manifest.env` (regenerated from the binary) stay valid at all
times, including mid-swap — so the baked `metaspot-launch`, `bin/registry`, and
the dashboard's manifest-derived resource list are all untouched.

**Routing/auth config per service lives in `etc/deploy.env`** (non-secret):
`ACCOUNT`, `SSH_USER` (`ec2-user`), `SSH_KEY` (`~/.ssh/id_ed25519_ai4mgreenly`);
`HOST` defaults to `${ACCOUNT}.metaspot.org`. AWS = `--profile ${ACCOUNT} --region
us-east-2`; SSM/secrets steps need a live SSO session (`aws sso login --profile
ai` — interactive, the user runs it; the token expires).

> **A migration is immutable once it has been applied to any live DB.** The
> migration runner keys solely on the integer version: it applies versions not
> yet recorded in `schema_migrations`, skips those already there, and refuses to
> start if the DB carries a version the binary no longer embeds (downgrade
> guard). So **editing the body of an already-applied migration in place is
> silently ignored on existing DBs** — the new SQL only ever runs against a fresh
> DB. Once a migration has shipped, treat it as frozen: every schema change is a
> **new, higher-numbered, additive** migration (`ALTER`/`CREATE`, never a
> re-`CREATE` of an existing table). Rewriting an applied migration is only safe
> pre-production, and then only by resetting (backup + drop) every DB that ran
> the old body. (This bit crm: `002_crm.sql` was rewritten greenfield-style after
> the `ai` box had already applied the old v2, so the box needed a backup+reset
> rather than a plain deploy — the now-standard pattern, since **no DB needs
> preserving** (2026-06-05).)

> **Dashboard cutover = reset + install (no DB preservation).** Per the
> 2026-06-05 directive **no databases need to be preserved**, so the dashboard
> box cutover is the same backup+reset pattern as every other DB. Adopting
> appkit's **integer-keyed** migration runner renumbered the dashboard's
> migrations from their old **name/timestamp-keyed** scheme
> (`schema_migrations.name`) to `NNN_*.sql` (+ a new `001_schema_migrations.sql`).
> A *fresh* DB migrates correctly (verified, v5); the live `ai` box's
> `/opt/dashboard/data/dashboard.db` applied the OLD name-keyed ledger, which the
> integer runner will not recognize — so a plain `optctl install` against the
> **existing** DB would fail to boot. That is exactly why the cutover includes a
> one-time DB reset: **stop → (optional backup) → drop/reset the DB → `optctl
> install` (the fresh DB migrates clean to v5) → restart → verify**. Off-box code
> needs no change; this is purely the box-DB reset. The operator deliverable is
> `docs/runbook-dashboard-box-cutover.md`.

**Secrets** — single SSM SecureString `/metaspot/<account>/app-config`: a JSON
blob keyed per app. `metaspot-launch` extracts `.["<app>"]` at every start,
exports each `KEY=value`, and **hard-fails if the key/param is missing** — so
secrets must be seeded *before* first start. `bin/secrets` (operator-side, on the
services that need it — dashboard, notify, dropbox, ralph, wiki) does a
**non-destructive read-modify-write of only its own key**, values pulled from
`~/.secrets/<NAME>` (never read/printed; masked in the summary), siblings
preserved. The launcher injects them into the process env only — never on disk.
(Secrets seeding has no `optctl` verb — it stays an operator-side script.)

**Registering a new MCP service with the dashboard is now automatic — no env
edit.** The dashboard derives its OAuth-AS resource list at startup from the
manifests under `/opt/*/etc/manifest.env` (those with `MCP=true`), via
`DASHBOARD_MANIFEST_ROOT` (default `/opt`). `optctl install` regenerates the
service's stable `etc/manifest.env` from its own binary on every swap, so adding a
service is just deploying it (which lands its manifest) then **restarting the
dashboard** so it re-reads the manifests. There is **no hardcoded resource list**
to hand-edit — the old hardcoded-env-list-in-the-dashboard-build mechanism is
**gone**. (A dashboard restart briefly drops `/internal/authn` for the whole box —
seconds.) Until that restart, the new service's `/srv/<svc>/mcp` 401 omits
`resource_metadata`.

**Order to bring up a new consumer service (notify was the worked example):**
1. seed secrets → `bin/secrets` (after SSO login),
2. deploy any **producer it consumes first** (e.g. crm's `/feed`) so it's live,
3. `optctl setup <svc>` (provision) — `optctl init-box` first only on a brand-new
   box,
4. `bin/bump <svc> <major|minor|patch>` (advance the committed `<svc>/VERSION` on
   `main`), then `bin/deploy <svc>` (build current `main` off-box → `optctl
   install` → migrate → atomic swap → start),
5. restart the dashboard so it re-reads the manifests (picks up the new
   `MCP=true` service automatically),
6. verify (and on failure, `optctl rollback <svc>`).

**Verify on the box:** `systemctl is-active <svc>`; `journalctl -u <svc>` for the
boot/migration/consumer lines (note: best-effort paths like notify's push log
only at **Debug** on success — silence ≠ failure; look for `Warn`); loopback
`curl 127.0.0.1:<port>/...` (services trust injected `X-Owner-Email`/`X-Client-Id`
headers, so you can drive `/mcp` directly); PRM `…/.well-known/oauth-protected-
resource` → 200; the `/srv/<svc>/mcp` 401 challenge must carry `resource_metadata`;
then an end-to-end check (event plane: create a row → observe the downstream
effect; auth: connector OAuth round-trip + `<svc>_whoami`).

## MCP client delivery — forced outcomes (2026-06)

How the suite's MCP servers reach Claude clients, and the constraints that
forced the split. The vast majority of users are **Claude Chat / Cowork**;
**Claude Code** users are an extreme minority.

- **Chat / Cowork (the majority): one custom connector per service, added by
  hand.** OAuth-protected remote MCP **cannot** be delivered via a plugin /
  marketplace here — plugin OAuth uses a *localhost* callback, but the Cowork
  agent runs in Anthropic's cloud, so nothing on the user's machine catches the
  redirect. The working path is *Customize → Connectors → "+" → paste the
  service's `/srv/<svc>/mcp` URL → authorize*, once per service. Connector OAuth
  uses Anthropic's **hosted** callback `https://claude.ai/api/mcp/auth_callback`
  — the dashboard OAuth AS must allow-list it and expose discovery
  (`/.well-known/oauth-authorization-server` / `-protected-resource`) + PKCE/DCR.
  At **Team/Enterprise** an admin adds each connector once for the whole org.
  No bundling exists; a single aggregating endpoint was rejected (too many tools).
- **Claude Code (the minority): plugin marketplace and/or the install script.**
  The localhost callback works here, so the plugin marketplace
  (`mgreenly/marketplace` — public, a **separate** GitHub repo, plugin
  `metaspot-suite@metaspot`) and the `curl … | sh` script (which runs
  `claude mcp add --transport http` per service) both work.

The `marketplace` repo is **kept regardless** — beyond CLI MCP config it's the
general channel for delivering **skills and slash commands** to Claude clients,
which is useful independent of the connector story above.

**Support = user-facing instructions for both paths** (per-connector steps for
Chat/Cowork; script/marketplace for Claude Code) — not automation; the
per-service connector add is irreducible for Cowork.

## Gotchas from the 2026-06 moves

Two structural changes happened in 2026-06, and stale references to the old
shape survive in places:

1. **Projects moved under `ikigai/`.** They used to live directly under
   `~/projects/`; they were moved into `~/projects/ikigai/`. Sub-project docs still
   reference the **old** sibling-relative path `../metaspot` (e.g.
   `crm/CLAUDE.md`), which from `ikigai/<svc>/` now resolves to the non-existent
   `ikigai/metaspot` — the correct relative path is `../../metaspot` from a service,
   or `../metaspot` from this ikigai root. A few comments still name
   `~/projects/nginx` (now `~/projects/ikigai/nginx`).
2. **`ikigai/` became a single mono-repo.** What were separate per-project git
   repos are now plain subdirectories of one repo (one `.git` at the ikigai root).
   Sub-project docs that still describe themselves as standalone repos (their own
   `.git`, "deploy is `git push`", etc.) are describing the pre-mono-repo shape.

All of this is cosmetic/documentary — code (Go module names, the root `go.work`,
nginx's self-locating `run`, relative includes, the `replace eventplane =>
../eventplane` directives) is unaffected.
