# ADR — Deployment / Packaging / Versioning Redesign

> **Status: ACCEPTED (design converged, 2026-06-05).** This is the durable
> architecture decision record for the ikigai suite's deploy story. It captures
> the converged design (`PLAN.md` §1), the resolved open decisions (`PLAN.md`
> §4), and the parts the plan flagged as sketch-only: the per-verb internals of
> `opsctl`, the exact `appkit.Spec` field set, and the `init-box` vs
> `setup <app>` split. It is written in the house *context → decision →
> consequences* shape of `docs/event-plane-decisions.md`.
>
> **Scope.** The whole ikigai mono-repo's deploy/packaging/versioning model:
> seven services (`dashboard`, `crm`, `ledger`, `notify`, `ralph`, `dropbox`,
> `wiki`) and three never-tagged in-repo libraries (`eventplane`, `agentkit`,
> and the new `appkit`). It does **not** change the event-plane wire protocol
> (`docs/event-protocol.md`), the nginx auth contract, or any service's domain
> surface.
>
> **On conflict, `PLAN.md` §1 (canon) and §2 (invariants) win and this doc is
> corrected to match.**

---

## Context — what we are replacing

The pre-redesign deploy model is *rsync the built working tree into place,
overwrite the running binary, no version pinned, no rollback*. Concretely, per
service:

