# Phase 12 — nginx fragment: the exact-match session-gated `= /srv/prompts/` landing location

*Realizes design Decision 10 (the session-gate that makes the landing page
reachable by humans). **Structural / config phase — no R-ids.** Edits only
`prompts/etc/nginx.conf`; touches no Go code, adds no migration. Depends on Phase
11 (the upstream `GET /{$}` the new location proxies to).*

The Phase 11 landing handler is ungated in-process; nginx is its gate. This phase
adds the **exact-match** location that session-gates the bare mount root and
proxies it to the app's `GET /{$}`, beside the unchanged bearer-gated prefix.

In **`prompts/etc/nginx.conf`**, add (the existing `location /srv/prompts/ {
auth_request /_authn; ... }` prefix with its 429 re-emit handling, the existing
exact-match PRM location, and the existing `location = /srv/prompts/feed { return
404; }` loopback guard stay exactly as they are — exact matches over distinct
paths coexist, and `=` wins over the prefix for the bare root only):

```
# Landing page — human web UI, gated by the dashboard browser SESSION (cookie),
# validated by the dashboard-owned /_session-authn internal location (sibling of
# /_authn). Exact-match `=` so it never shadows the gated prefix below or the MCP
# endpoint. A failed session check yields 401.
location = /srv/prompts/ {
    auth_request /_session-authn;
    auth_request_set $prompts_owner $upstream_http_x_owner_email;
    proxy_set_header X-Owner-Email $prompts_owner;
    proxy_pass http://127.0.0.1:__PORT__/;   # trailing slash → upstream sees "/"
    proxy_set_header Host $host;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_http_version 1.1;
}
```

`__PORT__` stays the templated placeholder (substituted from `etc/manifest.env`,
`PORT=3004`, at install time, like the rest of the fragment) — do **not**
hard-code the port. The session gate is `/_session-authn` (the cookie audience),
**not** `/_authn` (the bearer audience). This matches `sites/etc/nginx.conf`'s
private-tier pattern.

**Done when:** the Go suite stays green (this phase changes no Go — `cd prompts &&
go build ./... && go vet ./... && gofmt -l . && go test ./...` and
`bin/check-migrations prompts` all unaffected) **and** the named structural check
passes:

- `prompts/etc/nginx.conf` contains an **exact-match** `location = /srv/prompts/`
  whose body uses `auth_request /_session-authn` and `proxy_pass
  http://127.0.0.1:__PORT__/` (templated port, trailing slash), and sets
  `X-Owner-Email` from the session subrequest.
- The pre-existing prefix `location /srv/prompts/` (with `auth_request /_authn`),
  the exact-match PRM location `= /srv/prompts/.well-known/oauth-protected-resource`,
  and the `= /srv/prompts/feed { return 404; }` guard are **still present and
  unchanged** — the bearer surface, PRM bootstrap, and loopback feed guard are
  not disturbed.

(Structural phase: "Ids to cover" is **(none — structural phase)**; the build
loop verifies the green suite plus the fragment check above, not an `R-id` test.)
