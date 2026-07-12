# webhooks — Research

**Non-contractual.** This doc informs the *author* before design. The autonomous
build reads only product, design, and plan — never this file. Nothing downstream
consumes it. It records options, prior art, constraints, and recommendations
gathered from the codebase so the design spine (`design/README.md`) can be
written with confidence. Where this doc and the design spine disagree, **design
wins** — design is the sole
authority for *how*. Findings here were gathered by parallel codebase exploration
and may contain illustrative (non-verbatim) snippets; treat cited code as a
pointer, not gospel, and re-read the file before relying on an exact line.

All paths below are relative to the repo root `/mnt/projects/ikigenba/webhooks/`.
The service lives at `webhooks/`. References written as `<svc>/...:NN` are
file:line pointers; some were observed in a sibling worktree (`.../main/...`) and
the same paths exist here.

---

## Open questions this research answered

1. nginx — how to add a **public, unauthenticated, proxied** ingress tier next to
   the gated MCP/PRM tiers under one `/srv/webhooks/` mount.
2. eventplane — the exact **producer API** and how to publish **durably before
   acking** the caller.
3. appkit — the **scaffold** for a brand-new deployable service.
4. MCP — how to declare the **four owner tools** and read the authenticated owner.
5. Secrets — the suite's **token-gen / verifier-at-rest / constant-time / show-once**
   conventions to reuse.
6. opsctl/bin — the full **onboarding checklist** and the free **port**.

---

## 1. nginx ingress tier (three tiers under one mount)

**Finding — the tiering pattern already exists and is blessed.** `sites/` runs
four tiers under `/srv/sites/` in a single fragment; `crm/` runs the simpler
gated+PRM shape. The load-bearing rule is nginx location precedence: an **exact
`=` match always wins over a prefix match**, regardless of order. So an open
*prefix* ingress tier can never shadow the exact-match MCP/PRM endpoints.

Prior art:
- `sites/etc/nginx.conf` — PRM (`= .../.well-known/...`, no auth), MCP
  (`= .../mcp`, gated), public static (`/srv/sites/public/`, **no auth**, `alias`
  to disk), private static (`/srv/sites/private/`, cookie-gated via
  `/_session-authn`). This is the multi-tier template.
- `crm/etc/nginx.conf` — gated `/srv/crm/` prefix + unauth PRM + a hard
  `= /srv/crm/feed { return 404; }` that shields the internal feed from the public
  mount.

**What changes for webhooks:** the open tier **proxies to the loopback service**
instead of serving disk — `proxy_pass http://127.0.0.1:__PORT__/in/;` rather than
`alias`. The trailing slash on both the location and the `proxy_pass` strips the
`/srv/webhooks/in/` prefix so the service sees `/in/...`.

**Identity hygiene (gated tiers):** on `auth_request` routes nginx sets
`X-Owner-Email`/`X-Client-Id` from the introspection subrequest via
`proxy_set_header` (last-wins, replaces any inbound value) — a client cannot
smuggle identity. On the **open** ingress tier nginx sets *no* identity headers.

**Defense-in-depth (open tier):** the suite's convention for an unauthenticated
route is that the **handler is the primary guard** and rejects any request
carrying nginx identity headers — precedent: dropbox's loopback `/content`
handler 404s if `X-Owner-Email` or `X-Forwarded-Proto` is present
(`dropbox/internal/dropbox/content.go`), and crm's `/feed` does the same. The
webhooks `/in/<name>` handler should adopt this: a legit external call arrives
with **no** identity headers; their presence means it came through the gated
front door and must be 404'd.

**Recommended fragment skeleton** (design refines exact paths/headers):
```nginx
# unauth PRM bootstrap (exact, wins over /in/ prefix)
location = /srv/webhooks/.well-known/oauth-protected-resource { proxy_pass .../.well-known/...; }
# gated MCP (exact) — auth_request /_authn, identity-hygiene set, 429-passthrough block
location = /srv/webhooks/mcp { auth_request /_authn; ...; proxy_pass .../mcp; }
# PUBLIC ingress (prefix, NO auth_request, NO identity headers) -> loopback /in/
location /srv/webhooks/in/ { proxy_pass http://127.0.0.1:__PORT__/in/; proxy_set_header Host $host; proxy_set_header X-Forwarded-Proto $scheme; }
# shield internal feed from the public mount
location = /srv/webhooks/feed { return 404; }
```

**Open design point — payload cap at the edge vs in the handler.** nginx
`client_max_body_size` could cap on the ingress tier, but the product promises a
cap regardless; safest is to enforce the cap **in the handler** (so the promise
holds even if a fragment is misconfigured), optionally mirrored in nginx. Design
decides.

