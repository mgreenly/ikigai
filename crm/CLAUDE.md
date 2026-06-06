# crm

The **crm** service for the metaspot single-tenant suite. A pure REST + MCP API
with **no UI** and **no token logic**, deployed at
`<account>.metaspot.org/srv/crm/` (e.g. `ai.metaspot.org/srv/crm/`). First demo
account: **ai**.

This is a greenfield repo. **Read the decisions first — do not re-derive them:**

- `../metaspot/AGENTS.md` — platform spec (Service layer = path routing).
- `../metaspot/docs/path-routing-architecture.md` — server-side topology + the
  auth contract you live under.
- `../metaspot/docs/connector-and-install.md` — the suite plugin + install layer.
  Note: the CRM's connector **skills live in the `dashboard` repo's `plugin/`**,
  not here.
- `../crm.bak/` — the prior fused crm+dashboard codebase. **Reference only**, do
  not depend on it. Port the CRM domain from it; leave the auth/UI behind.

If anything here conflicts with those docs, the docs win — and flag the conflict.

## What this app is

A loopback-only domain service. nginx (owned by the dashboard) terminates TLS,
introspects every request via `auth_request` against the dashboard, and injects
`X-Owner-Email` / `X-Client-Id`. This service **trusts those headers** and does
no token validation of its own. nginx strips the `/srv/crm/` prefix, so internally
your routes stay `/mcp`, `/whoami`, etc. Small business, ≤100 users: SQLite,
single instance, is correct and deliberate.

## The domain — a polymorphic CRM (5 entities, 6 verbs)

The service is a real **sales CRM**, not an address book, and it is **MCP-only**
(no REST domain routes). The full design is in `PLAN.md`; the shape:

- **Five entities** (all ULID id, `created_at`/`updated_at`/`deleted_at` soft
  delete; every read filters `deleted_at IS NULL`): **organization**, **contact**
  (rich identity: emails/phones/tags, plus funnel fields `org_id`, `title`,
  `lifecycle`), **deal** (participants as `contacts:[{id,role}]`; `status` is
  **derived** from `stage`, never client-set), **task**, **interaction**
  (append-only timeline).
- **Six fixed MCP verbs** — the surface is a function of *verbs*, not entities:
  `crm_search`, `crm_get`, `crm_save` (loose polymorphic upsert over org/contact/
  deal/task), `crm_delete` (shallow soft-delete), `crm_log` (append an
  interaction), `crm_whoami`. Adding entities/fields later must **not** add tools.
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
`server` routing/PRM/`requireIdentityHeaders`/whoami, the `mcp.go` JSON-RPC
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
  (`crm_*`) described above. No REST `/contacts` routes exist or are added.
- The no-side-effect **`crm_whoami`** tool returns the authenticated owner email —
  the dashboard's connect skill uses it to verify the chain.

## nginx fragment (not a vhost)

`opsctl setup crm` writes only `/etc/nginx/conf.d/locations/crm.conf` (its
`location /srv/crm/` + the PRM well-known location, per
`path-routing-architecture.md`) and reloads nginx. It does **not** install a
server block and does **not** issue a TLS cert — the dashboard owns both (the
box-global apex/cert pieces are `opsctl init-box`).

## Manifest / deploy

crm is one static appkit binary (the `appkit.Main(appkit.Spec{…})` contract):
`<app>` serve + the fixed `version`/`manifest`/`migrate`/`schema`/`backup`/
`restore` verbs, no `run` wrapper. `etc/manifest.env` (`APP=crm`,
`MOUNT=/srv/crm/`, `DEFAULT=false`, `PORT=3001`, `MCP=true`; producer, so it also
round-trips `FEED` + the `OUTBOX_RETENTION_*` config) is emitted by `crm manifest`
and regenerated on the box by `opsctl deploy` on every swap. Shipping is the
shared repo-root `bin/ship crm` (no version arg; version is the committed
`crm/VERSION`, advanced by `bin/bump crm <field>`) → `opsctl stage` + `opsctl
deploy`; provisioning is `opsctl setup crm`. The only `bin/*` scripts crm still carries are `start`/`stop`
(systemd control), plus `backup`/`restore` (operator-side S3 tooling — see
below). No `plugin/` in this repo. **Backup note:** the binary's `backup`/
`restore` verbs give appkit's default local DB snapshot (used by `opsctl
deploy`/`rollback`); the richer operator S3-bucket workflow + event-plane epoch
re-mint on restore is **not yet** folded into `Spec.Backup`, so crm **retains its
`bin/backup`/`bin/restore` scripts operator-side** (the same category as
`secrets`) until that fold-in (or an `opsctl backup`/`restore` verb) lands — see
`AGENTS.md`.
