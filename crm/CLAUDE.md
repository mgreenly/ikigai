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
your routes stay `/contacts`, `/mcp`, etc. Small business, ≤100 users: SQLite,
single instance, is correct and deliberate.

## Keep / port from `../crm.bak`

`contacts`, the `/contacts` REST handlers, the MCP tools and `/mcp` endpoint,
`db` (+ migrations), and its own `audit` store (domain mutations, keyed by the
header identity).

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
- Add a no-side-effect **`crm_whoami`** MCP tool returning the authenticated
  owner email — the dashboard's connect skill uses it to verify the chain.
- Rename the MCP tools `lr_crm_* -> crm_*` (drop the legacy `lr_` prefix).

## nginx fragment (not a vhost)

This service's `bin/setup` writes only `/etc/nginx/conf.d/locations/crm.conf`
(its `location /srv/crm/` + the PRM well-known location, per
`path-routing-architecture.md`) and reloads nginx. It does **not** install a
server block and does **not** issue a TLS cert — the dashboard owns both.

## Manifest / deploy

`etc/manifest.env`: `APP=crm`, `MOUNT=/srv/crm/`, `DEFAULT=false`, `PORT=3001`
(loopback). Six/seven `bin/*` scripts per `AGENTS.md`. No `plugin/` in this repo.
