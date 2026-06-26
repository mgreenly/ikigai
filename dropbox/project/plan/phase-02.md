# Phase 2 — nginx fragment: the exact-match session-gated `= /srv/dropbox/` location

*Realizes design Decision 4 (the nginx session-gate fragment). Depends on Phase 1
(the service must actually serve `GET /` for the proxied location to have a target,
though this phase's done bar is provable without a running suite).*

Add one **exact-match** location to dropbox's nginx fragment so a logged-in browser
hitting the bare mount root `/srv/dropbox/` is session-gated by the dashboard
cookie and proxied to the service's landing page — purely additive, the existing
bearer-gated prefix, its `@dropbox_authn_500` re-emit, the `/content` 404, and the
PRM bootstrap untouched.

**What gets built (the observable end state):**

- **`dropbox/etc/nginx.conf`** gains, alongside the existing locations, the
  exact-match session-gated landing location:

  ```
  # Landing page — human web UI, gated by the dashboard browser SESSION (cookie),
  # validated by the dashboard-owned /_session-authn internal location (a sibling
  # of /_authn). Exact-match `=` so it never shadows the gated /srv/dropbox/ prefix
  # below or the MCP endpoint. A failed session check yields 401.
  location = /srv/dropbox/ {
      auth_request /_session-authn;
      auth_request_set $dropbox_session_owner $upstream_http_x_owner_email;
      proxy_set_header  X-Owner-Email $dropbox_session_owner;
      proxy_pass http://127.0.0.1:__PORT__/;   # trailing slash → upstream sees "/"
      proxy_set_header Host $host;
      proxy_set_header X-Forwarded-Proto $scheme;
      proxy_http_version 1.1;
  }
  ```

  - `__PORT__` stays templated (substituted from `etc/manifest.env` `PORT=3005`
    at install time) — do **not** hardcode `3005`.
  - The block is **added**, not substituted for anything: the existing
    `location /srv/dropbox/ { auth_request /_authn; … }` bearer prefix (with its
    `$authn_status`/`error_page 500 = @dropbox_authn_500` rate-limit re-emit), the
    `location @dropbox_authn_500` named location, the
    `location = /srv/dropbox/content { return 404; }` defence-in-depth block, and
    the `location = /srv/dropbox/.well-known/oauth-protected-resource` PRM
    bootstrap all remain verbatim.
  - It omits the `@dropbox_authn_500` / `$authn_status` 429 machinery on purpose
    (a cookie check is not rate-budgeted) — matching the `sites` private-tier
    location. It also takes only `X-Owner-Email` from the session subrequest, no
    `X-Client-Id`.
  - **Delta from the crm template:** dropbox has **no**
    `= /srv/dropbox/feed { return 404; }` location, so do **not** add or assert one
    — its `/feed` is safe on the handler's own identity-header guard. The
    retained-locations assertion enumerates the `/content` 404 and the
    `@dropbox_authn_500` named location instead.

**Proving it (this is a config artifact, not Go behavior).** Add a
genuinely-asserting Go test that reads `dropbox/etc/nginx.conf` from disk and
asserts over its content. Place it in a small dedicated package so it co-locates
with no domain code — e.g. `dropbox/internal/web/nginx_test.go` (`package web`),
reading the fragment via a path relative to the test (`../../etc/nginx.conf`), or a
focused test that locates the repo's `dropbox/etc/nginx.conf`. The test must
distinguish the **exact-match** `location = /srv/dropbox/ {` from the **prefix**
`location /srv/dropbox/ {` (e.g. by matching the `= ` form specifically).

**Done when:**

- R-NGNX-2P4Q — a test reads `dropbox/etc/nginx.conf` and asserts it contains an
  exact-match `location = /srv/dropbox/ {` block (distinct from the prefix
  `location /srv/dropbox/ {`).
- R-NGNX-4R6S — that test asserts the exact-match block uses
  `auth_request /_session-authn` and does **not** gate the landing root with
  `auth_request /_authn`.
- R-NGNX-6T8U — that test asserts the exact-match block proxies to the loopback
  upstream root: `proxy_pass http://127.0.0.1:__PORT__/;` (trailing slash).
- R-NGNX-8V1W — a test asserts the pre-existing locations survive: the
  bearer-gated `location /srv/dropbox/ {` prefix with `auth_request /_authn`, the
  `location = /srv/dropbox/content { return 404; }` defence-in-depth block, the
  `location = /srv/dropbox/.well-known/oauth-protected-resource` PRM bootstrap, and
  the `location @dropbox_authn_500` named location are all still present in the
  file.
- The suite is green: `cd dropbox && go build ./...`, `cd dropbox && go vet ./...`,
  `cd dropbox && gofmt -l .` (prints nothing), `cd dropbox && go test ./...`, and
  `bin/check-migrations dropbox`.
