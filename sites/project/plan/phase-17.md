# Phase 17 — nginx fragment: proxy the public/private tiers to the process

*Realizes design Decision 18 (nginx fragment). Depends on Phase 14 (the `/public/` and `/private/` process routes exist).*

Rewrite the two static-tier locations in `sites/etc/nginx.conf` to `proxy_pass`
to the sites process instead of serving files off disk with `alias`.

- `location /srv/sites/public/` → `proxy_pass http://127.0.0.1:3004/public/`, no
  `auth_request` (public tier unauthenticated); drop `alias`/`autoindex`/`index`.
- `location /srv/sites/private/` → keep `auth_request /_session-authn`, then
  `proxy_pass http://127.0.0.1:3004/private/`; drop `alias`/`autoindex`/`index`.
- Both carry the standard `proxy_set_header Host`/`X-Forwarded-Proto` +
  `proxy_http_version 1.1`. The fragment no longer references the on-disk state
  path. All other locations (PRM well-known, `= /srv/sites/mcp`, the landing root
  `= /srv/sites/`, `/srv/sites/static/`, `@sites_authn_500`) are unchanged.
- Extend the existing nginx-fragment content test in `cmd/sites/main_test.go`.

**Done when:** the sites suite is green with a content-assertion test over
`sites/etc/nginx.conf` covering R-R78H-DJF2 (public tier `proxy_pass` to `/public/`,
no `auth_request`), R-R8GD-RB5R (private tier `auth_request /_session-authn` +
`proxy_pass` to `/private/`), and R-R9OA-52WG (no `alias` and no on-disk state path
anywhere in the fragment; the pre-existing landing/PRM/mcp/`@sites_authn_500`
locations still present). D4's `R-NGNX-*` assertions continue to pass unchanged.
