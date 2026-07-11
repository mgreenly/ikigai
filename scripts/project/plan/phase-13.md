# Phase 13 — session-gated locations opt into the apex `@login_bounce`

*Realizes design Decision 15 (opt into `@login_bounce`). Depends on no earlier
phase (a purely additive change to `scripts/etc/nginx.conf` plus its
content-assertion test).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the two
session-gated locations in `scripts/etc/nginx.conf`: the exact-match landing root
`= /srv/scripts/` and the asset tier `/srv/scripts/static/`. Each retains its
`auth_request /_session-authn;` and `proxy_pass`; nothing else in the fragment
changes. The bearer prefix `location /srv/scripts/` (fronting `/srv/scripts/mcp`), the PRM bootstrap, and the 404 stubs deliberately do **not** get the line.
`@login_bounce` is a dashboard-owned apex external contract (defined by the
dashboard's own `project/`, D20) that scripts only references — like
`/_session-authn`. The proof extends the existing nginx content-assertion test
(`cmd/scripts/main_test.go`, reading the fragment from disk); nginx is not run by the suite.

**Done when:** the suite is green (per design *Conventions*) and each id below is
covered by a clearly-named test reading `scripts/etc/nginx.conf` from disk:

- R-465K-NCPV — each of `= /srv/scripts/` and `/srv/scripts/static/` contains both
  `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- R-47DH-14GK — the bearer prefix `location /srv/scripts/` does **not** contain it.
- R-49T9-SNXY — the change is additive: every pre-existing location still appears
  and each session-gated location keeps its `auth_request /_session-authn;` and
  `proxy_pass` (nothing removed or rewritten).
