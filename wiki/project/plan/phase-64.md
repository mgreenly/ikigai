# Phase 64 — nginx fragment: the exact-match session-gated `= /srv/wiki/` landing location

*Realizes design Decision 39 (the session-gate that makes the landing page
reachable by humans). **Structural / config phase — no R-ids.** Edits only
`wiki/etc/nginx.conf`; touches no Go code, adds no migration. Depends on Phase 63
(the upstream `GET /{$}` the new location proxies to).*

The Phase 63 landing handler is ungated in-process; nginx is its gate. This phase
adds the **exact-match** location that session-gates the bare mount root and
proxies it to the app's `GET /{$}`, beside the unchanged bearer-gated prefix.

In **`wiki/etc/nginx.conf`**, add (the existing `location /srv/wiki/ {
auth_request /_authn; ... }` prefix and the existing exact-match PRM location
stay exactly as they are — exact matches over distinct paths coexist, and `=`
wins over the prefix for the bare root only):

```
# Landing page — human web UI, gated by the dashboard browser SESSION (cookie),
# validated by the dashboard-owned /_session-authn internal location (sibling of
# /_authn). Exact-match `=` so it never shadows the gated prefix below or the MCP
# endpoint. A failed session check yields 401.
location = /srv/wiki/ {
    auth_request /_session-authn;
    auth_request_set $wiki_owner $upstream_http_x_owner_email;
    proxy_set_header X-Owner-Email $wiki_owner;
    proxy_pass http://127.0.0.1:__PORT__/;   # trailing slash → upstream sees "/"
    proxy_set_header Host $host;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_http_version 1.1;
}
```

`__PORT__` stays the templated placeholder (substituted from `etc/manifest.env`
at install time, like the rest of the fragment) — do **not** hard-code the port.
The session gate is `/_session-authn` (the cookie audience), **not** `/_authn`
(the bearer audience). This matches `sites/etc/nginx.conf`'s private-tier pattern.

**Done when:** the Go suite stays green (this phase changes no Go — `cd wiki &&
go build ./... && go vet ./... && gofmt -l . && go test ./...` and
`bin/check-migrations wiki` all unaffected) **and** the named structural check
passes:

- `wiki/etc/nginx.conf` contains an **exact-match** `location = /srv/wiki/`
  whose body uses `auth_request /_session-authn` and `proxy_pass
  http://127.0.0.1:__PORT__/` (templated port, trailing slash), and sets
  `X-Owner-Email` from the session subrequest.
- The pre-existing prefix `location /srv/wiki/` (with `auth_request /_authn`) and
  the exact-match PRM location `= /srv/wiki/.well-known/oauth-protected-resource`
  are **still present and unchanged** — the bearer surface and PRM bootstrap are
  not disturbed.

(Structural phase: "Ids to cover" is **(none — structural phase)**; the build
loop verifies the green suite plus the fragment check above, not an `R-id` test.)
