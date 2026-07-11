# Phase 13 — session-gated locations opt into the apex `@login_bounce`

*Realizes design Decision 15 (opt into `@login_bounce`). Depends on no earlier
phase (a purely additive change to `gmail/etc/nginx.conf` plus its
content-assertion test).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the two
session-gated locations in `gmail/etc/nginx.conf`: the exact-match landing root
`= /srv/gmail/` and the asset tier `/srv/gmail/static/`. Each retains its
`auth_request /_session-authn;` and `proxy_pass`; nothing else in the fragment
changes. The bearer prefix `location /srv/gmail/` (fronting `/srv/gmail/mcp`), the PRM bootstrap, and the 404 stubs deliberately do **not** get the line.
`@login_bounce` is a dashboard-owned apex external contract (defined by the
dashboard's own `project/`, D20) that gmail only references — like
`/_session-authn`. The proof extends the existing nginx content-assertion test
(`cmd/gmail/nginx_test.go`, reading the fragment from disk); nginx is not run by the suite.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `gmail/etc/nginx.conf` from disk:

- R-3YU6-CQ9P — each of `= /srv/gmail/` and `/srv/gmail/static/` contains both
  `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- R-4022-QI0E — the bearer prefix `location /srv/gmail/` does **not** contain it.
- R-419Z-49R3 — the change is additive: every pre-existing location still appears
  and each session-gated location keeps its `auth_request /_session-authn;` and
  `proxy_pass` (nothing removed or rewritten).
