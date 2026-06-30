# Build phases

We build the dashboard in phases. Each phase is **bounded breadth, production
depth** (see the Scope section in `CLAUDE.md`): only the capabilities named for
that phase, but each built to ship — full hardening, error handling, security,
tests. The standing bar: a phase is not done until it works **both** on localhost
(dev) **and** deployed on its real DNS name (`int.ikigenba.com`) with real TLS.

Phases are defined just-in-time. Phase 0 and Phase 1 are done; Phase 2 (MCP and
the token leg) is now split into sub-phases 2a/2b/2c, specified below; Phase 3
(push) stays provisional until we reach it, so we don't pre-commit to a shape
we'll understand better later.

This phasing parallels how `../crm.bak` was built (its `docs/goals/01..06`), but
sliced for the dashboard's role: Phase 0 ≈ Run 1 chassis, Phase 1 ≈ Run 2 web
auth, Phase 2+ ≈ Runs 4–6 (OAuth AS, MCP, the auth contract) plus dashboard-only
work (push, plugin, service inventory).

---

## Phase 0 — Structural web app (no auth)

**Goal:** a plain Go web app with **all the bits in the right structure** it will
eventually need — it serves the index page and static assets and does structured
logging, and that's it. No auth, no identity, no sessions, no tokens. The point is
to stand up the skeleton (and the box) cleanly so every later phase has a correct
place to hang code.

### In scope

- **CLI** — single binary, subcommands `serve` (long-running HTTP) and `reset`
  (destructive DB clear, prompts unless `--yes`). Flags `--db --ip --port
  --log-level --version`, each with a matching env var.
- **Config surface** — one package; every value the app reads comes from it.
  Required values fail loudly on absence, naming the missing variable. Phase 0
  needs only the non-auth config (db path, listen addr, log level); Google/auth
  vars are added in Phase 1.
- **Persistence + migrations** — SQLite via `modernc.org/sqlite` (pure-Go, no
  cgo). WAL, `foreign_keys=ON`, `busy_timeout=5000`. Homegrown numbered
  migrations, `go:embed`'d, tracked in `schema_migrations`, each in a
  transaction on startup. Phase 0 ships migration 001 (the `schema_migrations`
  table itself); no domain tables yet.
- **Logging** — `log/slog` JSON to stdout, level from config, a ULID request id
  on every request's log context. Request-id middleware. No secrets in logs.
- **HTTP server** — `http.ServeMux`, pinned read/write/idle timeouts, graceful
  shutdown.
- **Index + assets** — `GET /` renders the index template (the real hybrid
  landing page shell, but with no identity awareness yet — just the logged-out
  view). Static assets served under `/static/` from an embedded FS. Transport
  hardening that isn't auth-dependent: `X-Content-Type-Options: nosniff` always,
  HSTS on HTTPS via the forwarded-proto signal.
- **Startup banner** — first log record reports product, version (ldflag), and
  every effective config value (secret presence, never value).
- **Deploy spine** — stand the box up now, serving the index over real TLS, so
  the deploy architecture is proven before any auth complexity confounds it:
  - `etc/manifest.env` (`APP=dashboard MOUNT=/ DEFAULT=true PORT=3000`),
    `etc/deploy.env` (`ACCOUNT=int`, SSH user/key, `CERTBOT_EMAIL`).
  - The seven `bin/*` verbs; only `bin/build` is dashboard-specific (Go build to
    `build/dashboard`). `/opt/dashboard/bin/run` entrypoint + systemd unit via
    the platform launcher (`ExecStart=/usr/local/bin/ikigenba-launch dashboard`).
  - The dashboard-owned **apex nginx `server` block** for `int.ikigenba.com`, the
    one HTTP-01 (`--webroot`) cert + renewal with `--deploy-hook "systemctl
    reload nginx"`, the ACME-challenge location, the 80→443 redirect, and
    `include /etc/nginx/conf.d/locations/*.conf;` (the dir is empty — no
    services, no `/_authn` yet).

### Out of scope

Everything identity- or token-shaped: Google federation, sessions, login/logout,
opaque tokens, OAuth AS, `/internal/authn`, MCP, push, plugin. The index renders
only its logged-out state; there is no concept of a signed-in user yet.

### Definition of done

1. **Local:** `dashboard serve` on `127.0.0.1:3000`; `http://localhost:3000`
   serves the index page and its assets; logs are structured JSON with request
   ids.
2. **Deployed:** `https://int.ikigenba.com` serves the same index over a real
   Let's Encrypt cert — HSTS present, 80→443 redirect, cert renewal wired, the
   app running under systemd via the platform launcher.
3. **Hardened:** `go test ./...` green (config fail-loud per var, migrations
   apply/idempotent/no-downgrade, server serves index + assets, security headers
   present).

### Phase-0 decisions to confirm

- **Env var naming.** crm.bak uses `CRM_*`. Pick the dashboard's prefix (likely
  `DASHBOARD_*`). Note `IKIGENBA_*` is already the platform/node-identity
  namespace in `/etc/ikigenba/env`; app config is distinct and lives in the SSM
  `app-config` blob under `dashboard`.
