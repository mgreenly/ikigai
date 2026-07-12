# notify — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* notify's ralph-governed surfaces are built and
*how each behavior is proven*. The product (`project/product/README.md`) owns
the *why*, *for whom*, and the user-facing promises; design states the **exact,
checkable form** of those promises and never re-declares the why. Design *uses*
the product's contractual constants by value (the page lives at the mount root
only; v1 content is service name + version; the gate is `/_session-authn` and
coarse; the visual system is Carbon) but does **not** own them. This is the
single, current statement of the architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design's Decisions cover three threads:
>
> 1. **The web landing page** (D1–D8, built; substrate moved by D12): the
>    page's content, route, session gate, canonical markup, and self-served
>    assets — now rendered from the on-disk `share/www` tree through the
>    chassis.
> 2. **Registry adoption** (D9–D10, built; narrowed by D11): notify resolves
>    its **own** loopback port by name, with source-scan and deploy-artifact
>    drift guards. Peer feed addresses are chassis-resolved.
> 3. **The chassis conversion** (D11–D14, built): consumer loops declared
>    through `Spec.Consumers`, the web surface through `Spec.WWW`, the MCP
>    surface through `appkit/mcp`, and the leftover chassis shims deleted —
>    all behavior-preserving. appkit's `Spec.Consumers`/`Spec.WWW`/`appkit/mcp`
>    surfaces (appkit design D5–D10) are fixed external contracts consumed
>    through the committed `replace appkit => ../appkit`.
> 4. **Event-routing conformance, consumer side** (D16, active): notify's
>    handlers and declared subscriptions adopt the suite's revised event
>    addressing (`docs/event-routing-design.md`) — `consumer.Event{Kind,
>    Subject}` + `Key()` replace the deleted `Type`, subscription filters
>    become canonical-key globs matched by `eventplane/routing.Match`, and the
>    reflection `subscribes` surface restates the new keys. notify produces
>    nothing, so there is no outbox and no migration. The revised eventplane
>    (its design D1–D4) and the conformed appkit chassis are fixed external
>    contracts, operator-sequenced ahead of the build.
>
> The rest of the notify domain (the ntfy push mechanics —
> `Client`/`Publish`/`Send` — and the event-plane wire contract itself) is
> owned elsewhere (`notify/CLAUDE.md`, the event-protocol docs / the
> eventplane spec) and is untouched. **No schema changes: no Decision here
> adds a migration** (D16 explicitly confirms none is needed).

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

- **Language / toolchain:** Go **1.26**, single module `module notify` rooted at
  `notify/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo).
- **Build / typecheck command:** `cd notify && go build ./...` and
  `cd notify && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship notify`).
- **Test command:** `cd notify && go test ./...`. **"The suite is green"** means:
  `cd notify && go build ./...`, `cd notify && go vet ./...`,
  `cd notify && gofmt -l .` (no output), and `cd notify && go test ./...`
  all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed
  in-repo replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`, `replace registry => ../registry`).
  The repo-root `go.work` and the sibling modules themselves are external
  preconditions owned outside `notify/`.
- **The chassis owns the server.** notify is `appkit.Main(appkit.Spec{…})`:
  `App:"notify"`, `Mount:"/srv/notify/"`, `Port:registry.MustPort("notify")`
  (== `3201`; D9), `MCP:true`, `WWW:true` (D12), and
  `Consumers:[…crm…, …prompts…]` (D11 — the declared event-plane consumer role;
  notify serves **no** `/feed` of its own). The fixed verbs
  (`serve`/`version`/`manifest`/`migrate`/`schema`), config-from-env, the
  loopback HTTP server + PRM + identity gate, the www-site load + static
  mount, the MCP transport with the standard `health`/`reflection` tools, and
  the consumer loops are appkit's. main.go declares notify's identity (the
  Spec) and wires its surface through the Spec hooks; the landing route is
  wired through **`Spec.Handlers`**, beside the `POST /mcp` mount.
- **nginx is the sole trust boundary.** notify runs no token logic. nginx
  introspects every `/srv/notify/` request against the dashboard and forwards to
  the loopback service. The landing page's gate is therefore an **nginx** concern
  (the `notify/etc/nginx.conf` fragment), not a Go concern: the Go handler is
  mounted **ungated in-process**, exactly as `POST /mcp` relies on nginx for its
  bearer gate. notify binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; `/mcp` is the **bearer-gated agent**
  door.
- **Cross-module collaborators (outside `notify/`).** The repo-root `bin/start`
  is not Go and not under `notify/`; where a Decision names it (D11's env-name
  migration, D12's `NOTIFY_WWW_PATH` export) it is a boundary-crossing
  collaborator verified by the live `bin/start` smoke, **not** by the notify Go
  suite. Phases that touch it are called out explicitly in the plan.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing page is tested over the shipped tree.** Tests in `cmd/notify`
  load the repo-real `notify/share/www` via `appkit/web` (a relative path from
  the package dir) and drive the landing handler and the chassis static mount
  with `net/http/httptest`, asserting status, body substrings (name, version,
  asset links), and content types. The files under test are the exact files
  that ship. **No test makes a network call and no test needs a running
  suite.**
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it (Go 1.22+ semantics: `{$}` matches only the exact path).
- **The MCP surface is tested through the assembled chassis handler.** The
  `internal/mcp` tests build `NewHandler` over a mock-ntfy `push.Client` and
  drive `tools/list`/`tools/call` through the real appkit/mcp `ServeHTTP` seam,
  keeping the pre-conversion behavioral assertions (header mapping, validation
  rejections, no secret leakage).
- **The consumer declaration is tested at the Spec and handler seams.** The
  Spec's `Consumers` table is asserted directly (sources, order, subscription
  lists); each handler factory is invoked with a Router and its handler driven
  with real event values against a mock ntfy server (the existing push-test
  substrate). The loop engine itself (SSE, cursors, reconnect) is appkit's
  proof (appkit D10), not re-proven here.
- **Event matching runs the real matcher and the real plane.** Routing
  conformance claims (D16) exercise the real `eventplane/routing` matcher —
  never a reimplementation — and the end-to-end claim chains the real revised
  outbox + `FeedHandler` + `consumer.Run` over `httptest` to the mock ntfy
  server (the substrate the existing e2e push test already uses).
- **The nginx fragment is proven by content assertion.** Tests read
  `notify/etc/nginx.conf` from disk and assert the session-gated locations and
  registry-derived proxy targets (D4, D8, D10) — a genuine assertion over the
  shipped artifact, runnable in the same `go test ./...`.
- **Determinism.** Handlers take name/version as plain strings and clients take
  injected config; no clock, no network, no DB in the web/MCP tests.

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

**Package shape after D11–D14.** notify carries `internal/push` (the ntfy
domain), `internal/db` (embedded migrations + feed-offset guard tests only),
and `internal/mcp` (the `send` tool table over `appkit/mcp`). There is **no**
`internal/web` (deleted by D12 — the page template and assets live in
`share/www/`, the mechanism in `appkit/web`) and no hand-rolled consumer
wiring (D11 — the loops are chassis-run). The composition root
(`cmd/notify/main.go`) is the Spec plus the landing handler and the ntfy config
resolution.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
