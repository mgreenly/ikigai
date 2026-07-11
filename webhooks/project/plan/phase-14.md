# Phase 14 — session-gated locations opt into the apex `@login_bounce`

*Realizes design Decision 14 (opt into `@login_bounce`). Depends on no earlier
phase (a purely additive change to `webhooks/etc/nginx.conf` plus its
content-assertion test).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the two
session-gated locations in `webhooks/etc/nginx.conf`: the exact-match landing root
`= /srv/webhooks/` and the asset tier `/srv/webhooks/static/`. Each retains its
`auth_request /_session-authn;` and `proxy_pass`; nothing else in the fragment
changes. The bearer endpoint `= /srv/webhooks/mcp`, the public ingress `location /srv/webhooks/in/`, the PRM bootstrap, the feed 404 stub, and the trailing catch-all deliberately do **not** get the line.
`@login_bounce` is a dashboard-owned apex external contract (defined by the
dashboard's own `project/`, D20) that webhooks only references — like
`/_session-authn`. The proof extends the existing nginx content-assertion test
(`cmd/webhooks/nginx_test.go`, reading the fragment from disk); nginx is not run by the suite.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `webhooks/etc/nginx.conf` from disk:

- R-4B16-6FON — each of `= /srv/webhooks/` and `/srv/webhooks/static/` contains both
  `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- R-4C92-K7FC — the bearer `= /srv/webhooks/mcp`, the public ingress `/srv/webhooks/in/`, and the trailing catch-all `location /srv/webhooks/` do **not** contain it.
- R-4DGY-XZ61 — the change is additive: every pre-existing location still appears
  and each session-gated location keeps its `auth_request /_session-authn;` and
  `proxy_pass` (nothing removed or rewritten).