- `bin/build` emitted **two** artifacts: `build/<app>.bin` (the Go binary) and
  `build/<app>` (a generated shell wrapper that became `/opt/<app>/bin/run`,
  composed the public `RESOURCE_ID`/`AUTH_SERVER` from `METASPOT_DOMAIN`+`MOUNT`,
  and `exec`'d the binary on `127.0.0.1:$PORT`). It also bundled a copy of
  `registry` into the artifact set.
- `bin/deploy` was a **per-service clone** — one copy in each of the seven
  services — that did `build → ssh systemctl stop → rsync the artifacts over the
  live ones → chown → systemctl start → is-active`.
- There were **no versioned releases on the box**: the new binary overwrote the
  old one in place. A bad deploy had **no rollback** other than re-deploying a
  presumed-good tree, and "what is actually running" could only be guessed from
  the working tree the operator last shipped.
- Registering a new MCP service formerly required hand-editing a hardcoded
  resource list and redeploying the dashboard. **That mechanism is gone:** the
  dashboard now derives its OAuth-AS resource list from the manifests under
  `/opt/*/etc/manifest.env` at startup (`dashboard/cmd/dashboard/main.go`, via
  `DASHBOARD_MANIFEST_ROOT`, default `/opt`). Adding a service is now a dashboard
  restart, no env edit.

The pain this causes:

- **No rollback / no atomicity.** Overwrite-in-place means a failed start leaves
  the box with the broken binary and no prior version to fall back to. There is
  no atomic cutover between "old running" and "new running".
- **No durable answer to "what's deployed?"** Neither the box nor any artifact
  records the version; git tags don't exist per service.
- **Seven copies of deploy logic.** Each service carries a cloned `bin/build` +
  `bin/deploy`; the launcher-facing artifact shape (wrapper + bundled registry)
  is duplicated and drifts.
- **Latent build bugs travel with the clones.** The auto VCS stamp aborts at the
  bare mono-repo root; `go.work` leaks into prod builds non-deterministically;
  the version-stamp ldflags target was wrong. (See *Consequences → known build
  bugs to fix*.)

The constraints that bound any replacement (the §2 invariants, summarized): the
box never compiles (all Go building is off-box, artifacts are static binaries);
migrations are forward-only and downgrade-guarded; `/opt/<app>/etc/manifest.env`
and `/opt/<app>/bin/run` are load-bearing stable paths at runtime; the app's
SQLite DB is never touched on deploy; services stay loopback-only behind nginx;
and secrets never land on the box or in a log.

---

## Decision

Replace the model with a **uniform app contract** (`appkit`), an **on-box
platform CLI** (`opsctl`), **versioned release directories with atomic symlink
swap and rollback**, **per-service committed `<app>/VERSION` files** (the version
source of truth — superseding the originally-planned per-service `<app>/vX.Y.Z`
tags), and a **single thin shared `deploy` wrapper** (plus its `bin/bump`
companion). Five forks resolved in `PLAN.md` §4 are folded in (see *Resolved
decisions* at the end of this section).

### 1. The App Contract — "an ikigai app is exactly this"

- **One self-contained static `linux/amd64` Go binary, nothing else.** The
  generated `run` shell wrapper is **deleted**; the bundled `registry` copy is
  **dropped** from the artifact set. The build emits a single binary.
- **No-arg invocation = serve.** All runtime config is read from the
  **environment** (`PORT`, `METASPOT_DOMAIN`, secrets injected by the launcher).
  The public `RESOURCE_ID` / `AUTH_SERVER` are **composed inside the binary**
  from `METASPOT_DOMAIN` + `MOUNT` — the logic the deleted wrapper used to do now
  lives in Go.
- **Fixed subcommands, every app** (the contract *is* this verb set):
  - `<app>` (serve — the default, no-arg),
  - `version` — self-reports `<version> (<sha>[-dirty])`; the box can't lie,
  - `manifest` — emit *this app's* `manifest.env` to stdout; **the binary is the
    source of truth for its own identity**,
  - `migrate` — run forward-only migrations against the DB and exit,
  - `backup` — snapshot this app's SQLite state,
  - `restore` — restore this app's SQLite state from a snapshot.
- **`manifest` must emit every key the app declares**, not just the universal
  `APP`/`MOUNT`/`DEFAULT`/`PORT`/`MCP`. It must include the event-plane role keys
  (`FEED` for a producer like `dropbox`; `CONSUMES` for a consumer like
  `notify`/`wiki`) and any non-secret service config the old build wrapper used
  to export (e.g. wiki's `WIKI_INGEST_MODEL` / `WIKI_INGEST_MAX_TOKENS`). This is
  load-bearing: **both the dashboard's resource derivation and `bin/registry`
  parse `/opt/*/etc/manifest.env`**, so a regenerated manifest that drops a key
  silently breaks producer/consumer wiring or MCP-inventory inclusion.
- **State** is one SQLite DB at `/opt/<app>/data/<app>.db`; migrations run on
  start; the server binds loopback-only.

### 2. `appkit` — the shared chassis library

`appkit` is a sibling module to `eventplane`, consumed via a committed
`replace appkit => ../appkit` and `require appkit v0.0.0` — the same
never-tagged pattern. It owns the **uniform half** of every service so each
service's `main.go` collapses to a single `appkit.Main(appkit.Spec{…})` call.

`appkit` owns: the subcommand dispatcher (the fixed verbs), config-from-env at
the composition root, the migration runner + downgrade guard, the loopback HTTP
server with the PRM (`/.well-known/oauth-protected-resource`) route, the
`requireIdentityHeaders` gate, the `/feed` plumbing, and `manifest.env`
emit+parse.

**`appkit` is the deploy/serve chassis — it is NOT the agentic engine.** A
second shared library already exists — **`agentkit`**
(`agent/job/model/provider/schema/tools/trace/wire`: the LLM provider + async
job-runner extracted by the wiki work). The two are complementary and stay
**strictly separate**: `appkit` owns the contract/serve/migrate/manifest
plumbing; `agentkit` owns LLM/job machinery; neither absorbs the other. A
service may use both — `wiki = appkit chassis + agentkit ingest + eventplane
consumer` — so wiki's conversion is the proof `appkit` composes cleanly with
`agentkit`. Both libraries use the committed-`replace`, never-tagged pattern.

**The exact `appkit.Spec` field set.** The contract is literally the type
signature of `appkit.Spec` / `appkit.Main`. The fields:

| Field | Type (intent) | Purpose |
|---|---|---|
| `App` | `string` | Service name (`"ledger"`). Drives the manifest `APP` key, the DB filename `/opt/<app>/data/<app>.db`, the install root, and log identity. |
| `Mount` | `string` | The `/srv/<app>/` path prefix (manifest `MOUNT`). Used **in-binary** to compose `RESOURCE_ID`/`AUTH_SERVER` from `METASPOT_DOMAIN`, and to build the PRM route. For the dashboard this is the apex/`DEFAULT` case. |
| `Port` | `int` | Loopback port (manifest `PORT`). The server binds `127.0.0.1:$PORT`. Read from env at serve time, defaulted from the Spec. |
| `MCP` | `bool` | Whether this service exposes an MCP surface (manifest `MCP=true`) — what makes the dashboard include it in the AS resource list / service inventory. |
| `Feed` | `string` (empty = not a producer) | Producer role. Non-empty (e.g. `"/feed"`) emits `FEED=` in the manifest and mounts the `/feed` SSE outbox handler. Empty ⇒ no producer plumbing, no `FEED` key. |
| `Consumes` | `[]string` (nil = not a consumer) | Consumer role: the upstream producer names whose feeds this service reads (e.g. `["dropbox"]`). Emits `CONSUMES=` (comma-joined) in the manifest; the consumer loop resolves each feed URL via `bin/registry`. |
| `ManifestExtras` | `[]ManifestKV` (ordered) | Non-secret service config the manifest must round-trip beyond the universal/role keys (e.g. `WIKI_INGEST_MODEL`, `WIKI_INGEST_MAX_TOKENS`). Ordered so `manifest` output is byte-stable and the committed `etc/manifest.env` can be byte-compared in tests. **Secrets never appear here** — they flow via SSM/env only. |
| `Migrations` | `embed.FS` (or `[]Migration`) | The app's embedded, numbered, forward-only migration set. `appkit`'s runner applies the unapplied higher versions, records them in `schema_migrations`, and **refuses to start on a DB carrying a version the binary no longer embeds** (the downgrade guard). |
| `Handlers` | `func(*appkit.Router)` (registration hook) | Where the service registers its own domain routes (`/contacts`, `/journal`, `/mcp`, …) on the server `appkit` stands up behind the identity gate. The app's real domain surface lives here, untouched by the chassis. |
| `Backup` | `func(ctx, BackupReq) error` (optional; nil ⇒ default) | Service hook for the `backup` verb. Nil uses `appkit`'s default SQLite snapshot of `/opt/<app>/data/<app>.db`; non-nil lets a service (e.g. the dashboard) customize. |
| `Restore` | `func(ctx, RestoreReq) error` (optional; nil ⇒ default) | Symmetric hook for `restore`. Nil uses the default SQLite restore; non-nil lets a service handle the event-plane generation re-mint or extra state on restore. |

`appkit.Main` is the single entrypoint each service's `main.go` calls; its
signature is:

```go
func Main(spec Spec) // parses os.Args, dispatches the fixed verb (default = serve),
                     // exits with the verb's status. Never returns to the caller.
```

Each app's `main.go` reduces to roughly:

```go
func main() {
    appkit.Main(appkit.Spec{
        App: "ledger", Mount: "/srv/ledger/", Port: 3002, MCP: true,
        Migrations: migrationsFS,
        Handlers:   registerRoutes,
        // Feed / Consumes / ManifestExtras / Backup / Restore as the service needs
    })
}
```

Per-service divergence that **stays app-side** (not in `appkit`): the
dashboard's manifest-derived resource list and its extra lifecycle verbs
(`secrets`/`teardown`); wiki's `agentkit` ingest core. `appkit` provides the
chassis; these remain the service's own code.

### 3. `opsctl` — the on-box platform CLI

A single static Go binary at `/usr/local/bin/opsctl`. **On-box only** — external
callers SSH in and run it; there is *no* dual local/remote runner abstraction
(rejected as YAGNI). It runs privileged via `sudo opsctl …`. It is the substrate
the dashboard's future operations-MCP (deploy/backup/restore/health as MCP
tools) will call.

All of `opsctl`'s filesystem operations are rooted at a **configurable base**
(`OPSCTL_ROOT`, default `/opt`) so the whole CLI is testable against a temp dir
with no real box. Systemd / `sudo` / nginx invocations sit behind a seam tests
stub.

Verbs: `install · rollback · backup · restore · setup · init-box · prune`
(`launch` deferred to a later phase — see *Resolved decisions* #3).

### 4. Versioned release dirs + atomic swap + rollback (on the box)

```
/opt/<app>/
  releases/<version>/<app>        # the static binary for that version
  current -> releases/<version>   # ln -sfn = ATOMIC swap
  bin/run -> ../current/<app>     # STABLE path metaspot-launch execs (unchanged)
  etc/manifest.env                # regenerated by `<app> manifest` on swap (STABLE path)
  data/<app>.db                   # state — NEVER touched by deploy
```

The release dir on the box is named `v<version>` → `ledger/VERSION` of `1.4.0`
becomes `releases/v1.4.0/` (`bin/ship` prepends the `v`). `/opt/<app>/bin/run`
and `/opt/<app>/etc/manifest.env` stay as stable paths at all times (the symlink
and
the regenerated file), so the baked `metaspot-launch`, `bin/registry`, and the
dashboard's manifest-derived resource list are all untouched and remain valid
mid-swap.

### 5. `ship <app>` — the thin local wrapper (+ `bump`)

**One shared script** (repo-root `bin/ship`) replaces the seven cloned
`*/bin/deploy`. It takes **no version argument**: the version is the committed
`<app>/VERSION` file. Its body: source the app's `deploy.env` → build **current
`main` (HEAD)** in a throwaway detached `git worktree` (reproducible, doesn't
disturb the operator's working tree) → off-box `go build` (`CGO_ENABLED=0
GOOS=linux GOARCH=amd64 -trimpath -buildvcs=false GOWORK=off`, ldflags version
stamp) → read the version from **that worktree's** `<app>/VERSION` and prepend
`v` → `scp` the single artifact to the box `/tmp` → **print the two box commands**
(`sudo opsctl stage <app> v<version> --artifact /tmp/…` then `sudo opsctl deploy
<app> v<version>`) and **stop**. `bin/ship`'s only box write is the `/tmp` upload;
it runs **no** stage/deploy over ssh — the box owns every state change, split
across the `stage` (place) and `deploy` (activate) verbs.

The companion **`bin/bump <app> <major|minor|patch>`** is how the version is
advanced: it reads `<app>/VERSION`, increments the requested SemVer field, writes
the new bare number back, makes a **path-limited** commit of only that file
directly to `main` (`git commit … -- "<app>/VERSION"`), and pushes. `bump`
decides the version; `ship` builds + uploads current `main`. The operator-facing
how-to is [`versioning.md`](./versioning.md).

### 6. Versioning

> **Superseded — file, not tags.** This section originally specified per-service
> `<app>/vX.Y.Z` git tags resolved via `git describe`. The implemented model
> instead keeps the version in a **committed `<app>/VERSION` file** on `main`; git
> tags drive nothing (any that survive are vestigial). The section below is the
> current model.

> **Operator how-to:** the concrete bump→deploy procedure (`bin/bump`, the
> `<app>/VERSION` source of truth, the SHA+dirty co-stamp, the box release-dir
> naming) is written up step-by-step in [`versioning.md`](./versioning.md).

- **Independent per-service.** The suite has **no global version**.
- The version source of truth is the committed file **`<app>/VERSION`** — a bare
  SemVer number (no leading `v`), one per deployable service, on the **one**
  mono-repo's `main`. The **commit** that lands a `VERSION` change pins the app's
  code **and** its library source (via the committed `replace` directives)
  atomically — the job a tag used to do. `bin/bump <app> <field>` advances it
  (path-limited commit to `main` + push); `bin/ship <app>` builds current `main`
  and ships that committed version (the box `stage`/`deploy` verbs activate it).
  **Git tags are NOT the version mechanism**
  (no `git tag <app>/vX.Y.Z`, no `git describe`); the `v` is a deploy-time
  display prefix only.
- **Libraries (`eventplane`, `agentkit`, `appkit`) and `opsctl` are NOT
  versioned** — the libs are consumed at HEAD via `replace` + `require … v0.0.0`,
  carry no `VERSION` file. **HARD RULE: never convert an internal `replace` into a
  versioned `require`** (it drags in the proxy + subdir machinery this design
  routes around).
- Always **co-stamp git SHA + dirty** alongside the version (because
  `-buildvcs=false` drops the auto VCS stamp, the SHA is re-added via ldflags).
  The stamped commit SHA (`appkit.commit`) is what answers "what built this".
- **"What's deployed" is answered two ways:** the binary self-reports
  (`<app> version` → `v<x.y.z> (<sha>)`) — the box can't lie; `releases/` +
  `current` = the on-box ledger. The version history itself lives in `main`'s
  commits (the `<app>/VERSION` ledger), made immutable by branch protection.
- **No release tooling now** — `bin/bump` (file edit + commit) + `bin/ship`
  (build current `main`, upload) + the box `opsctl stage`/`deploy` verbs +
  ldflags. GoReleaser / release-please only if/when
  GitHub Actions arrives.

### `opsctl` — per-verb internals

All verbs operate over the §4 release-dir + atomic-symlink layout, rooted at
`OPSCTL_ROOT` (default `/opt`). "Atomic swap" everywhere means `ln -sfn` of
`current` (a symlink rename, atomic on the same filesystem).

Shipping a new version live is split across **two** verbs: `stage` (place a
release, not live) then `deploy` (activate it). `bin/ship` prints both. The split
keeps the stable `manifest.env`, the DB migrate, and the running unit untouched
until the operator explicitly runs `deploy`.

**`stage <app> <version> --artifact <path> [--force]`** — place a release; NOT
live.
1. **Preflight (refuse early, before placing anything):**
   - the artifact is a **static** binary (no dynamic linkage);
   - its arch is `amd64` (`linux/amd64`);
   - `<artifact> version` self-reports a version that **matches the `<version>`
     arg** (the binary can't lie about which version it is);
   - `<artifact> manifest` **parses** and emits a well-formed `manifest.env`.
   Any failure aborts.
2. **SHA collision guard.** If `releases/<version>/<app>` already exists, run its
   `version` verb and the incoming artifact's, and compare the **commit SHA**: same
   SHA → idempotent no-op (skip the copy); different SHA, or the existing binary
   won't exec → **refuse unless `--force`**.
3. **Place** the artifact into `releases/<version>/<app>` (creating the release
   dir).
4. **Delete the `/tmp` artifact on success** (freshly placed or idempotent no-op).
   On refusal/error the `/tmp` file is **kept** so the operator can retry (e.g.
   with `--force`) without a re-`scp`. `stage` never touches the manifest, the DB,
   or the running unit.

**`deploy <app> <version>`** — activate a staged release. `<version>` is
**required** (no "newest staged" default).
1. **Guard: the release was staged** — refuse early if `releases/<version>/<app>`
   is absent ("stage it first").
2. **Regenerate `etc/manifest.env`** by running `<new binary> manifest` and
   writing its output to the stable `/opt/<app>/etc/manifest.env` path. This is
   where `FEED`/`CONSUMES`/service-config round-trip back onto the box for the
   dashboard + `bin/registry` to read.
3. **Backup the DB if the schema advances.** Compare the migration version the
   new binary embeds against what the live DB carries; if the new binary
   advances the schema, snapshot `data/<app>.db` first (named/retained so the
   matching `rollback` can restore it). If the schema does not advance, no backup
   is needed.
4. **Migrate** — run `<new binary> migrate` against `data/<app>.db` (forward-only;
   applies only the unapplied higher versions).
5. **Atomic swap** — `ln -sfn releases/<version> current`. Because `bin/run`
   points at `../current/<app>`, the launcher now execs the new binary on next
   start without any path edit.
6. **Restart** the systemd unit (behind the stubbable seam) and assert
   `is-active`. On failure, the operator's recovery is `rollback` (the prior
   release dir and pre-migration backup are intact).
7. **Prune** old releases (see `prune`).

   The DB at `data/<app>.db` is never overwritten by the artifact placement —
   only ever read/migrated/snapshotted (honoring §2.7).

**`rollback <app> [version]`** — repoint to a prior release.
1. Resolve the rollback target — the immediately-prior release by default, or an
   explicit older `<version>` that still exists under `releases/`.
2. **Restore the DB first *iff* the release being rolled back *from* had advanced
   the schema.** The forward-only migration runner's downgrade guard refuses to
   boot a DB whose recorded version exceeds what the (older) binary embeds, so
   without restoring the matching pre-migration snapshot the rolled-back binary
   would crash on start. If no schema advance occurred, no DB restore is needed.
3. **Atomic swap** `current` → the target release.
4. **Restart** the unit and assert `is-active`.

**`backup <app>`** — snapshot the app's SQLite state to a recoverable location
(per-account bucket / box-local, per the metaspot backup convention). Invokes
the app's `backup` verb (which honors the `Spec.Backup` hook, default =
consistent SQLite snapshot of `data/<app>.db`). Used both standalone (operator
backup) and internally by `deploy` on schema advance.

**`restore <app> [snapshot]`** — restore the app's SQLite state from a snapshot.
Invokes the app's `restore` verb (honoring `Spec.Restore`). Used both standalone
and internally by `rollback` after a schema-advancing release. **Restore must
re-mint the event-plane generation token** for a producer (the event protocol
requires a fresh generation on every restore/rebuild so stale consumer cursors
are rejected with `resync` reason `stale-epoch`); the app's `Restore` hook owns
this.

**`setup <app>`** — first-time per-app provisioning (see *the split* below).

**`init-box`** — one-time box-global substrate (see *the split* below).

**`prune <app>`** — bound the on-box release history. Keep the **N most recent**
releases plus `current` (and the prior release `rollback` would target); delete
older `releases/<version>/` dirs. Never deletes `current` or its immediate
predecessor. Runs at the tail of `install`; also invocable standalone.

### `init-box` vs `setup <app>` — the box-global / per-app split

The pre-redesign `bin/setup` (and the dashboard's overloaded variant) mixed two
concerns: bringing up the box's shared substrate, and provisioning one app.
`opsctl` splits them cleanly so app provisioning never reaches for box-global
state and vice versa.

**`opsctl init-box` — box-global substrate, run once per box, owns global
state.** Responsibilities:
- nginx itself + certbot, obtaining the **one** TLS cert for the apex;
- the apex `server{}` block, the `/_authn` introspection hook, and the
  `conf.d/locations/` include directory the per-app fragments drop into;
- the cert-renewal timer;
- any box-wide directories/permissions the platform needs.

This is the substrate the dashboard's apex setup used to bootstrap. It is
**idempotent** and owns the global pieces no single app should.

**`opsctl setup <app>` — per-app provisioning, run once per service, owns
nothing global.** Responsibilities:
- create the dedicated `--system` app user and the `/opt/<app>/` tree
  (`releases/`, `bin/`, `etc/`, `data/`);
- write and **enable (not start)** the systemd unit with
  `ExecStart=/usr/local/bin/metaspot-launch <app>` (the stable launcher
  contract);
- drop the app's nginx location fragment into
  `/etc/nginx/conf.d/locations/<app>.conf`, then `nginx -t` + reload.

`setup <app>` is required once before a brand-new service's first `stage`/`deploy`. It
assumes `init-box` already ran (the `conf.d/locations/` dir, apex block, and
cert exist). The diff from today's behavior is **only the split** — the unit
file and nginx fragment it emits match what the old `bin/setup` produced.

### Resolved decisions (PLAN.md §4 — ratified leans, subject to user sign-off)

1. **Version scheme: loose SemVer, per service, no global suite version.** As
   ratified this was loose SemVer *tags* `<app>/vX.Y.Z` (resolved via
   `git describe`); **superseded in implementation** by a committed bare-SemVer
   `<app>/VERSION` file on `main` (advanced by `bin/bump`, stamped into the binary
   via ldflags). The commit on `main` — not a tag — now pins the build; tags drive
   nothing.
2. **`backup`/`restore`: folded into the binary** as fixed subcommands — the
   one-static-binary contract depends on it.
3. **`metaspot-launch`: additive first** — leave the baked launcher untouched
   (it is a load-bearing stable-path contract); add `opsctl launch` later.
4. **`ship` build source: throwaway `git worktree`** — reproducible build
   without disturbing the working tree. (Originally a checkout of the *tagged*
   commit; in the implemented file model it is a detached checkout of **current
   `main` (HEAD)**, whose committed `<app>/VERSION` supplies the version.)
5. **On-box grouping: keep `/opt/<app>`** — a move to `/opt/srv/<app>` would
   ripple through the launcher, `bin/registry`, and the dashboard's
   manifest-derived resource list for no P1 benefit.

### Phasing of capability

- **P1 (this design):** operator ships builds of committed `main` into versioned
  release dirs — full versioning + rollback immediately, no new infra.
- **P2 (later):** S3 artifacts bucket + box-pull (`opsctl stage --artifact
  s3://…`, new IAM) — enables CI, multi-box, dashboard-initiated deploy.
- **P3 (optional):** GitHub Actions builds+publishes on tag, box pulls. CI must
  be build-and-publish (box pulls), **never** inbound SSH (SSH is pinned to one
  admin IP; the box has no SSM Session Manager).

**Mono-repo stays.** Rejected alternatives: containers (no runtime; against the
appliance ethos), git-pull-build on the box (the box never compiles), Nix
(overkill).

---

## Consequences

**What gets better.**
- **Rollback is real and atomic.** Two known-good release dirs side by side and
  an `ln -sfn` cutover mean a failed start is one `opsctl rollback` from the
  prior version, schema-aware (DB restored when the rolled-back-from release had
  advanced the schema).
- **"What's deployed" is answerable two ways** — the self-reporting binary (the
  box can't lie) and the on-box `releases/`+`current` ledger; the version *history*
  lives in `main`'s committed `<app>/VERSION` ledger (immutable under branch
  protection).
- **One deploy path, not seven.** A single shared `bin/ship` wrapper + one
  `opsctl` (its `stage`/`deploy` verbs) replace the seven cloned
  `bin/build`+`bin/deploy` stacks; the
  artifact shrinks to one static binary (no wrapper, no bundled registry).
- **The app's identity is owned by the app.** `<app> manifest` is the single
  source of truth; `deploy` regenerates `/opt/<app>/etc/manifest.env` from it on
  every swap, so the dashboard's resource derivation and `bin/registry` always
  read a manifest the running binary itself produced.

**What this costs / what to watch.**
- **`appkit` extraction is unproven until done.** It is asserted, not yet
  demonstrated, that the chassis extracts cleanly from the existing services; the
  extraction-map step (B1) is the validation, and the richest source may be
  `dashboard`/`crm`, not `ledger`. `ledger` is the prototype precisely because it
  is the simplest.
- **The manifest round-trip is load-bearing.** If `<app> manifest` ever drops a
  declared key (a `FEED`/`CONSUMES`/service-config extra), it silently breaks
  producer/consumer wiring or MCP-inventory inclusion on the box. The
  byte-compare of `<app> manifest` against the committed `etc/manifest.env` is
  the guard, and `ManifestExtras` exists so every declared key round-trips.
- **Rollback across a schema advance depends on the pre-migration backup
  existing.** `deploy` must snapshot the DB *before* migrating whenever the
  schema advances, and `rollback` must restore it — otherwise the downgrade guard
  bricks the rolled-back binary on boot. This is a forced consequence of
  forward-only, downgrade-guarded migrations (§2.5), not an optional nicety.
- **Stable paths must never break, including mid-swap.** `/opt/<app>/bin/run`
  (symlink → `../current/<app>`) and `/opt/<app>/etc/manifest.env` (regenerated
  file) are sourced/exec'd by `metaspot-launch` and read by `bin/registry` + the
  dashboard; the install sequence keeps both valid throughout. The DB is never
  touched on deploy.

**Known build bugs to fix as the wrapper is built (do not propagate the clones'
bugs):**
- `-buildvcs=false` is **mandatory** — the module is a subdir of a *bare*
  mono-repo `.git`; Go's auto VCS stamp runs git at the bare root and aborts
  (exit 128). Version comes from ldflags, not the VCS stamp — which means the
  commit SHA + dirty flag must be **re-added via ldflags** (they're otherwise
  lost with the dropped `vcs.revision`/`vcs.modified`).
- `GOWORK=off` in production builds — `go.work` is local-dev only; prod builds
  must be deterministic via each consumer's committed `replace` directives
  (`eventplane`, `agentkit` where present, and `appkit`), with no network fetch
  of the in-repo library trees.
- ~~`git describe --match '<app>/*'` for the version — without the per-app match,
  the single shared tag namespace cross-contaminates services.~~ **Superseded:**
  there is no `git describe` and no tag namespace. The version is read from the
  committed `<app>/VERSION` file (per service by construction, so no
  cross-contamination is possible); `bin/ship` reads it from the build
  worktree's copy and stamps it via ldflags.
- the ldflags `-X` target must be a **`var`**, not a `const` — `-X` against a
  `const` is silently ignored, leaving the `dev` default.

**Out of scope (unchanged by this ADR).** The event-plane wire protocol
(`docs/event-protocol.md`), the nginx `auth_request` introspection contract,
loopback-only binding and the trust-the-injected-headers model, and every
service's domain surface. Secrets remain operator-side (`~/.secrets` → SSM →
launcher → process env only) — never read/printed, never on the box, never in a
log.
