# Phase 2 — nginx fragment: the exact-match session-gated `= /srv/notify/` location

*Realizes design Decision 4 (the nginx session-gate fragment). Depends on Phase 1
(the service must actually serve `GET /` for the proxied location to have a target,
though this phase's done bar is provable without a running suite).*

Add one **exact-match** location to notify's nginx fragment so a logged-in browser
hitting the bare mount root `/srv/notify/` is session-gated by the dashboard cookie
and proxied to the service's landing page — purely additive, the existing
bearer-gated prefix and the PRM bootstrap untouched. (notify is a consumer, so its
fragment has no `/feed` block to preserve.)

**What gets built (the observable end state):**

- **`notify/etc/nginx.conf`** gains, alongside the existing locations, the
  exact-match session-gated landing location:

  ```
  # Landing page — human web UI, gated by the dashboard browser SESSION (cookie),
  # validated by the dashboard-owned /_session-authn internal location (a sibling
  # of /_authn). Exact-match `=` so it never shadows the gated /srv/notify/ prefix
  # below or the MCP endpoint. A failed session check yields 401.
  location = /srv/notify/ {
      auth_request /_session-authn;
      auth_request_set $notify_session_owner $upstream_http_x_owner_email;
      proxy_set_header  X-Owner-Email $notify_session_owner;
      proxy_pass http://127.0.0.1:__PORT__/;   # trailing slash → upstream sees "/"
      proxy_set_header Host $host;
      proxy_set_header X-Forwarded-Proto $scheme;
      proxy_http_version 1.1;
  }
  ```

  - `__PORT__` stays templated (substituted from `etc/manifest.env` `PORT=3003`
    at install time) — do **not** hardcode `3003`.
  - The block is **added**, not substituted for anything: the existing
    `location /srv/notify/ { auth_request /_authn; … }` bearer prefix (with its
    `@notify_authn_500` rate-limit re-emit) and the
    `location = /srv/notify/.well-known/oauth-protected-resource` PRM bootstrap
    both remain verbatim.
  - It omits the `@notify_authn_500` / `$authn_status` 429 machinery on purpose (a
    cookie check is not rate-budgeted) — matching the `sites` private-tier
    location.

**Proving it (this is a config artifact, not Go behavior).** Add a
genuinely-asserting Go test that reads `notify/etc/nginx.conf` from disk and
asserts over its content. Place it in a small dedicated package so it co-locates
with no domain code — e.g. `notify/internal/web/nginx_test.go` (`package web`),
reading the fragment via a path relative to the test (`../../etc/nginx.conf`), or a
focused test that locates the repo's `notify/etc/nginx.conf`. The test must
distinguish the **exact-match** `location = /srv/notify/ {` from the **prefix**
`location /srv/notify/ {` (e.g. by matching the `= ` form specifically).

**Done when:**

- R-NGNX-3N6X — a test reads `notify/etc/nginx.conf` and asserts it contains an
  exact-match `location = /srv/notify/ {` block (distinct from the prefix
  `location /srv/notify/ {`).
- R-NGNX-5P8Y — that test asserts the exact-match block uses
  `auth_request /_session-authn` and does **not** gate the landing root with
  `auth_request /_authn`.
- R-NGNX-7Q1Z — that test asserts the exact-match block proxies to the loopback
  upstream root: `proxy_pass http://127.0.0.1:__PORT__/;` (trailing slash).
- R-NGNX-9R3B — a test asserts the pre-existing locations survive: the
  bearer-gated `location /srv/notify/ {` prefix with `auth_request /_authn` and the
  `location = /srv/notify/.well-known/oauth-protected-resource` PRM bootstrap are
  both still present in the file.
- The suite is green: `cd notify && go build ./...`, `cd notify && go vet ./...`,
  `cd notify && gofmt -l .` (prints nothing), `cd notify && go test ./...`, and
  `bin/check-migrations notify`.
