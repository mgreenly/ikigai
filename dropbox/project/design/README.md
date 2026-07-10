# dropbox — Design (landing page)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the dropbox landing page is built and *how each
behavior is proven*. The product (`project/product/README.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the mount root only;
v1 content is service name + version; the gate is `/_session-authn` and coarse;
the visual system is Carbon) but does **not** own them. This is the single,
current statement of the landing-page architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design's Decisions cover three threads:
>
> 1. **The web landing page** (D1–D8, built; substrate moved by D11): the page's
>    content, route, session gate, canonical markup, and self-served assets —
>    now rendered from the on-disk `share/www` tree through the chassis.
> 2. **Registry adoption** (D9–D10, active): dropbox resolves its **own**
>    loopback address by name (the listen port, the `/content` base default, and
>    the reflection example origin), with source-scan and deploy-artifact drift
>    guards.
> 3. **The chassis conversion** (D11–D13, active): the web surface through
>    `Spec.WWW`, the MCP surface through `appkit/mcp`, and the leftover
>    `internal/db` shim deleted — all behavior-preserving. appkit's
>    `Spec.WWW` / `appkit/web` / `appkit/mcp` surfaces (appkit design D5–D9) are
>    fixed external contracts consumed through the committed
>    `replace appkit => ../appkit`.
>
> The rest of the dropbox domain (the mirror-sync engine and its load-bearing
> correctness rules, the `/feed` outbox producer, the loopback `/content`/`/list`
> byte routes, the `list`/`get` domain-tool behaviors, the migrations) is owned
> elsewhere (`dropbox/CLAUDE.md`) and its behavior is untouched. **No schema
> changes: no Decision here adds a migration.**

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

- **Language / toolchain:** Go **1.26**, single module `module dropbox` rooted at
  `dropbox/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). The landing
  page itself touches no SQLite, but the module/build facts are unchanged.
- **Build / typecheck command:** `cd dropbox && go build ./...` and
  `cd dropbox && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship dropbox`).
- **Test command:** `cd dropbox && go test ./...`. **"The suite is green"**
  means: `cd dropbox && go build ./...`, `cd dropbox && go vet ./...`,
  `cd dropbox && gofmt -l .` (no output), and `cd dropbox && go test ./...` all
  succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`, `replace registry => ../registry`; the
  `registry` require+replace is added by D9). The repo-root `go.work` and the
  sibling modules themselves are external preconditions owned outside `dropbox/`.
  The web surface adds **no other** new dependency — the page mechanism is the
  chassis (`appkit/web`), the MCP transport is the chassis (`appkit/mcp`).
- **The chassis owns the server.** dropbox is `appkit.Main(appkit.Spec{…})`:
  `App:"dropbox"`, `Mount:"/srv/dropbox/"`, `Port:registry.MustPort("dropbox")`
  (== `3200`; D9), `MCP:true`, `WWW:true` (D11), `Feed:"/feed"` (event-plane
  producer), plus its `Migrations`, `Events`, `ManifestExtras`, a `Health`
  reporter, a `Producer` hook, and a `Workers` hook (the background sync engine).
  The fixed verbs (`serve`/`version`/`manifest`/`migrate`/`schema`),
  config-from-env, the loopback HTTP server + PRM + identity gate, the www-site
  load + static mount, the MCP transport with the standard `health`/`reflection`
  tools, and the `/feed` mount are appkit's. main.go declares dropbox's identity
  (the Spec) and wires its surface through the Spec hooks: the landing route
  (rendered via `rt.WWW()`) and the `POST /mcp` mount (assembled by
  `internal/mcp.NewHandler`) are wired through the **`Spec.Handlers`** hook,
  beside the loopback `GET /content` / `GET /list` byte mounts.
- **nginx is the sole trust boundary.** dropbox runs no token logic. nginx
  introspects every `/srv/dropbox/` request against the dashboard and forwards to
  the loopback service. The landing page's gate is therefore an **nginx** concern
  (the `dropbox/etc/nginx.conf` fragment), not a Go concern: the Go handler is
  mounted **ungated in-process**, exactly as `POST /mcp` relies on nginx for its
  bearer gate. dropbox binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; the existing `/mcp` is the
  **bearer-gated agent** door, unchanged. (The loopback `/content`/`/list` byte
  routes are a third, private-to-the-box door, self-guarded in-handler and
  unaffected by this work.)

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing page is tested over the shipped tree.** After D11, tests in
  `cmd/dropbox` load the repo-real `dropbox/share/www` via `appkit/web` (a
  relative path from the package dir) and drive the landing handler and the
  chassis static mount with `net/http/httptest`, asserting status, body
  substrings (name, version, asset links), and content types. The files under
  test are the exact files that ship. **No test makes a network call and no test
  needs a running suite.**
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it, asserting that the bare root path is served by the landing
  handler while a non-root path under the mux is **not** captured by `{$}`
  (Go 1.22+ pattern semantics: `{$}` matches only the exact path).
- **The MCP surface is tested through the assembled chassis handler.** After D12,
  the `internal/mcp` tests build `NewHandler(svc, rt)` over a domain
  `dropbox.Service` (through a `server.New`/`Register` seam, the crm/notify
  pattern) and drive `tools/list`/`tools/call` through the real `appkit/mcp`
  `ServeHTTP` seam, keeping the pre-conversion `list`/`get` behavioral assertions
  (path scoping, cursor pagination, the 25 MiB `too_large` cap, the rev-pin
  conflict, base64 bodies, the sentinel→code error envelope). The four-tool
  partition (`list`/`get` declared + chassis `health`/`reflection`) is asserted
  at the same seam.
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test that reads
  `dropbox/etc/nginx.conf` from disk and asserts the exact-match `= /srv/dropbox/`
  location exists, uses `auth_request /_session-authn` (not `/_authn`), and
  proxies to the **`registry`-derived** loopback upstream root
  (`registry.BaseURL("dropbox")`, per D10) — while the pre-existing bearer-gated
  `/srv/dropbox/` prefix, its `@dropbox_authn_500` re-emit, the
  `= /srv/dropbox/content` 404, and the PRM well-known location remain. This is a
  genuine assertion over the shipped artifact; after D11 it lives in `cmd/dropbox`.
- **Determinism.** The landing handler takes its name/version as plain string
  arguments (injected at the composition root from `rt.Service()`/`rt.Version()`)
  and the MCP tools take an injected Service, so their web/MCP tests have no
  clock, no network, and no external DB.

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

**Package shape after D9–D13.** dropbox carries `internal/dropbox` (the sync
engine, store, mirror, service, events, `/content`/`/list` handlers — untouched),
`internal/db` (embedded migrations + the load and `003_outbox.sql` byte-equality
guards only; the `Open`/`Migrate` wrappers deleted by D13), and `internal/mcp`
(the `list`+`get` domain-tool table over `appkit/mcp`, plus `Instructions` and
`NewHandler` — D12). There is **no** `internal/web` (deleted by D11 — the landing
template and `static/` assets live in `share/www/`, the mechanism in `appkit/web`).
The landing page and the woff2/token assets ship on disk under `dropbox/share/www/`,
loaded through `Spec.WWW`. The composition root (`cmd/dropbox/main.go`) is the
Spec plus the landing handler (over `rt.WWW()`), the `POST /mcp` mount (over
`internal/mcp.NewHandler`), the `/content`/`/list` mounts, and the sync-engine
wiring — with the listen port, content-base default, and reflection example
origin all resolved from `registry` (D9).

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
