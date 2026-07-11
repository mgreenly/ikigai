# Phase 21 — session-gated locations opt into the apex `@login_bounce`

*Realizes design Decision 21 (opt into `@login_bounce`). Depends on no earlier
phase (a purely additive change to `dropbox/etc/nginx.conf` plus its
content-assertion test).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the two
session-gated locations in `dropbox/etc/nginx.conf`: the exact-match landing root
`= /srv/dropbox/` and the asset tier `/srv/dropbox/static/`. Each retains its
`auth_request /_session-authn;` and `proxy_pass`; nothing else in the fragment
changes. The bearer prefix `location /srv/dropbox/` (fronting `/srv/dropbox/mcp`), the PRM bootstrap, and the 404 stubs deliberately do **not** get the line.
`@login_bounce` is a dashboard-owned apex external contract (defined by the
dashboard's own `project/`, D20) that dropbox only references — like
`/_session-authn`. The proof extends the existing nginx content-assertion test
(`cmd/dropbox/main_test.go`, reading the fragment from disk); nginx is not run by the suite.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `dropbox/etc/nginx.conf` from disk:

- R-3MN6-J0UR — each of `= /srv/dropbox/` and `/srv/dropbox/static/` contains both
  `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- R-3NV2-WSLG — the bearer prefix `location /srv/dropbox/` does **not** contain it.
- R-3P2Z-AKC5 — the change is additive: every pre-existing location still appears
  and each session-gated location keeps its `auth_request /_session-authn;` and
  `proxy_pass` (nothing removed or rewritten).
