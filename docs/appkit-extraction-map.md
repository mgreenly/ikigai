# appkit Extraction Map (PLAN step B1)

> **Status: design map, no code.** This is the B1 deliverable: it maps every
> piece of duplicated *chassis* across the seven services to its target `appkit`
> package, pins the concrete `appkit.Spec` field list and `appkit.Main`
> signature (reconciled against `docs/adr-deployment-redesign.md`), bounds
> `appkit` against the already-extracted `agentkit`, and records what must stay
> app-side. It feeds B2 (create the module) directly.
>
> On conflict, `PLAN.md` §1/§2 (canon/invariants) win and the ADR's `Spec`
> sketch is corrected to match — divergences from the ADR draft are flagged
> inline below.

---

## 0. What "duplicated chassis" means here

Five services (`crm`, `ledger`, `notify`, `dropbox`, `wiki`, `ralph`) carry a
**byte-for-byte (modulo rename) copy** of the same `internal/{server,db,mcp,
logging,ids}` chassis and the same `cmd/<svc>/main.go` env-plumbing boilerplate.
The dashboard carries a *richer, divergent* copy of the same shape (its own
`server`, `db`, `logging`, plus an `inventory` manifest reader). The deploy-time
identity (`manifest.env`) and the `bin/build` wrapper that composes
`RESOURCE_ID`/`AUTH_SERVER` are likewise cloned.

`appkit` absorbs the **uniform** half: config-from-env, the loopback server +
PRM + identity gate, the migration runner + downgrade guard, the `/feed` mount,
the manifest emit/parse, and the fixed-verb subcommand dispatcher. Each service's
*domain* (`internal/<domain>`, `internal/mcp` tool descriptors, its migrations,
its consumer/producer wiring intent) stays app-side and plugs in through `Spec`.

---

## 1. The extraction table — chassis piece → appkit package

