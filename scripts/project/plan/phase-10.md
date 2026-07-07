# Phase 10 — Serve the web surface from `share/www` through the chassis (de-embed)

*Realizes design Decision 12, carrying D1/D2/D3/D6/D7/D8's retained ids onto the
`share/www` substrate. Depends on Phase 09 only for a settled `main.go`; depends on
the appkit chassis providing `Config.WWWPath`, `appkit/web`, and `Spec.WWW` (appkit
plan Phases 05–07), consumed through the committed `replace appkit => ../appkit` as a
fixed external contract. Everything is inside `scripts/` except the one flagged
boundary-crossing `bin/start` line.*

Observable end state:

- `scripts/share/www/landing.html` and
  `scripts/share/www/static/{tokens.css,fonts/{space-grotesk,ibm-plex-sans,ibm-plex-mono-400,ibm-plex-mono-500}.woff2}`
  exist with the former `internal/web` bytes; `scripts/internal/web/` no longer
  exists (no `//go:embed` of web assets remains anywhere in scripts).
- `scriptsSpec()` sets `WWW: true`; the service-side `rt.Handle("GET /static/", …)`
  mount is gone from `registerRoutes` (chassis-mounted); `GET /{$}` renders
  `landing.html` through `rt.WWW().Render(w, "landing.html", struct{ Service, Version
  string }{rt.Service(), rt.Version()})`.
- The landing/asset/font tests and the composition-root/boot tests currently in
  `internal/web/web_test.go`, plus `internal/web/nginx_test.go`, live in
  `cmd/scripts` (package `main`), loading the repo-real `scripts/share/www` via
  `appkit/web` and driving the chassis static mount; the `{$}` mux tests keep their
  assertions. The nginx-fragment assertions are unchanged (they read
  `../../etc/nginx.conf`, the same relative depth from `cmd/scripts`, with
  registry-derived port expectations).
- `bin/start`'s `launch_scripts` exports
  `SCRIPTS_WWW_PATH="$repo/scripts/share/www"` (**the one D12 boundary-crossing
  line** — verified by the live `bin/start` smoke, not the Go suite).

**Boundary-crossing note (explicit).** The `SCRIPTS_WWW_PATH` export in `bin/start`
is the single sanctioned edit outside `scripts/`. It is dev-only (prod resolves
`share/current/www`); its proof is the live smoke (the landing page loads styled at
`:8080/srv/scripts/`), not a Go test. Add it beside the existing `launch_scripts`
exports; change nothing else in `bin/start`.

## The files move (same bytes)

- `scripts/internal/web/landing.html` → `scripts/share/www/landing.html`.
- `scripts/internal/web/static/` → `scripts/share/www/static/` (`tokens.css` +
  the four woff2 fonts). Preserve bytes exactly — D6's `diff … cron/internal/web/…`
  conform must still hold (see Done bar).

## `cmd/scripts/main.go` — opt in and render through the chassis

- Add `WWW: true` to the `scriptsSpec()` literal.
- In `registerRoutes`, replace the landing mount with the chassis render (D2's
  rewritten shape) and **delete** the `rt.Handle("GET /static/", web.StaticHandler())`
  line and the `scripts/internal/web` import:
  ```go
  rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
      _ = rt.WWW().Render(w, "landing.html", struct{ Service, Version string }{rt.Service(), rt.Version()})
  }))
  ```

## Delete `internal/web` and relocate its tests to `cmd/scripts`

Delete the whole `scripts/internal/web/` package (`embed.go`, `web.go`,
`landing.html`, `static/`, `web_test.go`, `nginx_test.go`). Move its tests to
`cmd/scripts` (package `main`), rewired to the new substrate but with their
**assertions intact**:

- The landing tests (`R-LAND-*`, D1) load `scripts/share/www` via `appkit/web`
  (`web.Load` on a relative path from `cmd/scripts`) and render `landing.html` with
  fixed service/version, asserting `200`, name+version body, `text/html`.
- The asset tests (`R-ASST-*`, D3) drive the chassis static mount over the same
  tree, asserting `text/css` for `tokens.css` and `font/woff2` for a font, and that
  the rendered page links only app-local `/static/` paths.
- The `{$}` mux tests (`R-ROUT-*`, D2) keep their exact-root/no-shadow assertions.
- The Home-link test (`R-HOME-8R2V`, D7) and the font tests (`R-M59W-5CAW`,
  `R-M6HS-J41L`, `R-M8XL-ANIZ`, `R-MA5H-OF9O` over the page/`tokens.css`; the nginx
  `R-MBDE-270D` in the relocated `nginx_test.go`) keep their assertions on the new
  substrate.
- The nginx-fragment tests (`R-NGNX-*`, D4) relocate unchanged (`../../etc/nginx.conf`,
  registry-derived ports).

## Add the D12 verification coverage (new tests in `cmd/scripts`, id-tagged)

- **R-8Z2T-SF7W** — load an `appkit/web` Site from `scripts/share/www` (relative to
  the `cmd/scripts` package dir) and render `landing.html` with an injected service
  and version; assert both appear in the output — proving the shipped template, not
  an embedded copy, is the page source. Tag `// R-8Z2T-SF7W`.
- **R-90AQ-66YL** — the chassis static mount over the same tree serves
  `GET /static/tokens.css` (`200`, `text/css`) and
  `GET /static/fonts/space-grotesk.woff2` (`200`, `font/woff2`) with **no**
  scripts-side static handler registered. Tag `// R-90AQ-66YL`.

## Done when

The suite is green (design *Conventions* commands, from `scripts/`, plus
`bin/check-migrations scripts` — no migration added) with zero failures, **and**:

- **R-8Z2T-SF7W** and **R-90AQ-66YL** (D12) are covered by clearly-named tests over
  the real `scripts/share/www` tree.
- The retained ids remain covered by tests on the new substrate: `R-LAND-7Q3D`,
  `R-LAND-9R5F`, `R-LAND-1S7G`, `R-LAND-3T9H` (D1); `R-ROUT-8U2J`, `R-ROUT-1V4K`,
  `R-ROUT-3W6L` (D2); `R-ASST-5X8M`, `R-ASST-7Y1N`, `R-ASST-9Z3P` (D3); `R-HOME-8R2V`
  (D7); `R-M59W-5CAW`, `R-M6HS-J41L`, `R-M8XL-ANIZ`, `R-MA5H-OF9O`, `R-MBDE-270D`
  (D8); `R-NGNX-2A5Q`, `R-NGNX-4B7R`, `R-NGNX-6C9S`, `R-NGNX-8D1T` (D4).
- `ls scripts/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" scripts/cmd scripts/internal --include=*.go | grep -v internal/db`
  returns no matches.
- `diff scripts/share/www/static/tokens.css cron/internal/web/static/tokens.css`
  (run from the repo root) prints nothing (D6's conform check on the moved file),
  and `scripts/share/www/landing.html` still matches `cron/internal/web/landing.html`
  on all but the three D6 substitution lines.
- `bin/start` contains `SCRIPTS_WWW_PATH="$repo/scripts/share/www"` and the live
  smoke shows the styled landing page at `:8080/srv/scripts/` (boundary-crossing
  line; not a Go test).
