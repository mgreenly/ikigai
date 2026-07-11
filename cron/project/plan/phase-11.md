# Phase 11 — session-gated locations opt into the apex `@login_bounce`

*Realizes design Decision 13 (opt into `@login_bounce`). Depends on no earlier
phase (a purely additive change to `cron/etc/nginx.conf` plus its
content-assertion test).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the two
session-gated locations in `cron/etc/nginx.conf`: the exact-match landing root
`= /srv/cron/` and the asset tier `/srv/cron/static/`. Each retains its
`auth_request /_session-authn;` and `proxy_pass`; nothing else in the fragment
changes. The bearer prefix `location /srv/cron/` (fronting `/srv/cron/mcp`), the PRM bootstrap, and the 404 stubs deliberately do **not** get the line.
`@login_bounce` is a dashboard-owned apex external contract (defined by the
dashboard's own `project/`, D20) that cron only references — like
`/_session-authn`. The proof extends the existing nginx content-assertion test
(`cmd/cron/main_test.go`, reading the fragment from disk); nginx is not run by the suite.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `cron/etc/nginx.conf` from disk:

- R-3V6H-7F1M — each of `= /srv/cron/` and `/srv/cron/static/` contains both
  `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- R-3WED-L6SB — the bearer prefix `location /srv/cron/` does **not** contain it.
- R-3XM9-YYJ0 — the change is additive: every pre-existing location still appears
  and each session-gated location keeps its `auth_request /_session-authn;` and
  `proxy_pass` (nothing removed or rewritten).
