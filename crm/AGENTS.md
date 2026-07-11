# crm

The **crm** service for the ikigenba single-tenant suite. A loopback-only domain
service that serves a bearer-gated MCP surface for agents and a
session-cookie-gated human web landing page under
`<account>.ikigenba.com/srv/crm/` (e.g. `int.ikigenba.com/srv/crm/`), with **no
token logic**. First demo account: **int**.

This is a greenfield repo. **Read the decisions first — do not re-derive them:**

- `../crm.bak/` — the prior fused crm+dashboard codebase. **Reference only**, do
  not depend on it. Port the CRM domain from it; leave the auth/UI behind.

The CRM's connector **skills live in the `dashboard` repo's `plugin/`**, not here.

## What this app is

A loopback-only domain service. nginx (owned by the dashboard) terminates TLS,
introspects every request via `auth_request` against the dashboard, and injects
`X-Owner-Email` / `X-Client-Id`. This service **trusts those headers** and does
no token validation of its own: the bearer-gated MCP surface for agents and the
session-cookie-gated human web landing page under `/srv/crm/` both depend on
nginx as the sole trust boundary. nginx strips the `/srv/crm/` prefix, so
internally your routes stay `/mcp`, `/health`, etc. Small business, ≤100 users:
SQLite, single instance, is correct and deliberate.

## The domain — a polymorphic CRM (5 entities, 6 verbs)

The service is a real **sales CRM**, not an address book, and it is **MCP-only**
(no REST domain routes). The full design is in `project/design/README.md`; the shape:

- **Five entities** (all ULID id, `created_at`/`updated_at`/`deleted_at` soft
  delete; every read filters `deleted_at IS NULL`): **organization**, **contact**
  (rich identity: emails/phones/tags, plus funnel fields `org_id`, `title`,
  `lifecycle`), **deal** (participants as `contacts:[{id,role}]`; `status` is
  **derived** from `stage`, never client-set), **task**, **interaction**
  (append-only timeline).
- **Six fixed MCP verbs** — the surface is a function of *verbs*, not entities:
  `search`, `get`, `save` (loose
  polymorphic upsert over org/contact/deal/task), `delete` (shallow
  soft-delete), `log` (append an interaction),
  `health`. Adding entities/fields later must **not** add tools.
- **`internal/crm/`** is the domain package, one file per entity (each a stateless
  `<entity>Store` of pure-SQL methods on a Service-owned `*sql.Tx`), plus
  `service.go` (the dispatcher seam) and `events.go`.
- **`crm.Service` (`service.go`) is the dispatcher and the only boundary:** typed
  decode of the loose `fields` JSON into `<Type>Input` → normalization (email
  lowercase, phone→E.164 via `phonenumbers`, `display_name` derivation, label
  validation) → vocabulary validation with corrective messages → rejects a
  client-supplied deal `status` → **exact** dedup probe (contact by normalized
  primary email; org by exact domain else exact name) returning a `duplicate`
  error with `existing_id` unless `force:true` → FK liveness checks → entity Save.
  It owns the tx; entities never own a transaction and never see `map[string]any`.
- **Errors:** uniform envelope `{"error":{code,message,field?,existing_id?}}`,
  closed vocabulary (`validation/not_found/duplicate/conflict`). Entities return
  typed sentinels; the dispatcher renders the wire JSON (`errorEnvelope` in
  `mcp.go`).
- **Events (first wave, in code):** `contact.created`, `contact.updated`,
  `contact.tagged`, `contact.untagged` (the last two derived from the declarative
  tag diff — the newsletter audience) appended to the eventplane outbox on the
  same tx as the write, `Ring()` after commit. Second-wave deal/interaction events
  are documented intent only, deferred until a consumer exists.

## Keep / port from `../crm.bak`

The platform scaffolding (`db` + migration runner/WAL pragma, `ids`, `logging`,
`server` routing/PRM/`requireIdentityHeaders`/health, the `mcp.go` JSON-RPC
transport, the `/mcp` + `/feed` endpoints, eventplane wiring) and the audit store
(domain mutations keyed by the header identity). The contact identity model
(multi-email/phone normalization, primary discipline, `phonenumbers`) was ported
into the new `internal/crm/` contact entity. The old contacts-only domain and its
fine-grained tools were **replaced**, not ported.

## Delete entirely (these moved to `dashboard`)

`oauth`, `oauthstate`, `session`, `googleidp`, `agentsevents`, `ratelimit`, all
of `ui/`. No login, no token store, no OAuth endpoints.

## Changes from the reference

- Replace `requireBearer` (`../crm.bak/internal/server/contacts.go`) with a thin
  `requireIdentityHeaders` that reads `X-Owner-Email`/`X-Client-Id` from nginx
  and builds the same audit identity — no token hashing, no `ValidateAccess`.
- **Bind `127.0.0.1` only.** Binding a public interface would let anyone spoof
  identity headers — it is a security defect.
- Serve one **unauthenticated** route: `/.well-known/oauth-protected-resource`
  (RFC 9728), composed from this service's resource id + the dashboard AS URL
  (both from env). It is the only route without auth.
- **Redesigned the whole domain (greenfield).** The reference's contacts-only
  address book and its ~12 fine-grained `lr_crm_*` tools were dropped and
  replaced by the 5-entity CRM behind the fixed 6-verb polymorphic surface
  (the bare-verb tools `search`/`get`/`save`/`delete`/`log`/`health`) described
  above. No REST `/contacts` routes exist or are added.
- The no-side-effect **`health`** tool returns the shared health
  envelope plus the authenticated owner email / client id — the dashboard's
  connect skill uses it to verify the chain.

## nginx fragment (not a vhost)

`opsctl setup crm` writes only `/etc/nginx/conf.d/locations/crm.conf` (its
`location /srv/crm/` + the PRM well-known location, per the suite's path-routing
model in the root `AGENTS.md`) and reloads nginx. It does **not** install a
server block and does **not** issue a TLS cert — the dashboard owns both (the
box-global apex/cert pieces are `opsctl init-box`).

## Manifest / deploy

crm is one static appkit binary (the `appkit.Main(appkit.Spec{…})` contract):
`<app>` serve + the fixed `version`/`manifest`/`migrate`/`schema`
verbs, no `run` wrapper. `etc/manifest.env` (`APP=crm`,
`MOUNT=/srv/crm/`, `DEFAULT=false`, `PORT=3100`, `MCP=true`; producer, so it also
round-trips `FEED` + the `OUTBOX_RETENTION_*` config) is emitted by `crm manifest`
and regenerated on the box by `opsctl deploy` on every swap. Shipping is the
shared repo-root `bin/ship crm` (no version arg; version is the committed
`crm/VERSION`, advanced by `bin/bump crm <field>`) → `opsctl stage` + `opsctl
deploy`; provisioning is `opsctl setup crm`. The only `bin/*` scripts crm carries are `start`/`stop`
(systemd control). No `plugin/` in this repo. **Backup note:** the binary has no
`backup`/`restore` verbs — backup/restore are box-level **opsctl** operations.
`opsctl backup crm` / `opsctl restore crm` tar `state/` to S3 (D07); restore
re-mints the event-plane epoch by deleting the `<db>.generation` sidecar, and
`opsctl rollback` restores an S3 snapshot by recency. The former per-service
`bin/backup`/`bin/restore` S3 scripts are retired and removed.
