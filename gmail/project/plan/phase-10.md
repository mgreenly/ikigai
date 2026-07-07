# Phase 10 — Serve the web surface from `share/www` through the chassis

*Realizes design Decision 9 (de-embed via `Spec.WWW`), carrying D1/D2/D3/D7/D8's
retained ids onto the new substrate. Depends on phase 9 for a settled `main.go`
(the Spec lives at `cmd/gmail`); depends on the appkit chassis providing
`Config.WWWPath`, `appkit/web`, and `Spec.WWW` (appkit plan Phases 05–07),
consumed through the committed `replace appkit => ../appkit` as a fixed external
contract. Covers `R-9LIV-1C1D`, `R-9MQR-F3S2`.*

Observable end state:

- `gmail/share/www/landing.html` and
  `gmail/share/www/static/{tokens.css,fonts/*}` exist with the former
  `internal/web` bytes; `gmail/internal/web/` no longer exists (no `//go:embed`
  of web assets remains anywhere in gmail — only the `internal/db` migrations
  embed).
- The gmail `appkit.Spec` (in `cmd/gmail/main.go`) sets `WWW: true`; the
  service-side `GET /static/` mount is gone from `Handlers` (chassis-mounted);
  `GET /{$}` renders `landing.html` through `rt.WWW()` via the D1 handler shape
  (`landingHandler(rt.WWW(), rt.Service(), rt.Version())`).
- The landing/asset tests live in `cmd/gmail`, loading the repo-real `share/www`
  via `appkit/web`, covering the new D9 ids and the retained D1/D3/D7/D8 ids; the
  D2 mux tests and the nginx fragment tests (D4/D8/D12) are relocated to
  `cmd/gmail` with their assertions intact (the phase-08 registry-derived
  `proxy_pass` assertions move unchanged).
- `bin/start`'s gmail launch function exports
  `GMAIL_WWW_PATH="$repo/gmail/share/www"` (the D9 boundary-crossing line, the
  only sanctioned out-of-tree change, verified by the live smoke, not Go tests).

**Done when:** the suite is green — `cd gmail && go build ./...`,
`cd gmail && go vet ./...`, `cd gmail && gofmt -l .` (no output), and
`cd gmail && go test ./...` all succeed with zero failures — and:

- R-9LIV-1C1D and R-9MQR-F3S2 (D9) are covered by clearly-named tests over the
  real `gmail/share/www` tree at `cmd/gmail`;
- R-LAND-3F7K, R-LAND-5H9M, R-LAND-7J2N, R-LAND-9K4P (D1), R-ROUT-4M6Q,
  R-ROUT-6N8R, R-ROUT-8P1S (D2), R-ASST-3T5V, R-ASST-5W7X, R-ASST-7Y9Z (D3),
  R-HOME-7Q9U (D7), R-3X4A-Y8CI, R-3YC7-C037, R-3ZK3-PRTW, R-40S0-3JKL,
  R-41ZW-HBBA (D8), and R-NGNX-3B6C, R-NGNX-5D8E, R-NGNX-7F1G, R-NGNX-9H3J (D4)
  remain covered by tests on the new substrate at `cmd/gmail`;
- `ls gmail/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" gmail/cmd gmail/internal --include=*.go | grep -v internal/db`
  returns no matches;
- `diff gmail/share/www/static/tokens.css cron/internal/web/static/tokens.css`
  (repo root) prints nothing (D6's conform check on the moved file; if cron has
  itself converted, compare against `cron/share/www/static/tokens.css`).
