# sites — Design (landing page)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the sites landing page is built and *how each
behavior is proven*. The product (`project/product/product.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the bare mount root
only; v1 content is service name + version; the gate is `/_session-authn` and
coarse — and **already present** on sites; the visual system is Carbon) but does
**not** own them. This is the single, current statement of the landing-page
architecture — it is rewritten in place to stay true (stale decisions are
removed, not stacked); the history of how it got here lives in the plan.

> **Scope.** This design covers **only** sites' standardized web landing page
> and the seam it establishes. The existing sites domain (the
> `ikigenba_sites_*` MCP surface that publishes static websites, the
> public/private static tiers served from disk, the dropbox mirror sync, the
> migrations) is owned elsewhere (`sites/cmd/sites/main.go`,
> `sites/internal/sites`, `sites/internal/mcp`) and is untouched. No schema
> changes: the landing page adds **no migration**.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is
  **no separate requirements document**.
- Design's responsibility for ids ends at minting them into this doc. How
  coverage is measured, what counts as a covered id, and when the work is "done"
  are **not** design's concern — downstream phases own that.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain:** Go **1.26**, single module `module sites` rooted at
  `sites/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). The landing page
  itself touches no SQLite, but the module/build facts are unchanged.
- **Build / typecheck command:** `cd sites && go build ./...` and
  `cd sites && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship sites`).
- **Test command:** `cd sites && go test ./...`. **"The suite is green"** means:
  `cd sites && go build ./...`, `cd sites && go vet ./...`, `cd sites && gofmt -l .`
  (no output), and `cd sites && go test ./...` all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit` and `eventplane` are committed in-repo
  replace-siblings. The landing page adds **no new dependency** — its template is
  loaded and rendered by the chassis (`appkit/web`, via `Spec.WWW`; the
  page/assets ship on disk in `share/www`, **D12** — no sites-side `//go:embed`
  or `html/template`), and the route handler uses only `net/http`. **D9** adds one
  further committed replace-sibling,
  `registry` (`replace registry => ../registry`), the suite's zero-dependency
  leaf `name → port` table — used at the composition root to resolve sites's own
  port and the dropbox mirror address by name instead of by literal. **D10/D11**
  *remove* the `agentkit` replace-sibling entirely: sites was the local `agentkit`
  module's only consumer, and the confined file-tool logic it borrowed moves into
  the native `internal/files` package, so `require agentkit` and
  `replace agentkit => ../agentkit` are dropped from `go.mod` and no agentkit
  (local or remote) is imported anywhere in sites. (The orphaned repo-root
  `agentkit/` tree is left on disk — deleting it is suite-level work outside this
  `project/`.)
- **The chassis owns the server.** sites is `appkit.Main(appkit.Spec{…})`:
  `App:"sites"`, `Mount:"/srv/sites/"`, `Port:registry.MustPort("sites")` (== 3004,
  resolved by name through the shared `registry` — D9, no longer a literal),
  `MCP:true`, `WWW:true` (**D12** — the chassis loads and serves the `share/www`
  landing template and `/static/` assets), `Migrations:db.FS`. sites is **not** an
  event-plane producer — the `Feed`/`Producer`/`Workers`/`Events` hooks are
  deliberately omitted (no `/feed`); its MCP `reflection` therefore reports an
  empty event graph (D13). The fixed verbs
  (`serve`/`version`/`manifest`/`migrate`/`schema`), config-from-env, the loopback
  HTTP server + PRM + identity gate, the `appkit/mcp` transport, and the
  `appkit/web` render/static mechanism are appkit's. main.go declares sites's
  identity (the Spec) and wires its surface through the Spec hooks. The landing
  route (`GET /{$}` rendering through `rt.WWW()`) and the `POST /mcp` mount (the
  `appkit/mcp` handler over sites's declared tool table, D13) are wired through
  the existing **`Spec.Handlers`** hook, beside the layout/store/mirror-client
  wiring.
- **nginx is the sole trust boundary.** sites runs no token logic. nginx
  introspects every `/srv/sites/` request against the dashboard and forwards to
  the loopback service (or serves static tiers from disk directly). The landing
  page's gate is therefore an **nginx** concern (the `sites/etc/nginx.conf`
  fragment), not a Go concern: the Go handler is mounted **ungated in-process**,
  exactly as `POST /mcp` relies on nginx for its bearer gate. sites binds
  `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; the existing `/mcp` is the
  **bearer-gated agent** door, unchanged. sites is the one service whose
  `/_session-authn` gate is **already wired** (its private static tier uses it),
  so the landing page introduces no new suite dependency.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing surface is tested over the repo-real `share/www` tree with
  `net/http/httptest`.** The tests (in `cmd/sites` after D12) load the shipped
  tree with `appkit/web.Load` relative to the package dir, render `landing.html`
  with fixed name/version values, and drive it with `httptest.NewRequest` /
  `httptest.NewRecorder`, asserting status, body substrings (name, version), and
  `Content-Type`. **No test makes a network call and no test needs a running
  suite** — the render is pure over its two string inputs and the shipped template
  file.
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it, asserting that the bare root path is served by the landing
  handler while a non-root path under the mux is **not** captured by `{$}`
  (Go 1.22+ pattern semantics: `{$}` matches only the exact path), and a sibling
  `POST /mcp` is not shadowed.
- **Shipped assets are real bytes.** The Carbon `tokens.css` and the woff2 fonts
  ship on disk in `share/www/static` and are served by the **chassis static
  mount** (D3/D12); tests drive that mount over the real tree and assert
  `tokens.css` is served with a CSS content type and that the template references
  the app's **own** `/static/` asset path (not a cross-service URL).
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test that reads
  `sites/etc/nginx.conf` from disk and asserts the exact-match `= /srv/sites/`
  location exists, uses `auth_request /_session-authn` (not `/_authn`), and
  proxies to the loopback upstream root — while sites' **five** pre-existing
  locations (the PRM well-known, the bearer-gated `= /srv/sites/mcp`, the public
  static tier, the private session-gated static tier, and the `@sites_authn_500`
  named re-emit) remain. This is a genuine assertion over the shipped artifact,
  runnable in the same `go test ./...`.
- **Determinism.** The handler takes its name/version as plain string arguments
  (injected at the composition root from `rt.Service()`/`rt.Version()`), so its
  output is fully determined by its inputs — no clock, no network, no DB.

## Layout

The design is split for addressability so a build phase reads only the one
Decision it realizes:

- `project/design/design.md` — this spine: static cross-cutting facts only, no
  per-Decision detail.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded:
  `D01.md`, `D02.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map. It is the grep target for resolving
  an id.

**Web surface (no service package).** The landing page and its Carbon assets
live on disk in `sites/share/www/` (`landing.html` + `static/tokens.css` + the
woff2 fonts), shipped in the release `share` tier and loaded/served by the
chassis (`appkit/web`, via `Spec.WWW`) — there is **no** `sites/internal/web`
package (**D12** deleted it; the landing route is a few-line handler at the
composition root over `rt.WWW().Render`). The remaining service packages are
`internal/sites` (domain store/layout/publish/sync), `internal/files` (confined
filesystem ops, D10), `internal/mcp` (the domain **tool table** over the
`appkit/mcp` transport, D13 — no local JSON-RPC transport, no local
`health`/`reflection`), and `internal/db` (the embedded migration set + its load
guard only, D14 — no `Open`/`Migrate` wrappers). (sites already serves *static*
user pages off disk via nginx from `state/www`; the `share/www` design assets are
the service's **own** UI, a distinct concern.)

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
