# cron — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* cron's web landing page is built, **how cron
conforms to the appkit chassis reference shape** (the `share/www` web surface,
the `appkit/mcp` tool table, `registry` ports, the inline composition root, the
`internal/db` shim deletion), **and how cron conforms to the suite's revised
event routing** (`docs/event-routing-design.md`: kind `tick` + subject
`/<schedule name>` replacing the packed `cron.<name>` type — D14), **and how
cron conforms to the suite's structured-MCP result contract**
(`docs/structured-mcp-design.md`: `structuredContent` + mirrored text via
`StructuredResult`, a declared `outputSchema` per domain tool, and
closed-vocabulary error codes — D15) — and *how each behavior is proven*. The product (`project/product/README.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the mount root only;
v1 content is service name + version; the gate is `/_session-authn` and coarse;
the visual system is Carbon) but does **not** own them. This is the single,
current statement of the landing-page architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design covers cron's web landing page **and cron's conformance
> to the appkit chassis reference shape** — the on-disk `share/www` web surface
> (D9), the `appkit/mcp` tool-table MCP surface (D10), `registry`-resolved
> ports (D11), the inline composition root in `cmd/cron/main.go` (D8), and the
> `internal/db` shim deletion + doctrine truth-up (D12) — **plus the
> event-routing conformance (D14)**: the tick event's address becomes kind
> `tick` + subject `/<schedule name>` (canonical key `cron:tick/<name>`), the
> LIVE `Publishes` provider becomes a one-family registry with a live schedule
> enumeration, and the outbox table converts to the revised `outbox.SchemaSQL`
> by **one new timestamped migration** (the sole schema change in this design;
> the chassis conversion itself added none). The crontab scheduling **domain
> behavior** — the CRUD tool semantics, the minute-aligned tick worker's
> at-most-once (schedule, slot) firing, and the `{name, scheduled_for,
> fired_at}` payload — is **unchanged**: the conversion and the routing
> revision move *how events are addressed and wired*, never *what fires when*.

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

- **Language / toolchain:** Go **1.26**, single module `module cron` rooted at
  `cron/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). The landing page
  itself touches no SQLite, but the module/build facts are unchanged.
- **Build / typecheck command:** `cd cron && go build ./...` and
  `cd cron && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship cron`).
