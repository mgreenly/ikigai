# Phase 2 — nginx fragment: the exact-match session-gated `= /srv/gmail/` location

*Realizes design Decision 4 (the nginx session-gate fragment). Depends on Phase 1
(the service must actually serve `GET /` for the proxied location to have a target,
though this phase's done bar is provable without a running suite).*

Add one **exact-match** location to gmail's nginx fragment so a logged-in browser
hitting the bare mount root `/srv/gmail/` is session-gated by the dashboard cookie
and proxied to the service's landing page — purely additive, the existing
bearer-gated prefix, the `/feed` 404, and the PRM bootstrap untouched.

**What gets built (the observable end state):**

- **`gmail/etc/nginx.conf`** gains, alongside the existing locations, the
  exact-match session-gated landing location:

  ```
  # Landing page — human web UI, gated by the dashboard browser SESSION (cookie),
  # validated by the dashboard-owned /_session-authn internal location (a sibling
  # of /_authn). Exact-match `=` so it never shadows the gated /srv/gmail/ prefix
  # below or the MCP endpoint. A failed session check yields 401.
  location = /srv/gmail/ {
      auth_request /_session-authn;
      auth_request_set $gmail_session_owner $upstream_http_x_owner_email;
      proxy_set_header  X-Owner-Email $gmail_session_owner;
      proxy_pass http://127.0.0.1:__PORT__/;   # trailing slash → upstream sees "/"
      proxy_set_header Host $host;
      proxy_set_header X-Forwarded-Proto $scheme;
      proxy_http_version 1.1;
  }
  ```

  - `__PORT__` stays templated (substituted from `etc/manifest.env` `PORT=3008`
    at install time) — do **not** hardcode `3008`.
  - The block is **added**, not substituted for anything: the existing
    `location /srv/gmail/ { auth_request /_authn; … }` bearer prefix (with its
    `@gmail_authn_500` rate-limit re-emit), the `location = /srv/gmail/feed { return
    404; }`, and the `location = /srv/gmail/.well-known/oauth-protected-resource`
    PRM bootstrap all remain verbatim.
  - It omits the `@gmail_authn_500` / `$authn_status` 429 machinery on purpose (a
    cookie check is not rate-budgeted) — matching the `sites` private-tier
    location.

**Proving it (this is a config artifact, not Go behavior).** Add a
genuinely-asserting Go test that reads `gmail/etc/nginx.conf` from disk and asserts
over its content. Place it in a small dedicated package so it co-locates with no
domain code — e.g. `gmail/internal/web/nginx_test.go` (`package web`), reading the
fragment via a path relative to the test (`../../etc/nginx.conf`), or a focused
test that locates the repo's `gmail/etc/nginx.conf`. The test must distinguish the
**exact-match** `location = /srv/gmail/ {` from the **prefix** `location /srv/gmail/ {`
(e.g. by matching the `= ` form specifically).

**Done when:**

- R-NGNX-3B6C — a test reads `gmail/etc/nginx.conf` and asserts it contains an
  exact-match `location = /srv/gmail/ {` block (distinct from the prefix
  `location /srv/gmail/ {`).
- R-NGNX-5D8E — that test asserts the exact-match block uses
  `auth_request /_session-authn` and does **not** gate the landing root with
  `auth_request /_authn`.
- R-NGNX-7F1G — that test asserts the exact-match block proxies to the loopback
  upstream root: `proxy_pass http://127.0.0.1:__PORT__/;` (trailing slash).
- R-NGNX-9H3J — a test asserts the pre-existing locations survive: the
  bearer-gated `location /srv/gmail/ {` prefix with `auth_request /_authn`, the
  `location = /srv/gmail/feed { return 404; }`, and the
  `location = /srv/gmail/.well-known/oauth-protected-resource` PRM bootstrap are all
  still present in the file.
- The suite is green: `cd gmail && go build ./...`, `cd gmail && go vet ./...`,
  `cd gmail && gofmt -l .` (prints nothing), `cd gmail && go test ./...`, and
  `bin/check-migrations gmail`.
