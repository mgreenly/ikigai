# Phase 8 — Serve the web surface from `share/www` through the chassis

*Realizes design Decision 10 (de-embed via `Spec.WWW`), carrying D1/D2/D3/D7/D8's
retained ids onto the new substrate. Depends on Phase 07 only for a settled
`main.go`/`go.mod`; depends on the appkit chassis providing `Config.WWWPath`,
`appkit/web`, and `Spec.WWW` (consumed through the committed `replace appkit =>
../appkit` as a fixed external contract). **Read D10, and D1/D2/D3/D8 for the
retained page/asset behaviors.***

Observable end state:

- `ledger/share/www/landing.html` and
  `ledger/share/www/static/{tokens.css,fonts/*}` exist with the former
  `internal/web` bytes (byte-for-byte); `ledger/internal/web/` no longer exists (no
  `//go:embed` of web assets remains anywhere in ledger).
- `cmd/ledger/main.go` sets `WWW: true`; both service-side static mounts (the
  `rt.Handle("GET /static/{file...}", …)` line **and** the `/static/` branch that
  lived inside `LandingHandler`) are gone — the chassis mounts `GET /static/`;
  `GET /{$}` renders `landing.html` through `rt.WWW()` via the D1 handler shape
  (`landingHandler(rt.WWW(), rt.Service(), rt.Version())`).
- The landing/asset tests live in `cmd/ledger`, loading the repo-real `share/www`
  via `appkit/web`, covering the retained D1/D3/D7/D8 ids and the new D10 ids; the
  D2 mux tests and the nginx fragment tests (D4/D8, plus D9's registry-derived
  proxy targets) are rewired/relocated to `cmd/ledger` with their assertions intact.
- `bin/start`'s `launch_ledger` exports
  `LEDGER_WWW_PATH="$repo/ledger/share/www"` (the D10 boundary-crossing line,
  verified by the live `bin/start` smoke, **not** the Go suite).

**Done when:** the suite is green — `cd ledger && go build ./...`,
`cd ledger && go vet ./...`, `cd ledger && gofmt -l .` (no output), and
`cd ledger && go test ./...` all succeed with zero failures — and:

- R-509H-WUP9 and R-51HE-AMFY (D10) are covered by clearly-named tests over the
  real `ledger/share/www` tree in `cmd/ledger`;
- R-LAND-3C9D, R-LAND-5E1F, R-LAND-7G2H, R-LAND-9J4K (D1), R-ROUT-2M6N,
  R-ROUT-4P8Q, R-ROUT-6R1S (D2), R-ASST-3T7V, R-ASST-5W9X, R-ASST-7Y2Z (D3),
  R-HOME-4M6R (D7), R-7AW0-4QF8, R-7DBS-W9WM, R-7EJP-A1NB, R-7FRL-NTE0,
  R-7GZI-1L4P (D8), R-NGNX-2B4C, R-NGNX-4D6E, R-NGNX-6F8G, R-NGNX-8H1J (D4), and
  R-4XTP-5B7V (D9) remain covered by tests on the new substrate (relocated to
  `cmd/ledger`);
- `ls ledger/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" ledger/cmd ledger/internal --include=*.go | grep -v internal/db`
  returns no matches;
- `test -f ledger/share/www/landing.html && test -f ledger/share/www/static/tokens.css`
  and `test -d ledger/share/www/static/fonts` all succeed (the assets ship on disk).