- **Product name / banner string** — branded **ikigenba**.

---

## Phase 1 — Login, identity-aware index, logout

**Goal:** a Google Workspace user can sign in; the index page becomes aware of
their identity (shows who they are, offers sign-out); they can sign out. This
layers identity onto the Phase 0 app — same box, now with a login leg. Proves the
external-IdP → dashboard login flow end to end.

### In scope (port of crm.bak Run 2)

- **Auth config** — add the Google/auth env vars (`GOOGLE_CLIENT_ID`,
  `GOOGLE_CLIENT_SECRET`, `GOOGLE_WORKSPACE_DOMAIN`, admin list, public base
  URL), fail-loud.
- **Google IdP seam** — `googleidp` with exactly two ops (authorize URL, code
  exchange returning `sub`, `email`, `hosted_domain`, `email_verified`); a real
  implementation and a network-free test double behind one interface.
- **OAuth state binding (web origin only)** — SQLite state store, 5-min TTL,
  bound to the browser via a binding cookie.
- **Web session store** — `web_sessions` table; cookie carries the plaintext id,
  table stores only its SHA-256 hash. Absolute + idle expiry, revoke on logout,
  session id rotated on every auth-state change.
- **Routes** — `GET /login`, `GET /oauth/google/callback` (web branch),
  `POST /logout`; and `GET /` gains its **signed-in** state (owner email + a
  sign-out form). The grants/management block stays an empty placeholder.
- **Middleware** — session lookup (cookie → hash → identity on context, never
  errors on absence), same-origin enforcement on every cookie-authenticated
  state-changing endpoint. (Request-id + security headers already exist from
  Phase 0; `Secure` cookie flag gated on the forwarded-proto signal so cookies
  work on plain-http localhost and are `Secure` on the deployed host.)
- **Federation checks** — `email_verified` true, workspace-domain match,
  ID-token audience match; on any failure no session is established. Forced
  re-auth (`prompt=login`) on the `/login` redirect.

### Out of scope

Opaque tokens, OAuth AS endpoints, `/internal/authn`, MCP, push, plugin, the
*live* grants/revocation data + `/agents/*` routes + SSE. The signed-in index
shows identity and logout only.

### Definition of done

1. **Local:** visit `http://localhost:3000`, "Sign in," complete a real Google
   Workspace login, land signed-in showing the owner email, sign out. A
   non-workspace identity is rejected with no session.
2. **Deployed:** the same flow on `https://int.ikigenba.com` — `Secure` cookies,
   HSTS, real cert.
3. **Hardened:** `go test ./...` green, including the full Run-2 security suite
   (state-binding rejections, workspace/`email_verified`/audience rejections,
   session lifecycle + idle/absolute expiry, cookie rotation on sign-in,
   same-origin rejection on `POST /logout`, forced-reauth assertion).

### Phase-1 decisions to confirm before coding

- **`GOOGLE_WORKSPACE_DOMAIN` for the `ai` box.** crm.bak's value was the old
  `logic-refinery.com` Workspace domain. Confirm the workspace domain the demo
  authenticates against (revisit for the ikigenba rebrand).
- **Public base URL / redirect URI.** The Google redirect URI must match exactly
  what's registered: `http://localhost:3000/oauth/google/callback` (dev) and
  `https://int.ikigenba.com/oauth/google/callback` (deployed) — both registered on
  the Google OAuth client. Decide whether the base URL is self-templated from the
  request `Host` or a config value; the redirect must round-trip in both
  environments either way.

---

## Phase 2 — MCP and the token leg

**Goal:** stand up the suite's second token layer — the dashboard mints its own
**opaque tokens**, enforces them at the front door via nginx `auth_request`, and
lets the owner see and revoke what's been granted. This is the architecture's
load-bearing claim (services stay dumb; the apex owns all token logic) proven end
to end. Built dashboard-first; a ~30-line **stub service** stands in for the real
crm so the full chain can be exercised locally before the crm exists.

Phase 2 is large enough to split into three sub-phases, built and verified in
order. Each obeys bounded-breadth / production-depth and the local-and-deployed
bar in its own right.

### Decisions already locked (apply across 2a–2c)

- **Token prefixes** — `ms_oat_` (access), `ms_ort_` (refresh), `ms_aco_`
  (auth code); replaces crm.bak's `lr_*`. Tokens are random, prefixed, and stored
  **hashed at rest** (SHA-256); the plaintext exists only in the response.
- **Multi-resource from the start** — RFC-8707 `resource` on authorize/token; a
  token chain may bind ≤3 service resources. We model the real shape now even
  though only one service exists, because it's cheap and the binding check in 2b
  depends on it.
- **TTLs** — carry crm.bak's values over unchanged.
- **Config** — stays `DASHBOARD_*`-prefixed; new AS/auth values fail-loud at the
  composition root like the rest.