| Chassis piece (today, per service) | Source of truth read | New `appkit` package | What it owns | What stays app-side |
|---|---|---|---|---|
| **config-from-env** (`run()` in `cmd/<svc>/main.go`: `envOr`/`envOrInt`/`envOrDuration`, `LEDGER_PORT`/`*_IP`/`*_LOG_LEVEL`/`*_RESOURCE_ID`/`*_AUTH_SERVER`/`*_DB_PATH`/`*_GENERATION_PATH`/retention; **`RESOURCE_ID`/`AUTH_SERVER` composed from `METASPOT_DOMAIN`+`MOUNT`** — moves out of the deleted `bin/build` wrapper into Go per §1.1) | `ledger/cmd/ledger/main.go:44-96`, `crm` ditto, `wiki/cmd/wiki/main.go:63-198`, `dropbox` main, `bin/build` wrappers | `appkit/config` | The universal env contract: parse `PORT`/`IP`/`LOG_LEVEL`/`DB_PATH`/`GENERATION_PATH`/retention; compose `RESOURCE_ID = https://${METASPOT_DOMAIN}${MOUNT}mcp` and `AUTH_SERVER = https://${METASPOT_DOMAIN}` in-binary; the `envOr*` helpers; dev-localhost defaults. | Service-specific **non-secret** knobs are declared via `Spec.ManifestExtras` + read by the service's own `Config`-hook (wiki's `WIKI_INGEST_*`, dropbox's `DROPBOX_LONGPOLL_TIMEOUT` etc.). **Secrets** (`ANTHROPIC_API_KEY`, `DROPBOX_*`, `NTFY_*`) are read by the service at its own composition root, never by appkit. |
| **server + PRM + identity gate** (`internal/server/{server,handlers,middleware}.go`: `Options`, `New`, `routes()`, `Run`+graceful shutdown, `handlePRMetadata` at `/.well-known/oauth-protected-resource`, `requireIdentityHeaders`, `securityHeaders`/`isHTTPS`, `handleWhoami`, `writeJSON`) | `ledger/internal/server/*` (identical in crm/notify/dropbox/wiki modulo rename) | `appkit/server` | The full HTTP layer: route table, the one unauthenticated PRM route, the identity-header gate (the `401`+`WWW-Authenticate resource_metadata` challenge), security headers, `<app>_whoami`, graceful shutdown with pinned timeouts. Mounts `Spec.MCP` handler behind the gate, mounts `Spec.Feed` (when producer) unauthenticated, exposes a `Router`/registration hook for extra routes. | The MCP **handler** itself (`internal/mcp` JSON-RPC transport + tool descriptors) is the service's; it is passed in via `Spec.Handlers`. Dropbox's extra unauthenticated `GET /content` route is registered through the same `Handlers` hook. The **dashboard's** server is NOT this server (apex/OAuth-AS) — see §4. |
| **migration runner + downgrade guard** (`internal/db/db.go`: `Open` with WAL/FK/busy-timeout/`SetMaxOpenConns(1)`, `loadMigrations`/`loadMigrationsFS`, `Migrate`, `runMigrations` with the embedded-vs-applied **downgrade refusal**, `applyOne`, `tableExists`) | `ledger/internal/db/db.go` (identical in crm/notify/dropbox/wiki) | `appkit/db` (+ `appkit/migrate`) | SQLite `Open` (single-writer pragmas); the forward-only numbered migration runner; the **downgrade guard** (refuse to start on a DB version the binary no longer embeds, §2.5); `schema_migrations` bookkeeping. Drives both the `migrate` verb and serve-time migrate-on-start. Exposes the *current applied version* + *max embedded version* so `optctl install` can decide "schema advances → backup". | The actual migration `*.sql` files (`002_<svc>.sql`, `003_outbox.sql`, `002_feed_offset.sql`) — passed in via `Spec.Migrations embed.FS`. The byte-equality tests asserting `003_outbox.sql == outbox.SchemaSQL` / `002_feed_offset.sql == consumer.SchemaSQL` stay app-side. |
| **`/feed` mount** (producer outbox wiring: `outbox.New(...)`, `go ob.StartRetention(ctx)`, `server.Options.Feed = ob.FeedHandler()`, the generation sidecar path) | `ledger/cmd/ledger/main.go:118-149`, `crm` ditto, `dropbox` main | `appkit/feed` (thin) — orchestration only; **`eventplane/outbox` is NOT re-implemented** | When `Spec.Feed != ""`: construct the `eventplane/outbox` producer over appkit's DB handle, start retention, mount `FeedHandler()` unauthenticated at the `Feed` path, thread the generation sidecar path, emit `FEED=` in the manifest. `appkit` *calls* `eventplane`; it does not wrap or own the event protocol. | The producer's **event payloads/builders** (`internal/<domain>/events.go`) and the `Service.Outbox` injection stay app-side — appkit hands the constructed `*outbox.Outbox` (or an `Append`/`Ring` seam) back to the service's `Handlers` hook. The **consumer** side (`eventplane/consumer.Run`, the `Handler`, the structural-vs-transport crash semantics) is wired by the *service* (notify/wiki) — appkit only emits `CONSUMES=` and exposes the DB handle; see §3. |
| **manifest emit/parse (incl. role/config extras)** (the committed `etc/manifest.env`; parsed today by **two** readers — `dashboard/internal/inventory.parseManifest` and root `bin/registry`'s `manifest_get`) | `*/etc/manifest.env`; `dashboard/internal/inventory/inventory.go`; `bin/registry:51-84` | `appkit/manifest` | **Emit**: the `manifest` verb writes this app's full `manifest.env` to stdout — `APP`/`MOUNT`/`DEFAULT`/`PORT`/`MCP` (universal) **+** `FEED=` (iff producer) **+** `CONSUMES=` (iff consumer, comma-joined) **+** every `Spec.ManifestExtras` KV, in a **fixed deterministic order** so it byte-compares to the committed file. **Parse**: a Go KEY=value reader matching the two existing parsers (skip blank/`#`, split first `=`, trim, strip one quote pair) — so `optctl` preflight `manifest` parse + any in-binary self-read share one implementation. | The *set of keys* a given service declares is the service's `Spec` (App/Mount/Port/MCP/Feed/Consumes/ManifestExtras). appkit emits exactly what `Spec` declares; the committed `etc/manifest.env` is the byte-compare oracle (see §5 comment-policy). |
| **subcommand dispatcher (six fixed verbs)** (today: ad-hoc `--version` flag in services; `serve`/`reset` switch in dashboard `main.go:71-82`) | `dashboard/cmd/dashboard/main.go:65-82` (the only real dispatcher today); `ledger/crm/wiki` only have a `--version` bool | `appkit` (root pkg, `Main`) | Parse `os.Args`, dispatch the **six fixed verbs** — `serve` (default/no-arg), `version` (self-reports `<version> (<sha>[-dirty])`), `manifest`, `migrate`, `backup`, `restore` — wire the verb to the right `appkit/*` package, exit with the verb's status, never return. | Extra app-only verbs (dashboard's `reset`; the operator-side `secrets`/`teardown` which are **bash scripts, not binary subcommands**) stay outside `appkit.Main`. `backup`/`restore` defaults live in appkit but are overridable via `Spec.Backup`/`Spec.Restore` (dashboard overrides to also snapshot the apex cert; producers override `restore` to re-mint the generation epoch). |
| **logging + request-id + ids** (`internal/logging/logging.go`: `ParseLevel`, JSON `New`, `RequestIDMiddleware`, `statusRecorder`; `internal/ids/ids.go` ULID) | `ledger/internal/logging/logging.go` (identical across services); `*/internal/ids/ids.go` | `appkit/logging` (`appkit/ids` folded in or kept beside) | Structured slog JSON logger, level parsing, the per-request request-id middleware (wired by `appkit/server`), ULID generation. | Nothing — this is purely uniform. (The dashboard has its own `logging`; converting it last reconciles the duplicate.) |