**Dev wiring:** `nginx/run` loops over services, `sed`-substitutes `__PORT__`
using `bin/registry port <svc>`, and writes `locations/<svc>.conf`. Add
`webhooks` to that loop.

---

## 2. eventplane producer (durable-before-ack)

**Finding — producing is a single outbox INSERT in the same transaction as the
domain write, then a non-blocking wake.** This *is* the durability guarantee.

Authoritative producer reference: **crm** (`crm/internal/crm/service.go`,
`crm/internal/crm/events.go`). The library is `eventplane/outbox/`.

- `Outbox.Append(tx *sql.Tx, ev outbox.Event)` — append within the caller's
  transaction. `Event{Type string, Payload json.RawMessage}`. The library mints
  the event **id (ULID)** and **timestamp**; the producer does not.
- After `tx.Commit()` succeeds, call `Outbox.Ring()` to wake parked `/feed`
  connections (async; off the request's critical path).
- The outbox is a SQLite table created by a migration (e.g. crm's `003_outbox.sql`,
  DDL byte-identical to `eventplane/outbox/schema.go`). `seq INTEGER PRIMARY KEY
  AUTOINCREMENT` is load-bearing: retention may empty the table, and AUTOINCREMENT
  prevents `seq` reuse that would make consumers skip post-trim rows.
- Wire-up via appkit: when `Spec.Feed != ""`, appkit constructs the outbox over
  the shared DB, starts retention, mounts the SSE handler, and calls
  `Spec.Producer(ob)` so the service injects the outbox into its domain.
- `/feed` is **loopback-only, unauthenticated, never through nginx**, and its
  handler 404s on inbound identity headers (same defense-in-depth as §1).

**Envelope (consumer-visible):** `{ id (ULID), type, source (service name),
time (RFC3339Nano), payload }`. Consumers cursor on `<generation>.<seq>` and
dedup on the ULID `id`. v1 of webhooks owes **at-least-once** only (no dedup) —
consistent with product's no-exactly-once non-goal; duplicates are the consumer's
problem to absorb via the ULID.

**For webhooks, on a valid `/in/<name>` call:** open a tx → (optionally record the
hit) → `Append(tx, Event{Kind, Subject, Payload: <marshaled
{name, owner, body, received_at}>})` → `Commit` → `Ring` → return 2xx. The
durable-before-ack promise is satisfied because the 2xx is returned only after a
successful commit.

**Event addressing:** the suite's addressing model is the event-routing
revision (`docs/event-routing-design.md`): an event is addressed by a routing
key `<source>:<kind><subject>`, where `kind` is the fact class with the
redundant noun prefix dropped (`source` already names the domain) and
`subject` is a `/`-rooted producer-chosen routing name. The suite key map
records webhooks' direction as `webhooks:received/<hook name>` — a single
kind, per-hook precision carried by the subject (the name also stays in the
payload as detail). Design pins the exact kind/subject (a contractual-ish
constant for consumers).

**Caution:** one exploration cited a `dropbox` "webhook event builder" — treat as
**illustrative only**; dropbox is a folder-mirror producer and is not an
authoritative reference for this. Use crm/ledger.

References: `eventplane/outbox/outbox.go` (Append/Ring/New), `.../feed.go`
(envelope), `.../cursor.go`, `.../schema.go`; `crm/internal/crm/service.go`,
`.../events.go`; `appkit/feed/feed.go`.

---

## 3. appkit scaffold (new deployable service)

**Finding — a service's entry point collapses to one `appkit.Main(appkit.Spec{…})`
call; appkit owns the fixed-verb half.** appkit provides: env config
(`<APP>_PORT/_IP/_LOG_LEVEL/_DB_PATH/_GENERATION_PATH`, composes `RESOURCE_ID`/
`AUTH_SERVER` from `IKIGENBA_DOMAIN`+`Mount`), migration runner + downgrade guard,
loopback HTTP server (PRM, `/health`, gated `POST /mcp`, optional `/feed`),
producer feed mount, manifest emit/parse, and the verb dispatcher
(`serve|version|manifest|migrate|schema`, default `serve`).

**Templates:** `gmail/` (newest, producer+workers shape) and `crm/`/`sites/`.

**Spec wiring order at serve time:** `config.Resolve` → open SQLite → `Migrate`
→ (if producer) build outbox + retention → build HTTP server → **`Handlers(rt)`**
(app mounts routes, builds domain over `rt.DB()`) → **`Producer(ob)`** (inject
outbox; runs after Handlers) → **`Workers`** (run on serve ctx) → listen + block
on SIGTERM.

**Directory layout to create** (`webhooks/`):
```
cmd/webhooks/main.go                    # Spec{} + appkit.Main
internal/db/db.go                       # //go:embed migrations/*.sql; FS + test helpers
internal/db/migrations/001_schema_migrations.sql   # appkit-shared tracking table (copy)
internal/db/migrations/0002…_webhooks.sql          # domain schema (timestamped; bin/create-migration)
internal/db/migrations/0003…_outbox.sql            # outbox table (copy crm's; producer)
internal/webhooks/service.go            # domain + secret/name logic, Outbox field
internal/webhooks/events.go             # event registry + payload builder
internal/mcp/mcp.go, tools.go, tools_test.go       # MCP surface
etc/manifest.env                        # committed; <bin> manifest must byte-equal it
etc/deploy.env                          # ACCOUNT/SSH_USER/SSH_KEY/APEX_SUFFIX (no CERTBOT_EMAIL)
etc/nginx.conf                          # the three-tier fragment (§1), __PORT__ placeholder
go.mod                                  # replace appkit=>../appkit, eventplane=>../eventplane
Makefile, VERSION (0.1.0), .gitignore, .envrc (only if secrets)
```
Note: legacy `001_*` integer prefixes coexist with new timestamped migrations;
use `bin/create-migration webhooks <name>` for new ones (never hand-pick numbers).
`001_schema_migrations.sql` is copied verbatim from an existing service.

References: `appkit/appkit.go`, `appkit/verbs.go`, `appkit/config/config.go`,
`appkit/db/db.go`, `appkit/server/server.go`, `appkit/manifest/manifest.go`,
`appkit/feed/feed.go`; `gmail/cmd/gmail/main.go`, `gmail/internal/db/db.go`,
`gmail/Makefile`, `gmail/go.mod`.

---

## 4. MCP tool surface (four owner tools)

**Finding — MCP is JSON-RPC 2.0 over `POST /mcp` (no SSE).** appkit gates it with
`rt.RequireIdentity(...)`; the handler reads the authenticated owner from
`X-Owner-Email` (and `X-Client-Id`) off the request headers. Tools are declared
as descriptors `{name, description, inputSchema}` via small helpers (`desc/obj/typ`)
and dispatched by name. Results are `{content:[{type:"text", text:<json>}]}`;
errors are `{isError:true, content:[…]}` with a domain error envelope
`{error:{code,message,field?}}`. PRM is served by appkit automatically.

**Owner scoping:** existing services mostly operate over shared data, but the
pattern for per-owner scoping is to thread `id.OwnerEmail` into store queries
(`WHERE owner_email = ?`). webhooks **must** scope list/delete/rotate to the
calling owner (product: "owners see only their own").

**Minted trigger URL:** build from `rt.ResourceID()` with the trailing `mcp`
trimmed to get the mount base (`https://<domain>/srv/webhooks/`), then append the
ingress path + unguessable name → `https://<domain>/srv/webhooks/in/<name>`.
**Correction to one exploration:** do **not** put the owner email in the public
URL — the unguessable `name` is the route; owner identity stays server-side.
`sites/internal/mcp/` shows the URL-minting-from-ResourceID pattern.

**The four tools (illustrative shapes; design fixes schemas/results):**
- `create{name?}` → mints name (if not given) + secret, persists verifier, returns
  `{trigger_url, secret}` (secret **shown once**).
- `list{}` → the caller's webhooks (name, created_at, last_triggered?), **no secret**.
- `delete{name}` → removes; URL goes dead.
- `rotate{name}` → new secret (shown once), old invalidated, URL unchanged.

References: `crm/internal/mcp/mcp.go` + `tools.go` (declaration, dispatch, result/
error helpers, identity), `gmail/internal/mcp/`, `sites/internal/mcp/` (URL mint),
`appkit/server/middleware.go` (RequireIdentity / identity context),
`appkit/server/handlers.go` (PRM).

---

## 5. Secret + name primitives (reuse, don't invent)

**Finding — the dashboard already establishes every primitive webhooks needs;
there is no shared appkit crypto package (helpers are deliberately duplicated
per-service).**

- **Random generation:** `crypto/rand` → 128-bit → base32 (Crockford, no pad),
  26 chars per call. Pattern: `dashboard/internal/ids/ids.go` (`ids.New()`).
  Tokens that must be human-distinguishable use a typed prefix
  (`ms_pat_`, `ms_oat_`, …). Recommendation: webhooks gets its own
  `internal/ids` (copy), `name = ids.New()`, `secret = "ms_wh_" + ids.New() + ids.New()`.
- **Verifier at rest:** salted-free **SHA-256 hex** of the plaintext, stored as
  `*_hash`; lookups/verify via the hash. Pattern (duplicated):
  `dashboard/internal/pat/pat.go`, `dashboard/internal/oauth/authcodes.go`
  (`hashString`). Copy the trivial helper.
- **Constant-time compare** where a fetched hash is compared in memory:
  `crypto/subtle.ConstantTimeCompare` (`dashboard/internal/oauthstate/store.go`).
  Note: a `WHERE secret_hash = ?` lookup is itself timing-safe enough for the
  common path; constant-time matters if comparing after fetch.
- **Show-once:** `Create()` returns the plaintext to the caller exactly once and
  persists only the hash; render the full response before writing (no partial
  writes leaking the secret), and set `Cache-Control: no-store`. Pattern:
  `dashboard/internal/pat/pat.go` (Create returns plaintext) +
  `dashboard/internal/server/pat.go` (buffered render).

**Note:** these secrets are ordinary **application data** in webhooks' own SQLite
DB — *not* `~/.secrets/` material — so storing a verifier and returning a
plaintext once is correct and in-policy. (The webhooks service's *own* bootstrap
secrets, if any, would still come via `.envrc`/env, but the per-webhook secrets
do not.)

---

## 6. Onboarding checklist + port

**Port — verified directly from manifests:** `3000` dashboard, `3100` crm,
`3101` ledger, `3201` notify, `3002` prompts, `3200` dropbox, `3001` wiki,
`3005` cron, `3202` gmail, `3003` scripts, `3004` sites. **`3006` is the first
free port → recommended for webhooks.**

**Dev harness edits (root files — the only legitimate root touches):**
- `go.work` — add `./webhooks` to the `use (…)` block (alphabetical).
- `bin/start` — add `webhooks` to the build list; add a `launch_webhooks` (export
  `WEBHOOKS_DB_PATH`, source `.envrc` if present, run on `:3006`); add to the
  PORTS map + wait-for list.
- `bin/stop` — add `webhooks` to the shutdown list (reverse order).
- `nginx/run` — add `webhooks` to the fragment-generation loop.

**Service-local files:** §3 layout + `etc/manifest.env`
(`APP/MOUNT=/srv/webhooks//DEFAULT=false/PORT=3006/MCP=true/FEED=/feed` +
outbox retention extras), `etc/nginx.conf` (§1 three-tier), `etc/deploy.env`,
`VERSION=0.1.0`.

**No code changes elsewhere:** the dashboard auto-discovers MCP services by
globbing `*/etc/manifest.env` (`appkit/inventory`), so `MCP=true` is enough — but
the dashboard must be **restarted** to re-read manifests after adding the service.

**Build/deploy path (only when explicitly told to deploy):** `bin/bump webhooks
<level>` → `bin/ship webhooks` (static linux/amd64, `GOWORK=off`, version+commit
stamped) → on box `opsctl setup webhooks` (first time) → `opsctl stage` →
`opsctl deploy`. Reference scaffold commit: gmail's "P1 scaffold" (`a203b6d`).

References: `bin/start`, `bin/stop`, `nginx/run`, `bin/registry`, `bin/ship`,
`bin/bump`, `bin/create-migration`, `opsctl/internal/opsctl/{setup,layout,templates}.go`,
`appkit/inventory/inventory.go`, `go.work`.

---

## Decisions deferred to design (with this research's recommendation)

- **Ingress path shape** → `/srv/webhooks/in/<name>`, prefix tier, no auth_request,
  handler validates secret + rejects identity headers.
- **HTTP method/response** → accept `POST` (likely any method later), return a
  fast `2xx` (200/202) after commit; uniform rejection (same response) for
  unknown-name and bad-secret to resist enumeration; reject over-cap payloads.
- **Payload cap** → enforce in the handler (authoritative), value TBD (e.g. 64KB–1MB);
  optionally mirror with nginx `client_max_body_size`.
- **Event addressing** → single kind `received`, subject `/` + hook name (key
  `webhooks:received/<hook name>`, per the event-routing revision); webhook
  `name`, `owner`, raw `body` (size-capped), and `received_at` in the payload.
- **Secret format** → `ms_wh_` prefix + 2×`ids.New()`; SHA-256 hex verifier at
  rest; show-once; rotation replaces the hash, name/URL unchanged.
- **Name** → `ids.New()` (128-bit base32) by default; allow a user-chosen name but
  treat it as routing/obscurity only (secret is the boundary).
- **Port** → `3006`.
- **Schema** → a `webhooks` table (id, name unique, owner_email, secret_hash,
  created_at, last_triggered_at?) + the standard outbox table.

## Out of scope (confirmed, from product)

HMAC/provider-signature verification (named future feature — design must not
preclude), rate limiting, dedup/exactly-once, enable/disable, account-shared
webhooks.
