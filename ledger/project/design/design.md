# ledger — Design (landing page)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the ledger landing page is built and *how each
behavior is proven*. The product (`project/product/product.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the mount root only;
v1 content is service name + version; the gate is `/_session-authn` and coarse;
the visual system is Carbon) but does **not** own them. This is the single,
current statement of the landing-page architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design covers **only** ledger's web landing page and the seam it
> establishes. The existing ledger bookkeeping domain (the immutable journal, the
> eight-verb MCP surface, the outbox producer, the migrations) is owned elsewhere
> (`ledger/CLAUDE.md`) and is untouched. No schema
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

- **Language / toolchain:** Go **1.26**, single module `module ledger` rooted at
  `ledger/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). The landing page
  itself touches no SQLite, but the module/build facts are unchanged.
- **Build / typecheck command:** `cd ledger && go build ./...` and
  `cd ledger && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship ledger`).
- **Test command:** `cd ledger && go test ./...`. **"The suite is green"** means:
  `cd ledger && go build ./...`, `cd ledger && go vet ./...`,
  `cd ledger && gofmt -l .` (no output), `cd ledger && go test ./...`, and
  `bin/check-migrations ledger` all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit` and `eventplane` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`). The landing page adds **no new
  dependency** — it uses only the standard library (`net/http`, `embed`,
  `html/template` or `text/template`) and the appkit chassis.
- **The chassis owns the server.** ledger is `appkit.Main(appkit.Spec{…})`:
  `App:"ledger"`, `Mount:"/srv/ledger/"`, `Port:3101`, `MCP:true`, `Feed:"/feed"`
  (event-plane producer). The fixed verbs (`serve`/`version`/`manifest`/`migrate`
  /`backup`/`restore`), config-from-env, the loopback HTTP server + PRM +
  identity gate, and the `/feed` mount are appkit's. main.go declares ledger's
  identity (the Spec) and wires its surface through the Spec hooks. The landing
  route is wired through the existing **`Spec.Handlers`** hook, beside the
  `POST /mcp` mount.
- **nginx is the sole trust boundary.** ledger runs no token logic. nginx
  introspects every `/srv/ledger/` request against the dashboard and forwards to
  the loopback service. The landing page's gate is therefore an **nginx** concern
  (the `ledger/etc/nginx.conf` fragment), not a Go concern: the Go handler is
  mounted **ungated in-process**, exactly as `POST /mcp` relies on nginx for its
  bearer gate. ledger binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; the existing `/mcp` is the
  **bearer-gated agent** door, unchanged.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing handler is tested in-process with `net/http/httptest`.** The
  handler is a plain `http.HandlerFunc` built from the service name and version
  strings the chassis already exposes (`rt.Service()`, `rt.Version()`); its tests
  construct it directly with fixed name/version values and drive it with
  `httptest.NewRequest` / `httptest.NewRecorder`, asserting status, body
  substrings (name, version), and `Content-Type`. **No test makes a network call
  and no test needs a running suite** — the handler is pure over its two string
  inputs and its embedded assets.
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it, asserting that the bare root path is served by the landing
  handler while a non-root path under the mux is **not** captured by `{$}`
  (Go 1.22+ pattern semantics: `{$}` matches only the exact path).
- **Embedded assets are real bytes.** The Carbon `tokens.css` and the woff2 fonts
  are embedded via `//go:embed` and served by the same handler/mux; tests assert
  the embedded `tokens.css` is served with a CSS content type and that the
  template references the app's **own** embedded asset path (not a cross-service
  URL).
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test that reads
  `ledger/etc/nginx.conf` from disk and asserts the exact-match `= /srv/ledger/`
  location exists, uses `auth_request /_session-authn` (not `/_authn`), and
  proxies to the loopback upstream root — while the pre-existing bearer-gated
  `/srv/ledger/` prefix, its `@ledger_authn_500` re-emit, and the PRM well-known
  location remain. This is a genuine assertion over the shipped artifact, runnable
  in the same `go test ./...`.
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

**New package.** The landing page introduces one new package,
`ledger/internal/web/` — the handler, the embedded `*.html` template, and the
embedded `static/` design assets (`tokens.css` + the woff2 fonts). This keeps the
web surface in one place, parallel to the existing `internal/ledger`, `internal/db`,
`internal/ids`, `internal/mcp`, `internal/server` packages, and is the seam every
later ledger web page grows from.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
