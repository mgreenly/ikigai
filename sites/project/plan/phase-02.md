# Phase 2 — nginx fragment: the exact-match session-gated `= /srv/sites/` landing root

*Realizes design Decision 4 (the nginx session-gate fragment). Depends on Phase 1
(the service must actually serve `GET /` for the proxied location to have a target,
though this phase's done bar is provable without a running suite).*

Add one **exact-match** location to sites's nginx fragment so a logged-in browser
hitting the bare mount root `/srv/sites/` is session-gated by the dashboard cookie
and **proxied** to the service's dynamic landing page — purely additive, with all
five existing locations (PRM, bearer-gated `= /srv/sites/mcp`, the public static
tier, the private session-gated static tier, and `@sites_authn_500`) untouched.
sites is the special case: it **already** uses the `/_session-authn` gate (its
private tier), so this reuses an already-present gate.

**What gets built (the observable end state):**

- **`sites/etc/nginx.conf`** gains, alongside the existing five locations, the
  exact-match session-gated landing root:

  ```
  # Landing page — human web UI, gated by the dashboard browser SESSION (cookie),
  # validated by the dashboard-owned /_session-authn internal location (the SAME
  # gate the private static tier already uses). Exact-match `=` so it carves out
  # ONLY the bare mount root: it never shadows the /srv/sites/public/ or
  # /srv/sites/private/ tier prefixes nor the exact = /srv/sites/mcp. A failed
  # session check yields 401. Unlike the disk-backed tiers, the landing root is
  # PROXIED to the sites process, which renders the dynamic name+version page.
  location = /srv/sites/ {
      auth_request /_session-authn;
      auth_request_set $sites_session_owner $upstream_http_x_owner_email;
      proxy_set_header  X-Owner-Email $sites_session_owner;
      proxy_pass http://127.0.0.1:__PORT__/;   # trailing slash → upstream sees "/"
      proxy_set_header Host $host;
      proxy_set_header X-Forwarded-Proto $scheme;
      proxy_http_version 1.1;
  }
  ```

  - `__PORT__` stays templated (substituted from `etc/manifest.env` `PORT=3010`
    at install time) — do **not** hardcode `3010`.
  - The block is **added**, not substituted for anything: the existing
    `location = /srv/sites/.well-known/oauth-protected-resource` PRM bootstrap, the
    bearer-gated `location = /srv/sites/mcp { auth_request /_authn; … }` (with its
    `@sites_authn_500` rate-limit re-emit and `$authn_status` machinery), the
    public `location /srv/sites/public/` tier (no `auth_request`), the private
    `location /srv/sites/private/ { auth_request /_session-authn; … }` tier, and
    the `location @sites_authn_500` named block all remain verbatim.
  - It **proxies** to the upstream root (it is not an `alias` disk tier — the
    landing card is rendered in-process so it reflects the live version).
  - It omits the `@sites_authn_500` / `$authn_status` 429 machinery on purpose (a
    cookie check is not rate-budgeted) — matching the private-tier location, which
    likewise omits it.

**Proving it (this is a config artifact, not Go behavior).** Add a
genuinely-asserting Go test that reads `sites/etc/nginx.conf` from disk and
asserts over its content. Place it in a small dedicated package so it co-locates
with no domain code — e.g. `sites/internal/web/nginx_test.go` (`package web`),
reading the fragment via a path relative to the test (`../../etc/nginx.conf`), or a
focused test that locates the repo's `sites/etc/nginx.conf`. The test must
distinguish the **exact-match** `location = /srv/sites/ {` from the **prefix** tier
locations `location /srv/sites/public/ {` and `location /srv/sites/private/ {`
(e.g. by matching the `= ` form specifically).

**Done when:**

- R-NGNX-3P6T — a test reads `sites/etc/nginx.conf` and asserts it contains an
  exact-match `location = /srv/sites/ {` block (distinct from the tier prefixes
  `location /srv/sites/public/ {` and `location /srv/sites/private/ {`).
- R-NGNX-5R8V — that test asserts the exact-match block uses
  `auth_request /_session-authn` and does **not** gate the landing root with
  `auth_request /_authn`.
- R-NGNX-7T1X — that test asserts the exact-match block **proxies** to the loopback
  upstream root: `proxy_pass http://127.0.0.1:__PORT__/;` (trailing slash), not an
  `alias` disk tier.
- R-NGNX-9W4Z — a test asserts the five pre-existing locations survive: the
  `= /srv/sites/.well-known/oauth-protected-resource` PRM bootstrap, the
  bearer-gated `location = /srv/sites/mcp {` with `auth_request /_authn`, the
  public `location /srv/sites/public/ {` (no `auth_request`), the private
  `location /srv/sites/private/ {` with `auth_request /_session-authn`, and the
  `location @sites_authn_500 {` named re-emit are all still present in the file.
- The suite is green: `cd sites && go build ./...`, `cd sites && go vet ./...`,
  `cd sites && gofmt -l .` (prints nothing), `cd sites && go test ./...`, and
  `bin/check-migrations sites`.
