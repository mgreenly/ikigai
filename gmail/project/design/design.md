# gmail — Design (landing page)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the gmail landing page is built and *how each
behavior is proven*. The product (`project/product/product.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the mount root only;
v1 content is service name + version; the gate is `/_session-authn` and coarse;
the visual system is Carbon) but does **not** own them. This is the single,
current statement of the landing-page architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design covers gmail's web landing page and the seam it
> establishes (D1–D8) **plus** the appkit-chassis conversion (D9–D14): serving the
> web surface from `share/www` through `Spec.WWW` (D9), the MCP surface over
> `appkit/mcp` (D10), `registry` port adoption + drift guards (D11–D12),
> composition-root normalization (D13), and the `internal/db` shim deletion + doc
> truth-up (D14). The conversion is **behavior-preserving**: the normal-mailbox MCP
> tool surface (the ten mailbox verbs), the History-API poll daemon (`Workers`),
> the `mail.*` outbox producer (`Producer`/`Feed`), and the migrations keep their
> observable contracts — only their wiring moves onto the shared chassis. **No
> schema changes: this work adds no migration.**

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

- **Language / toolchain:** Go **1.26**, single module `module gmail` rooted at
  `gmail/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). The landing page
  itself touches no SQLite, but the module/build facts are unchanged.
- **Build / typecheck command:** `cd gmail && go build ./...` and
  `cd gmail && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship gmail`).
- **Test command:** `cd gmail && go test ./...`. **"The suite is green"** means:
  `cd gmail && go build ./...`, `cd gmail && go vet ./...`, `cd gmail && gofmt -l .`
  (no output), and `cd gmail && go test ./...` all
  succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`, `replace registry => ../registry` — the
  last added by D11). The web surface adds **no new third-party dependency** — the
  template loading, rendering, and static serving are the appkit chassis's
  (`appkit/web`, `Spec.WWW`); the service ships only the on-disk `share/www` tree.
- **The chassis owns the server.** gmail is `appkit.Main(appkit.Spec{…})`:
  `App:"gmail"`, `Mount:"/srv/gmail/"`, `Port:registry.MustPort("gmail")` (== 3202,
  D11), `MCP:true`, `WWW:true` (D9), `Feed:"/feed"` (event-plane producer). The
  fixed verbs (`serve`/`version`/`manifest`/`migrate`/`schema`), config-from-env,
  the loopback HTTP server + PRM + identity gate, the `/feed` mount, **and the
  www loader + `GET /static/` mount** are appkit's. The Spec is declared inline at
  the composition root (`cmd/gmail/main.go` as `gmailSpec()`, D13) and wires
  gmail's surface through the Spec hooks — `Handlers` (the landing render through
  `rt.WWW()` + the `POST /mcp` mount), `Producer` (the outbox sink), and
  `Workers` (the poll daemon). The landing route is wired through the
  **`Spec.Handlers`** hook, beside the `POST /mcp` mount; the `Producer`/`Workers`
  connector wiring is untouched.
- **nginx is the sole trust boundary.** gmail runs no token logic. nginx
  introspects every `/srv/gmail/` request against the dashboard and forwards to the
  loopback service. The landing page's gate is therefore an **nginx** concern
  (the `gmail/etc/nginx.conf` fragment), not a Go concern: the Go handler is
  mounted **ungated in-process**, exactly as `POST /mcp` relies on nginx for its
  bearer gate. gmail binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; the existing `/mcp` is the
  **bearer-gated agent** door, unchanged.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing render is tested in-process with `net/http/httptest`.** The
  page renders through an `appkit/web` Site loaded from the repo-real
  `gmail/share/www` tree (resolved relative to the `cmd/gmail` test package);
  tests render `landing.html` with fixed service/version values and drive it with
  `httptest.NewRequest` / `httptest.NewRecorder`, asserting status, body
  substrings (name, version), and `Content-Type`. **No test makes a network call
  and no test needs a running suite** — the render is deterministic over its two
  string inputs and the shipped template.
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it, asserting that the bare root path is served by the landing
  handler while a non-root path under the mux is **not** captured by `{$}`
  (Go 1.22+ pattern semantics: `{$}` matches only the exact path).
- **Shipped assets are real bytes.** The Carbon `tokens.css` and the woff2 fonts
  ship on disk in `share/www/static/` and are served by the chassis static mount
  (`Spec.WWW`); tests assert `tokens.css` is served with a CSS content type and
  that the template references the app's **own** `/static/` asset path (not a
  cross-service URL).
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test that reads
  `gmail/etc/nginx.conf` from disk and asserts the exact-match `= /srv/gmail/`
  location exists, uses `auth_request /_session-authn` (not `/_authn`), and
  proxies to the loopback upstream root — while the pre-existing bearer-gated
  `/srv/gmail/` prefix, the `= /srv/gmail/feed` 404, and the PRM well-known
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

**Web assets on disk (no service package).** The web surface is **shipped
files**, not a Go package: `gmail/share/www/landing.html` + `share/www/static/`
(`tokens.css` + the woff2 fonts), loaded and served by the chassis through
`Spec.WWW`/`appkit/web` (D9). The former `gmail/internal/web/` package is
deleted; the few-line landing render (`rt.WWW().Render("landing.html", …)`) lives
at the composition root (`cmd/gmail`), alongside `internal/db`, `internal/gmail`,
`internal/mcp`. `share/www` is the seam every later gmail web page grows from.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
