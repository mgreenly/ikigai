# Phase 13 — session-gated locations opt into the apex `@login_bounce`

*Realizes design Decision 15 (opt into `@login_bounce`). Depends on no earlier
phase (a purely additive change to `notify/etc/nginx.conf` plus its
content-assertion test).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the two
session-gated locations in `notify/etc/nginx.conf`: the exact-match landing root
`= /srv/notify/` and the asset tier `/srv/notify/static/`. Each retains its
`auth_request /_session-authn;` and `proxy_pass`; nothing else in the fragment
changes. The bearer prefix `location /srv/notify/` (fronting `/srv/notify/mcp`), the PRM bootstrap, and the 404 stubs deliberately do **not** get the line.
`@login_bounce` is a dashboard-owned apex external contract (defined by the
dashboard's own `project/`, D20) that notify only references — like
`/_session-authn`. The proof extends the existing nginx content-assertion test
(`cmd/notify/main_test.go`, reading the fragment from disk); nginx is not run by the suite.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `notify/etc/nginx.conf` from disk:

- R-3IZH-DPMO — each of `= /srv/notify/` and `/srv/notify/static/` contains both
  `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- R-3K7D-RHDD — the bearer prefix `location /srv/notify/` does **not** contain it.
- R-3LFA-5942 — the change is additive: every pre-existing location still appears
  and each session-gated location keeps its `auth_request /_session-authn;` and
  `proxy_pass` (nothing removed or rewritten).
