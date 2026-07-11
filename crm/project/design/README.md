# crm — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* crm's ralph-governed surfaces are built and *how
each behavior is proven*. The product (`project/product/README.md`) owns the
*why*, *for whom*, and the user-facing promises; design states the **exact,
checkable form** of those promises and never re-declares the why. Design *uses*
the product's contractual constants by value (the landing page lives at the mount
root only; v1 content is service name + version; the gate is `/_session-authn`
and coarse; the visual system is Carbon; the MCP surface is self-sufficient with
no external skill; discovery describes but does not change behavior and adds no
per-entity tool) but does **not** own them. This is the single, current statement
of the architecture — it is rewritten in place to stay true (stale decisions are
removed, not stacked); the history of how it got here lives in the plan.

> **Scope.** This design covers three crm surfaces: **(1)** the web landing page
> and the seam it establishes (D1–D8), **(2)** the agent-facing **MCP discovery
> surface** — the service `instructions`, the per-tool descriptions, and the
> read-only `guide` tool (D9–D11), and **(3)** the **chassis adoption** that
> makes crm the suite's reference service shape — web assets served from the
> on-disk `share/www` root through `appkit/web`, the MCP transport and standard
> tools supplied by `appkit/mcp`, and the local shims deleted (D12–D14). crm's
> CRM **domain behavior** — the five entities, the verb semantics, validation,
> the outbox producer, the migrations — is owned elsewhere (`crm/CLAUDE.md`) and
> is **unchanged** by all three threads. No schema changes on any surface: this
> design adds **no migration**.

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

- **Language / toolchain:** Go **1.26**, single module `module crm` rooted at
  `crm/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo).
- **Build / typecheck command:** `cd crm && go build ./...` and
  `cd crm && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship crm`).
- **Test command:** `cd crm && go test ./...`. **"The suite is green"** means:
  `cd crm && go build ./...`, `cd crm && go vet ./...`, `cd crm && gofmt -l .`
  (no output), and `cd crm && go test ./...` all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`, `replace registry => ../registry`). The
  web and MCP surfaces add **no third-party dependency** — they use the standard
  library and the appkit chassis (`appkit/web`, `appkit/mcp`). Since D15 crm
  resolves its own loopback port by name through `registry`
  (`registry.MustPort("crm")`) instead of a bare literal.
- **The chassis owns the server — and, since D12/D13, the web mechanism and the
  MCP transport.** crm is `appkit.Main(appkit.Spec{…})`: `App:"crm"`,
  `Mount:"/srv/crm/"`, `Port:registry.MustPort("crm")` (== `3100`; D15),
  `MCP:true`, `Feed:"/feed"` (event-plane producer), `WWW:true` (web assets from
  the on-disk www root). The fixed verbs
  (`serve`/`version`/`manifest`/`migrate`/`schema`), config-from-env, the
  loopback HTTP server + PRM + identity gate, the `/feed` mount, the
  **auto-mounted `GET /static/`**, and the JSON-RPC `/mcp` plumbing with the
  standard `health`/`reflection` tools are appkit's. main.go declares crm's
  identity (the Spec) and wires its surface through the Spec hooks: the landing
  route and the gated `/mcp` mount live in **`Spec.Handlers`**.
- **Web assets are release artifacts on disk, not embedded.** crm's page
  template and static assets live in the source tree at `crm/share/www/`
  (`landing.html` + `static/tokens.css` + `static/fonts/*.woff2`), shipped by
  `bin/ship` into the versioned `share/<version>` tier and read on the box at
  `share/current/www` (appkit D5–D7 own the resolution and mechanism). The only
  `//go:embed` surfaces left in crm are the migrations (`internal/db`) and the
  MCP guide document (`internal/mcp/guide.md`) — agent-facing content, not web
  assets.