- **Test command:** `cd cron && go test ./...`. **"The suite is green"** means:
  `cd cron && go build ./...`, `cd cron && go vet ./...`, `cd cron && gofmt -l .`
  (no output), and `cd cron && go test ./...` all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`, `replace registry => ../registry` — the
  last added by D11). The web surface adds **no third-party dependency** — the
  landing page renders through the chassis `appkit/web` site, the MCP surface
  assembles through `appkit/mcp`, and the port resolves through `registry`; the
  service code uses only the standard library plus the appkit/eventplane/registry
  siblings.
- **The chassis owns the server.** cron is `appkit.Main(cronSpec())`, where
  `cronSpec()` is a `func cronSpec() appkit.Spec` declared **inline in
  `cmd/cron/main.go`** (D8 — there is no `internal/cronapp` package):
  `App:"cron"`, `Mount:"/srv/cron/"`, `Port: registry.MustPort("cron")` (D11),
  `MCP:true`, `WWW:true` (D9), `Feed:"/feed"` (event-plane producer). The fixed
  verbs (`serve`/`version`/`manifest`/`migrate`/`schema`), config-from-env, the
  loopback HTTP server + PRM + identity gate, the `Spec.WWW` site load + auto
  `GET /static/` mount, and the `/feed` producer mount are appkit's. `main.go`
  declares cron's identity (the Spec) and wires its surface through the Spec hooks
  (the crontab `Store`, the assembled `appkit/mcp` `POST /mcp` handler, the LIVE
  `Publishes` provider, and the tick `Producer`/`Workers`). The landing route is
  wired through the **`Spec.Handlers`** hook, rendering `share/www/landing.html`
  through `rt.WWW()`, beside the `POST /mcp` mount.
- **nginx is the sole trust boundary.** cron runs no token logic. nginx
  introspects every `/srv/cron/` request against the dashboard and forwards to the
  loopback service. The landing page's gate is therefore an **nginx** concern
  (the `cron/etc/nginx.conf` fragment), not a Go concern: the Go handler is mounted
  **ungated in-process**, exactly as `POST /mcp` relies on nginx for its bearer
  gate. cron binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; the existing `/mcp` is the
  **bearer-gated agent** door, unchanged.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing handler is tested over the repo-real `share/www` tree.** The
  composition-root handler renders `landing.html` through an `appkit/web` Site
  loaded from `cron/share/www` (resolved relative to the `cmd/cron` test package,
  so tests exercise the exact files that ship). Its tests build the Site over the
  real tree, drive the handler with `httptest.NewRequest` / `httptest.NewRecorder`
  over fixed name/version values, and assert status, body substrings (name,
  version), and `Content-Type`. **No test makes a network call and no test needs a
  running suite** — the handler is pure over its two string inputs and the loaded
  on-disk site (D9's `R-LPMQ-FKBR`).
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it, asserting that the bare root path is served by the landing
  handler while a non-root path under the mux is **not** captured by `{$}`
  (Go 1.22+ pattern semantics: `{$}` matches only the exact path).
- **On-disk assets are real bytes.** The Carbon `tokens.css` and the woff2 fonts
  live on disk in `cron/share/www/static/` and are served by the **chassis** static
  mount (`Spec.WWW`); tests configure the chassis static mount over the real
  `share/www` tree and assert `tokens.css` is served with a CSS content type and
  the fonts with `font/woff2` real `wOF2` payloads, and that the template
  references the app's **own** service-local asset path (not a cross-service URL)
  (D9's `R-LQUM-TC2G`).
- **The MCP surface is tested over the assembled `appkit/mcp` handler.** The MCP
  tests build the handler `mcp.NewHandler(store, rt)` produces (over a crontab
  store on a temp DB, threading the live `Publishes` provider through a real
  `server.Router`) and drive `tools/list` and `tools/call`, asserting the
  exactly-seven-tool partition and the crontab behaviors (expr validation, CRUD
  round-trip, live tick-family reflection per D14) unchanged (D10's
  `R-LS2J-73T5`).
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test (relocated to
  `cmd/cron` by D9) that reads `cron/etc/nginx.conf` from disk and asserts the
  exact-match `= /srv/cron/` location exists, uses `auth_request /_session-authn`
  (not `/_authn`), and proxies to the loopback upstream root — while the
  pre-existing bearer-gated `/srv/cron/` prefix, the `= /srv/cron/feed` 404, and
  the PRM well-known location remain. Its `proxy_pass` origins are derived from
  `registry.BaseURL("cron")` (D11), so a registry renumber fails the test. This is
  a genuine assertion over the shipped artifact, runnable in the same
  `go test ./...`.
- **Determinism.** The landing handler takes its name/version as plain string
  arguments (injected at the composition root from `rt.Service()`/`rt.Version()`),
  so its output is fully determined by its inputs and the on-disk template — no
  clock, no network, no DB.

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

**Package layout (post-conversion).** cron's web surface is **not** a Go package:
the landing template and Carbon assets live on disk under `cron/share/www/`
(`landing.html` + `static/tokens.css` + the woff2 fonts), served through the
chassis `appkit/web` mechanism (D9); the former `cron/internal/web/` package is
**deleted**. The composition root — the `appkit.Spec` and the `landingHandler`
render helper — lives **inline in `cmd/cron/main.go`** (D8; there is no
`internal/cronapp`). `cron/internal/mcp` is the MCP **tool table**
(`Instructions` + `Tools(store)` + `NewHandler`) over the shared `appkit/mcp`
transport (D10), not a hand-rolled JSON-RPC handler. `cron/internal/db` holds
**only** the embedded migration set (`FS`) and its app-side guard tests — appkit
owns DB opening and the migration run (D12); D14 adds one new timestamped
outbox migration beside the frozen originals and re-points the DDL drift guard
at it. The domain packages `internal/crontab`, `internal/cron`, and
`internal/tick` keep their shapes; `internal/event` carries D14's revised
event contract (kind `tick`, `Subject(name)`, the one-family live `Publishes`
provider).

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
