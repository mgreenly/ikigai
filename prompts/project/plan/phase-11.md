# Phase 11 — The landing page: `internal/web` + `LandingHandler` wired at `GET /{$}`

*Realizes design Decision 10 (the session-gated human web surface). New package
`internal/web`; touches the composition root `cmd/prompts/main.go`
(`registerRoutes`). No migration, no DB, no LLM, no runner. Depends only on the
appkit `Router` interface (`HandleFunc`, `Handle`, `Service()`, `Version()`)
already used by `registerRoutes`.*

prompts gains its first HTML web page — a Carbon-styled landing page at the bare
mount root showing the service **name** and **version**. It is added beside the
unchanged `POST /mcp`/`/health`/PRM/`/feed` surfaces and is mounted **ungated
in-process** (nginx is the gate, added in Phase 12).

In a new **`prompts/internal/web`** package:

- Embed the page assets via `//go:embed`, mirroring the dashboard's `ui/`
  precedent (`dashboard/ui/efs.go` exposes `//go:embed html static` as an
  `embed.FS`): a `landing.tmpl` (Go `html/template`) plus a `static/` dir holding
  `tokens.css` and the woff2 font file(s) (IBM Plex Sans/Mono, Space Grotesk),
  **copied** from the repo-root Carbon source (`design/tokens.css` +
  `dashboard/ui/static/fonts/`; rules in `design/carbon.md`, reference
  `design/example.html`). The binary must stay one static file — assets ship
  in-binary, never read from disk at runtime.
- `LandingHandler(service, version string) http.HandlerFunc` — renders
  `landing.tmpl` with the injected name+version, writing
  `Content-Type: text/html; charset=utf-8` and 200. The page is a **pure
  function** of name+version: no DB, no LLM, no runner, no identity header read.
  The template references the embedded `tokens.css`/fonts via a sibling static
  route.
- `StaticHandler() http.Handler` over the embedded `static/` sub-FS, serving
  `tokens.css` + fonts with correct content-types.
- Carbon visual form: a single centered card — name in display type (Space
  Grotesk), version as a mono label (IBM Plex Mono); monochrome neutrals, blue
  `#2563EB` the only signal color, 4px grid. Minimal, no JS.

In **`cmd/prompts/main.go`** (`registerRoutes`, beside the existing
`rt.Handle("POST /mcp", rt.RequireIdentity(...))` wiring), mount **ungated** — do
**not** wrap in `rt.RequireIdentity`:

```go
rt.HandleFunc("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
rt.Handle("GET /static/", http.StripPrefix("/static/", web.StaticHandler()))
```

`{$}` matches only the exact root path `/`, so the landing route cannot shadow
`/mcp`, `/health`, `/feed`, or the PRM doc.

**Done when:** the suite is green (per design *Conventions* — `cd prompts &&
go build ./... && go vet ./... && gofmt -l . && go test ./...` and
`bin/check-migrations prompts`) and these ids are covered by clearly-named tests
in `prompts/internal/web/web_test.go` (`package web`), driving the handlers via
`net/http/httptest` (no DB, no LLM, no runner, no live call):

- **R-LAND-PG01** — `GET /` (exact root) returns **200** with
  `Content-Type: text/html; charset=utf-8`.
- **R-LAND-NMVR** — the rendered body contains the injected **service name** and
  **version**; built with a non-default version string, the page shows that exact
  version (proving it renders the injected value, not a hard-coded literal).
- **R-LAND-CARB** — the embedded `embed.FS` contains `tokens.css` and the woff2
  font(s), the served page references `tokens.css`, and a request for the
  embedded `tokens.css` asset returns 200 with a CSS content-type — assets ship
  in-binary, not from disk.
- **R-LAND-ROOT** — the landing handler is bound to the exact root only: a
  request whose path is not `/` (e.g. `/health`, `/mcp`, `/nope`) routed through
  the same `{$}`-pattern mux does **not** receive the landing page — the mount
  does not shadow the other routes.
- **R-LAND-UNGT** — the landing route is served **ungated in-process**: an
  httptest request carrying **no** identity header and no bearer still gets the
  200 page, proving the route trusts nginx as the sole gate (it is not wrapped in
  `RequireIdentity`).