**Coverage check (acceptance):** every chassis piece the briefing names is
mapped above — config-from-env → `appkit/config`; server+PRM+identity gate →
`appkit/server`; migration runner+downgrade guard → `appkit/db`(+`appkit/migrate`);
`/feed` mount → `appkit/feed`; manifest emit/parse (incl. role/config extras) →
`appkit/manifest`; the six-verb dispatcher → `appkit` root (`Main`). Plus
logging/ids → `appkit/logging`. No listed piece is left unmapped.

### Proposed `appkit` package tree (for B2)

```
appkit/
  appkit.go        // Spec, ManifestKV, Main (the six-verb dispatcher), version vars
  config/          // env contract: Resolve(spec) -> Config; envOr/envOrInt/envOrDuration; RESOURCE_ID/AUTH_SERVER compose
  server/          // Options/New/Run, routes, PRM, requireIdentityHeaders, securityHeaders, whoami, Router hook
  db/              // Open (WAL/FK/single-writer)
  migrate/         // loadMigrationsFS, runMigrations + downgrade guard, AppliedVersion/MaxEmbedded  (may live under db/)
  feed/            // producer orchestration over eventplane/outbox (only compiled-in when Spec.Feed != "")
  manifest/        // Emit(spec) -> string (ordered), Parse(r) -> map[string]string
  logging/         // ParseLevel, New, RequestIDMiddleware, statusRecorder, ULID ids
```

`appkit` `require`s `eventplane v0.0.0` (committed `replace`) for the `feed`
package. It does **not** require `agentkit` (see §2).

---

## 2. appkit (chassis) vs agentkit (LLM/job engine) — the boundary

**Explicit boundary statement.** `appkit` and `agentkit` are complementary,
strictly non-overlapping libraries. Neither imports the other.

- **`appkit` = the deploy/serve chassis.** Everything that makes a process "an
  ikigai app": the six fixed verbs, config-from-env, the loopback HTTP server +
  PRM + identity-header gate, the migration runner + downgrade guard, the `/feed`
  producer mount, and `manifest.env` emit/parse. It is HTTP/SQLite/deploy
  plumbing. It knows nothing about LLMs, prompts, jobs, or tools. Used by **all
  seven** services.
