# Phase 07 — Serve the web surface from `share/www` through the chassis

*Realizes design Decision 9 (de-embed via `Spec.WWW`), carrying D1/D2/D3/D6/D7's
retained ids onto the new substrate. Depends on Phase 06 for a settled inline
`main.go`; depends on the appkit chassis providing `Config.WWWPath`, `appkit/web`,
and `Spec.WWW` (a fixed external contract via the committed `replace appkit =>
../appkit`).*

Observable end state:

- `cron/share/www/landing.html` and
  `cron/share/www/static/{tokens.css,fonts/*.woff2}` exist with the former
  `internal/web` bytes; `cron/internal/web/` no longer exists (no `//go:embed` of
  web assets remains anywhere in cron).
- `cronSpec()` in `cmd/cron/main.go` sets `WWW: true`; the service-side
  `GET /static/` mount is gone from `Handlers` (chassis-mounted); `GET /{$}`
  renders `landing.html` through `rt.WWW()` via a `landingHandler(site *web.Site,
  service, version string) http.Handler` helper (D1's shape).
- The landing/asset/mux tests and the nginx fragment tests live in `cmd/cron`,
  loading the repo-real `share/www` via `appkit/web.Load` (a relative path from the
  package dir), covering the retained D1/D3/D6/D7 ids and the new D9 ids; the D2
  mux tests and the D4 nginx fragment tests are relocated to `cmd/cron` with their
  assertions intact.
- `bin/start`'s `launch_cron` exports `CRON_WWW_PATH="$repo/cron/share/www"` beside
  its existing `CRON_DB_PATH`/`CRON_GENERATION_PATH` exports (the D9
  boundary-crossing line — the sole sanctioned edit outside `cron/` — verified by
  the live `bin/start` smoke, **not** the Go suite).

**Done when:** the suite is green — `cd cron && go build ./...`,
`cd cron && go vet ./...`, `cd cron && gofmt -l .` (no output),
`cd cron && go test ./...`, and `bin/check-migrations cron` all succeed with zero
failures — and:

- R-LPMQ-FKBR and R-LQUM-TC2G (D9) are covered by clearly-named tests over the
  real `cron/share/www` tree;
- R-LAND-3C9K, R-LAND-5E2L, R-LAND-7G4M, R-LAND-9J6N (D1), R-ROUT-2P8Q,
  R-ROUT-4R1S, R-ROUT-6T3U (D2), R-ASST-3V7W, R-ASST-5X9Y, R-ASST-7Z2A (D3),
  R-HOME-2K4P (D6), R-21DE-LOX3, R-22LA-ZGNS, R-23T7-D8EH, R-2513-R056,
  R-2690-4RVV (D7), and R-NGNX-3B6C, R-NGNX-5D8E, R-NGNX-7F1G, R-NGNX-9H3J (D4)
  remain covered by tests on the new substrate;
- `ls cron/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" cron/cmd cron/internal --include=*.go | grep -v internal/db`
  returns no matches;
- `cron/share/www/landing.html` and `cron/share/www/static/tokens.css` exist, and
  `grep -n "WWW:[[:space:]]*true" cron/cmd/cron/main.go` matches.