- **Service mount prefix** — services live under the reserved `/srv/<svc>/`
  namespace (e.g. `int.ikigenba.com/srv/crm/mcp`, PRM at
  `/srv/crm/.well-known/oauth-protected-resource`), so the DEFAULT=true apex's
  many top-level routes never collide with a service. (See the path-routing model
  in the root `AGENTS.md`.)
- **Build order** — dashboard first, then the real crm. 2b/2c are tested against
  a throwaway stub service (serves its static PRM well-known doc + echoes the
  injected `X-Owner-Email`), **not** the real crm.

---

### Phase 2a — Opaque tokens + OAuth AS

**Goal:** the dashboard is a working OAuth authorization server — it registers
clients, runs the authorize/token/refresh/revoke/introspect flows, and persists
hashed opaque tokens. The minting side, standalone, before anything enforces it.

**In scope** (port of crm.bak `oauth` + extend `oauthstate`):

- **Migrations** — `oauth_chains`, `oauth_tokens`, `oauth_authcodes`,
  `dcr_clients`, `audit_log` (auth/token/grant events only — per-service audit).
- **`oauth` package** — token chains, PKCE, refresh with **reuse-detection
  cascade** (a replayed refresh token revokes the whole chain), hashed-at-rest
  storage. Ported and hardened from crm.bak `tokens.go`/`authcodes.go`/
  `clients.go`/`types.go`.
- **`oauthstate` extension** — add the MCP-context columns + `CreateMCP` / MCP
  consume alongside the existing web-origin `Create`/`Consume`.
- **Endpoints** — `GET /.well-known/oauth-authorization-server`,
  `POST /oauth/register` (DCR), `GET /oauth/authorize` (PKCE; rides the Phase-1
  Google login for the human leg), `POST /oauth/token` (auth-code + refresh),
  `POST /oauth/revoke`, `POST /oauth/introspect`.

**Out of scope:** `/internal/authn`, nginx enforcement, rate limiting, the grants
UI, any service. Tokens can be minted and introspected over loopback but nothing
gates a request on them yet.

**Definition of done:** a scripted client completes register → authorize (through
a real Google login) → token → introspect → refresh → revoke; reuse-detection
revokes the chain; tokens are hashed at rest; `go test ./...` green incl. the AS
security suite. Verified locally and deployed.

---

### Phase 2b — The auth contract

**Goal:** a request to a service is actually gated by an opaque token. nginx
`auth_request` calls the dashboard on every service request; the dashboard
introspects and answers `200` + identity headers / `401` / `429`. The stub
service mounts behind it.

**In scope:**

- **`POST /internal/authn`** — loopback-only; crm.bak's `requireBearer` logic
  lifted out of the request path. Validates the opaque token, checks resource
  binding + workspace + per-token rate limit, returns `200` with `X-Owner-Email`
  / `X-Client-Id`, or `401` (with the MCP `WWW-Authenticate` challenge) / `429`.
- **`ratelimit` package** — per-token, ported from crm.bak.
- **nginx blocks** — the `/_authn` internal location and a `/srv/<svc>/`
  service block wired through `auth_request`; local in
  `~/projects/nginx/locations/`, prod in dashboard `bin/setup`.
- **Stub service** — minimal stand-in (static PRM doc + identity echo) to prove
  the chain without the real crm.

**Out of scope:** the grants UI, service inventory, plugin, the real crm.

**Definition of done:** through the nginx front door, a request to the stub
service with a valid token gets `200` + correct injected identity; an invalid
token gets `401` + the MCP challenge; an over-limit token gets `429`; the stub
binds loopback only and is unreachable except via nginx. Verified locally and
deployed.

---

### Phase 2c — Grants UI + service inventory + plugin

**Goal:** the owner can see and revoke grants from the apex page, the box can
describe its own services, and a client can be wired up from the suite plugin.
Lights up the management half of the dashboard's role.

**In scope:**

- **`agentsevents` + `/agents/*`** — live grants/revocation list, HTML fragment
  responses, and SSE; rendered into the dormant install-card/agents markup
  already left in the index template + CSS.
- **Service inventory endpoint** — lists the box's services (name, mount, MCP
  resource URL) for the plugin's connect skill.
- **Public install snippet** — self-templated from the request host, no secrets.
- **`plugin/` + marketplace** — the one suite plugin per box (`.claude-plugin/
  marketplace.json`, `source: "./plugin"`) with the connect/doctor skill and the
  per-service skills; git-repo source in dev, dashboard-served in prod.

**Out of scope:** push (Phase 3), the real crm service (separate effort after the
dashboard is complete).

**Definition of done:** the signed-in apex page lists active grants and revokes
one live (SSE-updated); the inventory endpoint returns the stub service; the
plugin installs from the marketplace and its connect skill wires the stub's MCP
using the inventory + AS discovery. Verified locally and deployed.

---

## Phase 3 — Push (provisional)

**Goal:** web push owned by the dashboard — services publish, the dashboard owns
the keys and subscriptions.

**In scope:** VAPID keypair, subscription store, internal send API. Every push
carries `source` (service name) + `category` for per-source/category mute.
Services are publishers only; they never own VAPID or subscriptions.

(Scoped in detail when we reach it, after Phase 2 lands.)