- **`agentkit` = the LLM/job engine.** `agent` (one-iteration agent loop), `job`
  (generic async agent-job lifecycle), `model` (model-id resolution), `provider`
  (+`provider/anthropic` — the provider-neutral client), `schema` (JSON-schema
  subset), `tools` (the agent's tool surface/confinement), `trace`, `wire`. It is
  prompt/tool/provider machinery. It knows nothing about ports, nginx, PRM,
  manifests, or deploy. Used **only** by LLM services (`wiki`; future `ralph`).

**The proof of separation is `wiki`:** `wiki = appkit chassis + agentkit ingest
core + eventplane consumer`. In wiki's `main.go` today, the chassis lines
(`db.Open`/`db.Migrate`, `server.New`/`server.Run`, env plumbing) move into
`appkit`; the agentic lines (`anthropic.New`, `ingest.New`/`lint.New`/`ask.New`,
`core.Recover`) stay in wiki's own `serve`/`Handlers` and continue to import
`agentkit`. The two never cross: `appkit.Spec.Handlers` is the seam where wiki's
agentkit-backed MCP handler is handed to appkit's server. **Hard rule: appkit
must not grow an `agent`/`provider`/`job` dependency, and agentkit must not grow
a server/manifest/migrate dependency.**

---

## 3. The richest extraction source — and what does NOT extract cleanly

**Richest source = `dashboard` for the *dispatcher*, but the cleanest full-chassis
template is the `ledger`/`crm` producer family.** (§5 risk materialized — split
verdict.)

- The **six-verb subcommand dispatcher** does not exist in clean form in any
  service today: `ledger`/`crm`/`wiki`/`dropbox` only carry an ad-hoc `--version`
  flag inside a single `flag.FlagSet`; the **only real multi-verb dispatcher is
  the dashboard's** `serve`/`reset` global-flagset-then-subcommand pattern
  (`dashboard/cmd/dashboard/main.go:42-83`). So the dispatcher is *designed new*
  in `appkit`, taking the dashboard's global/sub flagset split as its template,
  not copied from ledger.
- The **server + PRM + identity gate + db migration runner + logging** extract
  **most cleanly from `ledger`/`crm`** — they are byte-identical across the five
  non-dashboard services (modulo the `LEDGER_`/`CRM_` rename), so ledger is the
  right C-phase prototype. `crm` adds nothing structural over ledger here (its
  richness is all *domain* — five entities, six verbs — which stays app-side).

**Chassis pieces that do NOT cleanly extract (risks to flag for B2):**

1. **`backup`/`restore` have no extractable bodies.** The whole non-dashboard
   service family ships **no** `bin/backup`/`bin/restore` (ledger/crm/notify/
   dropbox/wiki). Only the **dashboard** has real backup/restore logic — and it
   is **divergent and un-generalizable**: it snapshots the DB **plus the apex TLS
   cert** to S3 with a 30-deep retention + `latest` pointer convention
   (`dashboard/bin/backup`), stops/copies/starts for DB consistency. So
   `appkit`'s default `backup`/`restore` verbs must be a *minimal* consistent
   SQLite snapshot/restore of `/opt/<app>/data/<app>.db`, with the dashboard's
   cert-and-S3 behavior supplied via `Spec.Backup`/`Spec.Restore` overrides. The
   verbs exist contract-wide (§1.1) but their bodies are **not** a copy-paste
   extraction — they are written fresh in appkit + overridden in the dashboard.
2. **The producer `outbox` ↔ consumer `consumer` asymmetry.** appkit cleanly owns
   the **producer** `/feed` mount (it is mechanical: construct outbox, start
   retention, mount handler). It does **not** own the **consumer** run-loop:
   notify/wiki wire `eventplane/consumer.Run` with their own `Handler` and the
   structural-vs-transport crash coupling (a consumer fault must cancel the HTTP
   server — wiki `main.go:404-423`, notify ditto). That run-the-server-and-the-
   consumer-concurrently shape is service-shaped, not chassis-shaped; appkit
   provides the server + DB handle + `CONSUMES=` manifest key and a `serve`-hook
   seam, but the consumer goroutine + cancel coupling stays in the service. Do
   **not** try to fold the consumer loop into appkit.
3. **The dashboard is not a path-routed service at all.** Its server is the apex
   OAuth-AS (`googleidp`/`oauth`/`session`/`/internal/authn`/inventory-derived
   resources), `MOUNT=/`, `DEFAULT=true`, no PRM-for-itself, no `/feed`, no
   identity-header gate (it *issues* the identity, services consume it). It uses
   `appkit` only for the **outer contract** (the six verbs, config-from-env,
   migrate, manifest emit) — its entire HTTP layer is `Spec.Handlers`, bypassing
   `appkit/server`'s PRM/identity-gate routes. This is the §5-flagged "richest
   `Spec`/`Main` use" and confirms `appkit/server` must let a service supply its
   *whole* route table, not just augment a fixed one.

---

## 4. The `appkit.Spec` field list (final)

Reconciled with the ADR draft (`docs/adr-deployment-redesign.md` §2 table). The
field set matches the ADR; types and one clarification (`Config` hook for
non-secret extras / secret reads) are pinned for B2.

| Field | Type | Purpose |
|---|---|---|
| `App` | `string` | Service name (`"ledger"`). Drives manifest `APP`, the DB filename `/opt/<app>/data/<app>.db`, install root, log identity. |
| `Mount` | `string` | The `/srv/<app>/` prefix (manifest `MOUNT`). Composes `RESOURCE_ID`/`AUTH_SERVER` from `METASPOT_DOMAIN`; builds the PRM route. Dashboard = `/` + `Default:true`. |
| `Default` | `bool` | Apex/`DEFAULT=true` flag (dashboard only). Emits `DEFAULT=true/false`. **(Added vs the ADR draft, which omitted it — the manifest carries `DEFAULT` and the byte-compare requires it; §1.1's "universal `APP/MOUNT/DEFAULT/PORT/MCP`" lists it.)** |
| `Port` | `int` | Loopback port (manifest `PORT`); server binds `127.0.0.1:$PORT`. Read from env at serve, defaulted from Spec. |
| `MCP` | `bool` | Exposes an MCP surface (manifest `MCP=true`) — what makes the dashboard include it in the AS resource list / inventory. |
| `Feed` | `string` | Producer role (empty = not a producer). Non-empty (`"/feed"`) emits `FEED=` and mounts the unauthenticated `/feed` outbox handler over `eventplane/outbox`. |
| `Consumes` | `[]string` | Consumer role (nil = not a consumer). Upstream producer names (e.g. `["dropbox"]`); emits `CONSUMES=` comma-joined. appkit emits the key only — the consumer loop is wired service-side (§3). |
| `ManifestExtras` | `[]ManifestKV` | Ordered non-secret service config the manifest must round-trip beyond universal/role keys (`WIKI_INGEST_MODEL`, `WIKI_INGEST_MAX_TOKENS`, `OUTBOX_RETENTION_DAYS`, `OUTBOX_RETENTION_MAX_ROWS`, dropbox knobs). Ordered ⇒ byte-stable `manifest`. **Secrets never appear here.** |
| `Migrations` | `embed.FS` | The app's embedded numbered forward-only migration set. appkit's runner applies unapplied higher versions, records them, and refuses to start on a DB version the binary no longer embeds (downgrade guard). |
| `Handlers` | `func(*appkit.Router) error` | Registration hook: the service registers its own routes (`/mcp`, `/content`, …) on the server appkit stands up behind the identity gate; the dashboard registers its *whole* apex route table here. The real domain surface lives here, untouched by the chassis. |
| `Config` | `func(getenv func(string) string) (any, error)` (optional) | Service-side composition-root hook: reads `ManifestExtras` non-secret config **and** the service's secrets (`ANTHROPIC_API_KEY`, `DROPBOX_*`, `NTFY_*`) from env, returns the service's own config object for `Handlers`. **(Clarifies the ADR — the ADR put domain config implicitly in `Handlers`; splitting it out keeps the secret-read at the app boundary, honoring §2.8.)** |
| `Backup` | `func(ctx, BackupReq) error` (optional; nil ⇒ default) | `backup` verb hook. Nil = appkit's default consistent SQLite snapshot of `data/<app>.db`. Non-nil = dashboard's cert+S3 snapshot (§3 risk 1). |
| `Restore` | `func(ctx, RestoreReq) error` (optional; nil ⇒ default) | `restore` verb hook. Nil = default SQLite restore. Non-nil = producer's generation-epoch re-mint / dashboard's cert restore. |

`ManifestKV` = `struct{ Key, Value string }` (ordered slice, not a map, so emit
order is deterministic for the byte-compare).

### `appkit.Main` signature

```go
// Main is the single entrypoint each service's main.go calls. It parses os.Args,
// dispatches the fixed verb (default = serve), and exits with the verb's status.
// It never returns to the caller.
func Main(spec Spec)
```

Each service's `main.go` collapses to:

```go
//go:embed migrations/*.sql
var migrationsFS embed.FS

func main() {
    appkit.Main(appkit.Spec{
        App: "ledger", Mount: "/srv/ledger/", Port: 3002, MCP: true,
        Feed:       "/feed",                    // producer
        Migrations: migrationsFS,
        ManifestExtras: []appkit.ManifestKV{
            {"OUTBOX_RETENTION_DAYS", "7"},
            {"OUTBOX_RETENTION_MAX_ROWS", "1000000"},
        },
        Handlers: registerRoutes,               // mounts the ledger MCP handler
        // Config / Consumes / Backup / Restore as the service needs
    })
}
```

(The `version` string + git SHA/dirty are package-level `var`s in `appkit`,
stamped via `-ldflags -X appkit.version=… -X appkit.commit=…` per §F1 — the ADR's
"build bugs to fix" note: target a **`var`**, not a `const`.)

---

## 5. Per-service divergences that stay app-side

| Service | Stays app-side (NOT in appkit) |
|---|---|
| **dashboard** | Its whole apex HTTP layer (OAuth AS, `googleidp`/`oauth`/`session`/`oauthstate`/`ratelimit`/`grantevents`/`audit`); the **resource derivation from `/opt/*/etc/manifest.env`** (`inventory.Read` + `deriveResources`, `DASHBOARD_MANIFEST_ROOT`) — appkit's `manifest.Parse` may *back* it, but the AS-resource policy is dashboard's; the extra lifecycle pieces `backup`(cert+S3)/`restore`/`secrets`/`teardown` (the last two are **operator-side bash**, not binary verbs); the `reset` verb. `Default:true`, `MCP` absent, no PRM-for-self, no identity gate. |
| **wiki** | The entire `agentkit` ingest/lint/ask core (`ingest.New`/`lint.New`/`ask.New`, `anthropic.New`, `core.Recover`, the job machinery), kept behind `replace agentkit => ../agentkit`; the non-secret `WIKI_INGEST_MODEL`/`WIKI_INGEST_MAX_TOKENS` (+ lint/ask variants) → declared in `Spec.ManifestExtras` **and** read via `Spec.Config`; the `ANTHROPIC_API_KEY` secret read at wiki's boundary (graceful-disable on absence), never composed in or logged; the **consumer** loop (`eventplane/consumer.Run` of dropbox's feed) + the server/consumer cancel-coupling. `CONSUMES=dropbox` is the only consumer footprint appkit emits. |
| **dropbox** | The sync daemon (`internal/dropbox/*`, the longpoll goroutine), the private `/content` handler (registered via `Handlers`), the three `DROPBOX_*` secrets (read app-side), the non-secret `DROPBOX_LONGPOLL_TIMEOUT`/`DROPBOX_MAX_ENTRY_RETRIES`/etc (`ManifestExtras`). **Producer**: `Feed:"/feed"` ⇒ appkit mounts the outbox; the three `file.*` payload builders stay in `internal/dropbox/events.go`. **No** `backup`/`restore` by design (its state is reconstructible) — its `Spec.Backup`/`Restore` stay nil, but the verbs still exist (contract); a future call is a wipe+rebootstrap, not a snapshot. |
| **notify** | The `internal/push` ntfy domain + `consumer.Handler`; the consumer loop + cancel-coupling; the `NTFY_*` secrets (read app-side, fail-loud at boot); `NOTIFY_FROM`/`NOTIFY_NTFY_BASE_URL` non-secret config. `CONSUMES=crm`. Not a producer (no `Feed`). |
| **crm / ledger / ralph** | Their `internal/<domain>` + `internal/mcp` tool descriptors + their `002_<svc>.sql` migration. crm/ledger are **producers** (`Feed:"/feed"`, `OUTBOX_RETENTION_*` extras, producer `restore` re-mints the epoch). ralph is currently a whoami-skeleton (universal keys only). |

### Producer `FEED` vs consumer `CONSUMES` (the role asymmetry, restated)

- A **producer** sets `Spec.Feed` → appkit emits `FEED=/feed`, constructs the
  `eventplane/outbox`, starts retention, mounts the unauthenticated handler. Read
  by `bin/registry feed-url <name>` (hard-requires `FEED`) so consumers resolve
  the loopback feed URL by name.
- A **consumer** sets `Spec.Consumes` → appkit emits `CONSUMES=<names>` (purely
  documentary/registry metadata) and exposes the DB handle; the *service* wires
  `eventplane/consumer.Run`. The two keys are mutually independent (a service can
  be neither, like ralph; never both today).

---

## 6. Manifest-comment policy (DECISION — unblocks C1)

**Both manifest parsers ignore comments.** Verified directly:
- `dashboard/internal/inventory/inventory.go:parseManifest` — skips `line == "" ||
  strings.HasPrefix(line, "#")`, splits on first `=`, trims, strips one quote pair.
- `bin/registry:manifest_get` — skips blank lines and `case "$line" in '#'*)`,
  same split/trim/quote-strip.

So the comment lines in the committed `etc/manifest.env` files (e.g. ledger's
`# MCP=true marks this service…`, crm's `OUTBOX_RETENTION` block) are **purely
documentary** — they carry no runtime meaning to either consumer of the file.

**Recommendation: REGENERATE the committed `etc/manifest.env` files to match a
canonical comment-free `appkit manifest` emit** (and make appkit emit *no*
comments). Justification (one line): a binary cannot be the "source of truth for
its own identity" (§1.1) while a human-authored comment block it can't reproduce
sits in the byte-compare oracle — emitting comments would force appkit to carry
per-service prose strings purely to satisfy a diff that means nothing to any
parser, so the clean, deterministic, no-information-lost choice is to drop the
comments from both the emit and the committed files.

Concretely for C1: appkit `manifest` emits keys only, in fixed order (`APP`,
`MOUNT`, `DEFAULT`, `PORT`, `MCP`, then `FEED`/`CONSUMES`, then `ManifestExtras`
in declared order); the committed `etc/manifest.env` is regenerated once from
that emit so `<app> manifest` byte-equals the committed file. No runtime behavior
changes (comments were inert). The documentary intent the comments carried moves
into each service's `CLAUDE.md` / the `Spec` declaration, which is where it
belongs.

---

## 7. Carry-forward facts confirmed (from A2)

- **Two manifest readers** confirmed: `dashboard/cmd/dashboard/main.go:136`
  (`DASHBOARD_MANIFEST_ROOT`, via `internal/inventory`) **and** root `bin/registry`
  (`REGISTRY_ROOT`). `bin/registry feed-url` hard-requires `FEED`;
  `bin/registry resource-url`/`list-mcp` and `inventory.Read` hard-require
  `MCP=true`. appkit's `manifest` emit MUST round-trip `FEED`/`CONSUMES`/`MCP` +
  the service-config extras exactly (the `ManifestExtras` ordered slice + the
  `Feed`/`Consumes` fields are what guarantee this).
- **`DASHBOARD_RESOURCES` is DEAD** — not referenced anywhere in this map. The
  dashboard derives resources from manifests at startup; adding a service is a
  dashboard restart, not an env edit.
- The committed manifests **do** currently carry comment lines → policy in §6
  (regenerate comment-free) resolves the C1 byte-compare gate.

---

## 8. The `replace` / `require v0.0.0` lines B2 must copy

`appkit` is a sibling module (`module appkit`, `go.mod`, `go 1.26`), never
tagged, consumed via committed `replace` + `require … v0.0.0` — exactly like
`eventplane`. For B2:

**In `appkit/go.mod`** (appkit itself depends on eventplane for the `feed` pkg —
copy eventplane's plain-`v0.0.0` form, from `ledger/go.mod:5-13`):

```
require (
	eventplane v0.0.0
	modernc.org/sqlite v1.50.1
)

// The shared event-plane library is a sibling source tree, not a published
// module. go.work resolves it for local dev; this committed replace makes the
// build deterministic with or without the workspace.
replace eventplane => ../eventplane
```

**Add `./appkit` to the root `go.work`** `use(...)` block (keep alphabetical;
goes between `./agentkit` and `./crm`):

```
use (
	./agentkit
	./appkit
	./crm
	...
)
```

**In each consumer's `go.mod` (added in C1/E, NOT B2)** — the line every service
gains, plain `v0.0.0` (matches `eventplane`, `ledger/go.mod:6,13`):

```
require appkit v0.0.0
replace appkit => ../appkit
```

**Note the two `require` forms in the wild** (both valid; B2 copies the
eventplane/plain form, NOT agentkit's):
- **Plain** (`eventplane v0.0.0`) — used by every current service for eventplane;
  this is the form `appkit` uses.
- **Zero-pseudo-version** (`agentkit v0.0.0-00010101000000-000000000000`,
  `wiki/go.mod:6`) — Go's auto-written form when `go mod tidy` runs on a
  `replace`d module with no tag. Functionally identical under the committed
  `replace`. Either is fine; the plain `v0.0.0` is preferred for readability.
  **HARD RULE (§1.6): never convert these `replace`s into versioned `require`s.**
```
