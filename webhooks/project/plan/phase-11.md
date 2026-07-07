# Phase 11 — Serve the web surface from `share/www` through the chassis (de-embed)

*Realizes design Decision 11 (de-embed via `Spec.WWW`), carrying D9's retained
landing/asset ids onto the on-disk substrate. Depends on Phase 10 only for a
settled `main.go` and the registry-derived nginx assertions; depends on the appkit
chassis providing `Config.WWWPath`, `appkit/web`, and `Spec.WWW` (a fixed external
contract via the committed `replace appkit => ../appkit`). Covers `R-0GVM-EPJ9`,
`R-0I3I-SH9Y`; re-proves D9's `R-TMJH-V1NP`, `R-TNRE-8TEE`, `R-TOZA-ML53`,
`R-TQ77-0CVS`, `R-TRF3-E4MH` on the new substrate. **Read D11 and the rewritten D9
for the exact shape.***

Observable end state:

- `webhooks/share/www/landing.html` and
  `webhooks/share/www/static/{tokens.css,fonts/*}` exist with the former
  `internal/web` bytes; `webhooks/internal/web/` no longer exists (no `//go:embed`
  of web assets remains anywhere in webhooks).
- `cmd/webhooks/main.go`'s `webhooksSpec()` sets `WWW: true`; the service-side
  `GET /static/` mount is gone from `Handlers` (chassis-mounted); `GET /{$}`
  renders `landing.html` through `rt.WWW()` via the D9 `landingHandler` closure.
  (The `spec := …; spec.Handlers = …` post-construction form is untouched this
  phase — it is normalized in phase 13.)
- The landing/asset/mux/nginx tests live in `cmd/webhooks`, loading the repo-real
  `share/www` via `appkit/web.Load`, covering the retained D9 ids and the new D11
  ids; the nginx fragment tests (D7 + the phase-10 registry-derived `proxy_pass`
  assertions, R-0FNQ-0XSK) relocate to `cmd/webhooks` with their assertions intact.
- `bin/start`'s `launch_webhooks` exports
  `WEBHOOKS_WWW_PATH="$repo/webhooks/share/www"` beside its existing
  `WEBHOOKS_DB_PATH` export (the D11 boundary-crossing line, verified by the live
  smoke, not the Go suite).

**What gets changed (all inside `webhooks/`, plus the one flagged `bin/start`
line):**

- Move `internal/web/landing.html` → `share/www/landing.html` and
  `internal/web/static/` → `share/www/static/` (byte-identical).
- Set `WWW: true` in `webhooksSpec()`; add the `landingHandler(site, service,
  version)` closure and wire `rt.Handle("GET /{$}", landingHandler(rt.WWW(),
  rt.Service(), rt.Version()))`; delete the `rt.Handle("GET /static/", …)` mount.
- Delete `internal/web/` entirely (`web.go`, `embed.go`, `landing.html`,
  `static/`, `web_test.go`, `nginx_test.go`).
- Relocate the content tests to `cmd/webhooks` (loading `share/www` via
  `appkit/web.Load` and the chassis static mount): the retained D9 ids
  (R-TMJH-V1NP, R-TNRE-8TEE, R-TOZA-ML53, R-TQ77-0CVS, R-TRF3-E4MH) and the two
  new D11 ids (R-0GVM-EPJ9, R-0I3I-SH9Y). Relocate the still-untagged mux
  shadowing test rebuilt against the chassis static mount and `GET /{$}`. Relocate
  `nginx_test.go` to `cmd/webhooks` unchanged in its assertions (D7 + R-0FNQ-0XSK).
- `../bin/start` — add `export WEBHOOKS_WWW_PATH="$repo/webhooks/share/www"` to
  `launch_webhooks` (the single sanctioned boundary-crossing line; flagged, and
  verified by the live `bin/start` smoke, not the Go suite).

**Done when:** the suite is green — `cd webhooks && go build ./...`,
`cd webhooks && go vet ./...`, `cd webhooks && gofmt -l .` (no output), and
`cd webhooks && go test ./...` all succeed with zero failures — and:

- R-0GVM-EPJ9 — an `appkit/web` Site loaded from `webhooks/share/www` (resolved
  relative to the test package) renders `landing.html` with an injected service +
  version visible in the output.
- R-0I3I-SH9Y — the chassis static mount over the same tree serves
  `GET /static/tokens.css` (`200`, `text/css`) and a `.woff2` font (`200`,
  `font/woff2`) with no webhooks-side static handler registered.
- R-TMJH-V1NP, R-TNRE-8TEE, R-TOZA-ML53, R-TQ77-0CVS, R-TRF3-E4MH remain covered
  by clearly-named tests over the real `webhooks/share/www` tree (the D9
  substrate re-description).
- `ls webhooks/internal/web 2>/dev/null` reports no such directory, and
  `grep -rn "go:embed" webhooks/cmd webhooks/internal --include=*.go | grep -v internal/db`
  returns no matches.
- `diff webhooks/share/www/static/tokens.css cron/internal/web/static/tokens.css`
  (from the repo root) prints nothing (D9's cron-canonical conform check on the
  moved file).
- The `bin/start` export is present and the live `../bin/start` smoke serves the
  webhooks landing + `/static/` through `:8080` (verified by the smoke, not a Go
  test).
