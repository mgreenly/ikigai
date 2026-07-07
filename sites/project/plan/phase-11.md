# Phase 11 — Serve the web surface from `share/www` through the chassis

*Realizes design Decision 12 (de-embed via `Spec.WWW`), carrying
D1/D2/D3/D6/D7/D8's retained ids onto the new substrate. Depends on the appkit
chassis providing `Config.WWWPath`, `appkit/web`, and `Spec.WWW` (appkit plan
Phases 05–07), consumed through the committed `replace appkit => ../appkit` as a
fixed external contract. Mechanically independent of Phases 12–13.*

Observable end state:

- `sites/share/www/landing.html` and
  `sites/share/www/static/{tokens.css,fonts/*}` exist with the former
  `internal/web` bytes; `sites/internal/web/` no longer exists (no `//go:embed`
  of web assets remains anywhere in sites).
- `sitesSpec()` in `cmd/sites/main.go` sets `WWW: true`; the service-side
  `GET /static/` mount (`web.StaticHandler`) is gone from `Handlers`
  (chassis-mounted); `GET /{$}` renders `landing.html` through `rt.WWW()` via the
  D1 handler shape (`rt.WWW().Render(w, "landing.html", struct{ Service, Version
  string }{rt.Service(), rt.Version()})`). The layout/store/mirror-client/`POST
  /mcp` wiring is otherwise unchanged in this phase.
- The landing/asset tests live in `cmd/sites`, loading the repo-real `share/www`
  via `appkit/web`, covering the retained D1/D2/D3/D6/D7/D8 ids and the new D12
  ids; the nginx fragment tests (`nginx_test.go`, D4/D8/D9's R-NGNX/R-4LKF ids)
  are relocated to `cmd/sites` with their assertions intact (they read
  `../../etc/nginx.conf`; adjust only the relative path if the package depth
  changes).
- `bin/start`'s `launch_sites` exports
  `SITES_WWW_PATH="$repo/sites/share/www"` (the D12 boundary-crossing line,
  verified by the live smoke, **not** Go tests).

Dependency interfaces (from D12/D1/D2, consumed as fixed contracts):

```go
// appkit/web
func Load(root string) (*web.Site, error)
func (s *Site) Render(w http.ResponseWriter, name string, data any) error
func (s *Site) Static() http.Handler
// appkit Router
func (rt *Router) WWW() *web.Site   // nil unless Spec.WWW
```

**Done when:** the suite is green — `cd sites && go build ./...`,
`cd sites && go vet ./...`, `cd sites && gofmt -l .` (no output), and
`cd sites && go test ./...` all succeed with zero failures — and:

- R-0SF5-VPQF and R-0TN2-9HH4 (D12) are covered by clearly-named tests over the
  real `sites/share/www` tree (an `appkit/web` Site loaded relative to the test
  package);
- R-LAND-3C9K, R-LAND-5E2M, R-LAND-7G4P, R-LAND-9J6R (D1), R-ROUT-4Q8B,
  R-ROUT-6S1D, R-ROUT-8U3F (D2), R-ASST-3H7N, R-ASST-5K9Q, R-ASST-7M2S (D3),
  R-HOME-9S3W (D7), R-629P-84O5, R-63HL-LWEU, R-64PH-ZO5J, R-65XE-DFW8,
  R-675A-R7MX (D8), and R-NGNX-3P6T, R-NGNX-5R8V, R-NGNX-7T1X, R-NGNX-9W4Z (D4),
  R-4LKF-FB23 (D9's nginx guard) remain covered by tests on the new substrate
  (relocated to `cmd/sites`, with the handler-driving ids exercised over the
  `appkit/web` Site / chassis static mount instead of `web.LandingHandler` /
  `web.StaticHandler`);
- `ls sites/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" sites/cmd sites/internal --include=*.go | grep -v internal/db`
  returns no matches;
- `diff sites/share/www/static/tokens.css cron/internal/web/static/tokens.css`
  (from the repo root) prints nothing (D6's conform check on the moved file — the
  cron canonical stays at `cron/internal/web/` until cron converts), and
  `sites/share/www/landing.html` still equals `cron/internal/web/landing.html`
  under the three D6 substitutions.