- **nginx is the sole trust boundary.** crm runs no token logic. nginx
  introspects every `/srv/crm/` request against the dashboard and forwards to the
  loopback service. The landing page's gate is therefore an **nginx** concern
  (the `crm/etc/nginx.conf` fragment), not a Go concern: the Go handlers are
  mounted **ungated in-process**, exactly as `POST /mcp` relies on nginx for its
  bearer gate. crm binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; `/mcp` is the **bearer-gated agent**
  door.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing page is tested over the real `crm/share/www` tree.** The page
  handler is a small `http.Handler` at the composition root that renders
  `landing.html` through the chassis-loaded site (`rt.WWW()`); its tests build
  an `appkit/web` Site from the repo-real `share/www` directory (a relative path
  from the test's package dir), drive it with `httptest.NewRequest` /
  `httptest.NewRecorder`, and assert status, body substrings (name, version),
  and `Content-Type`. **No test makes a network call and no test needs a running
  suite** — the handler is pure over its two string inputs and the on-disk
  template, and the disk is the real substrate (the same files `bin/ship`
  bundles).
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against a mux configured the way the composition root configures it,
  asserting that the bare root path is served by the landing handler while a
  non-root path is **not** captured by `{$}` (Go 1.22+ pattern semantics: `{$}`
  matches only the exact path).
- **Shipped assets are real bytes on disk.** The Carbon `tokens.css` and the
  woff2 fonts live under `crm/share/www/static/` and are served by the chassis
  static mount; tests drive that mount over the real directory and assert
  `tokens.css` is served with a CSS content type and that the template
  references the app's **own** asset path (not a cross-service URL).
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test that reads
  `crm/etc/nginx.conf` from disk and asserts the exact-match `= /srv/crm/`
  location exists, uses `auth_request /_session-authn` (not `/_authn`), and
  proxies to the loopback upstream root — while the pre-existing bearer-gated
  `/srv/crm/` prefix, the `= /srv/crm/feed` 404, and the PRM well-known location
  remain. This is a genuine assertion over the shipped artifact, runnable in the
  same `go test ./...`.
- **Determinism.** The landing handler takes its name/version as plain string
  arguments (injected at the composition root from
  `rt.Service()`/`rt.Version()`), so its output is fully determined by its
  inputs and the template file — no clock, no network, no DB.
- **The MCP surface is tested through the real JSON-RPC seam.** The service
  `instructions` (D9), the tool descriptors (D10), the `guide` tool (D11), and
  the tool table (D13) are proven by driving the `appkit/mcp` handler built from
  crm's declared `Options` (instructions + tool table over a real migrated DB)
  with `initialize`, `tools/list`, and `tools/call` requests — the
  `internal/mcp` `tools_test.go` harness, rewired at the same seam the
  composition root uses. The guide document stays embedded bytes (`//go:embed`),
  asserted as real content.

## Layout

The design is split for addressability so a build phase reads only the one
Decision it realizes:

- `project/design/README.md` — this spine: static cross-cutting facts only, no
  per-Decision detail.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded:
  `D01.md`, `D02.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map. It is the grep target for resolving
  an id.

**The web surface is content, not a package.** crm's pages and assets live at
`crm/share/www/` (templates + `static/`), rendered and served through the
chassis (`appkit/web` via `Spec.WWW`). There is **no** `crm/internal/web`
package: the only Go the web surface needs is the few-line landing handler at
the composition root. This is the seam every later crm web page grows from —
add a template under `share/www`, mount a route in `Handlers`, render with
`rt.WWW()`.

**`internal/mcp` is the tool table, not a transport.** The package declares
crm's domain tools (`search`/`get`/`save`/`delete`/`log`/`guide` — descriptors,
schemas, handlers over `crm.Service`) and the service `instructions`, consumed
by `appkit/mcp.New` at the composition root. The JSON-RPC plumbing and the
standard `health`/`reflection` tools are the chassis's (appkit D8–D9). It adds
no package and no migration; the embedded `guide.md` stays here.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
