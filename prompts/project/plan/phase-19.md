# Phase 19 — Serve the web surface from `share/www` through the chassis

*Realizes design Decision 16 (de-embed via `Spec.WWW`), carrying D10's `R-LAND-*`,
D11's conform check, D12's `R-HOME-2T4X`, and D13's `R-D*-*` ids onto the new
substrate. Depends on Phase 18 only for a settled `main.go`/`promptsSpec()`;
depends on the appkit chassis providing `Config.WWWPath`, `appkit/web`, and
`Spec.WWW` (appkit plan Phases 05–07), consumed through the committed
`replace appkit => ../appkit` as a fixed external contract.*

prompts' landing page and assets are embedded (`//go:embed` in
`internal/web/efs.go`). This phase moves them to the on-disk `share/www` tree
served by the chassis, deletes `internal/web`, and relocates its tests to the
composition-root package over the shipped tree. The page content (markup, tokens,
fonts, Home link, nginx gate) is unchanged.

## Steps

- **Move the files (same bytes).** `prompts/internal/web/landing.tmpl` →
  `prompts/share/www/landing.html` (rename `.tmpl`→`.html`; body byte-identical).
  `prompts/internal/web/static/tokens.css` → `prompts/share/www/static/tokens.css`.
  `prompts/internal/web/static/fonts/*.woff2` → `prompts/share/www/static/fonts/`.
- **Opt the Spec in.** In `cmd/prompts/main.go` set `WWW: true` in
  `promptsSpec()`. In `registerRoutes`, replace the two lines
  ```go
  rt.HandleFunc("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
  rt.Handle("GET /static/", http.StripPrefix("/static/", web.StaticHandler()))
  ```
  with a single landing mount rendering through the chassis site (the service-side
  `GET /static/` mount is **deleted** — the chassis auto-mounts it):
  ```go
  rt.HandleFunc("GET /{$}", func(w http.ResponseWriter, r *http.Request) {
      _ = rt.WWW().Render(w, "landing.html",
          struct{ Service, Version string }{rt.Service(), rt.Version()})
  })
  ```
  Drop the now-unused `prompts/internal/web` import (and `http.StripPrefix`'s use,
  if `net/http` is otherwise still needed — it is, for the handler signature).
- **Delete `prompts/internal/web/`** entirely (`web.go`, `efs.go`, `landing.tmpl`,
  `static/`, `web_test.go`).
- **Relocate the web tests** to `cmd/prompts` (`package main`), loading the
  repo-real `prompts/share/www` via `appkit/web.Load` with a path relative to the
  package dir (`../../share/www`) and driving the chassis static mount, so they
  exercise the exact shipped files. Carry over every retained assertion:
  - D10 `R-LAND-PG01` (200 `text/html`), `R-LAND-NMVR` (name+version),
    `R-LAND-CARB` (on-disk `tokens.css` served `text/css`), `R-LAND-ROOT`
    (exact-root only), `R-LAND-UNGT` (ungated in-process);
  - D12 `R-HOME-2T4X` (Home link markup);
  - D13 `R-DFKP-IVZU`, `R-DGSL-WNQJ` (served `tokens.css`: `font-display: optional`,
    relative font `src`), `R-DI0I-AFH8`, `R-DJ8E-O77X` (rendered `<head>`:
    relative tokens link, crossorigin font preloads), `R-DKGB-1YYM` (nginx
    fragment — reads `../../etc/nginx.conf`, path unchanged from `cmd/prompts`).
- **Boundary-crossing dev wiring (flagged; verified by the live smoke, not Go
  tests):** in `bin/start`, `launch_prompts` gains
  `export PROMPTS_WWW_PATH="$repo/prompts/share/www"` so the repo-root-launched
  dev binary finds the source tree. This single line crosses the `prompts/`
  boundary per the workspace's standing convention.

## Tests to add

The relocated tests in `cmd/prompts` cover the retained ids on the new substrate,
plus the two new D16 ids below.

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-DIAW-ZFMC** — a test loads an `appkit/web` Site from `prompts/share/www`
  (relative to the `cmd/prompts` package) and asserts `Render("landing.html", …)`
  with an injected non-default service and version writes both strings into the
  output — proving the shipped template is the page source. *(httptest over the
  real tree)*
- **R-DJIT-D7D1** — a test drives the chassis static mount over the same tree and
  asserts `GET /static/tokens.css` returns `200` `text/css` and
  `GET /static/fonts/space-grotesk.woff2` returns `200` `font/woff2`, with **no**
  prompts-side static handler registered. *(httptest over the real tree)*
- the retained ids above (`R-LAND-*`, `R-HOME-2T4X`, `R-DFKP-IVZU`, `R-DGSL-WNQJ`,
  `R-DI0I-AFH8`, `R-DJ8E-O77X`, `R-DKGB-1YYM`) remain covered by clearly-named
  tests on the new substrate;
- `ls prompts/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" prompts/cmd prompts/internal --include=*.go | grep -v internal/db`
  returns no matches;
- `diff prompts/share/www/static/tokens.css notify/share/www/static/tokens.css`
  (from the repo root) prints nothing — the shared Carbon token set, anchored to
  the converted, on-`main` `notify` tree (D11's conform check on the moved file).
