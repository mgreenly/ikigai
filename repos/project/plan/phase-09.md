# Phase 9 — nginx fragment & canonical landing page

*Realizes design Decision 10. Depends on Phase 8.*

Ship `etc/nginx.conf` — the four-tier fragment (open PRM exact-match,
`= /srv/repos/feed` 404, session-gated `= /srv/repos/` + `/srv/repos/static/`
with `@login_bounce`, bearer prefix tier with header replacement and the
429-recovery block, `127.0.0.1:3007` upstream) — and the canonical landing:
`share/www/landing.html` (crm/cron-canonical Carbon layout, three
service-specific text fields), `share/www/static/tokens.css` + vendored
fonts, rendered via `rt.WWW()` at `GET /{$}` with `Spec.WWW: true`. Mirror
the crm-clone web test set plus a content-assertion `nginx_test.go`.

**Done when:** R-G1OF-AAC8, R-G2WB-O22X, and R-G448-1TTM are each covered by
a clearly-named test, and the suite is green per design Conventions.
