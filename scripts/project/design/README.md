# scripts — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the scripts landing page is built and *how each
behavior is proven*. The product (`project/product/README.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the mount root only;
v1 content is service name + version; the gate is `/_session-authn` and coarse;
the visual system is Carbon) but does **not** own them. This is the single,
current statement of the landing-page architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design began as scripts's web **landing page** and the seam it
> establishes (D1–D8), and grew two composition-root Decisions orthogonal to the
> landing series — **D9** (relocate the rebuildable `runs/` tree) and **D10** (adopt
> the shared `registry` for loopback-port resolution). It now also carries the
> **appkit-chassis conversion** (D11–D14): the event-plane consumer loops move to the
> chassis `Spec.Consumers` table (**D11**, which also normalizes the composition
> root), the web surface de-embeds to `share/www` through `Spec.WWW` (**D12**), the
> MCP surface moves onto the `appkit/mcp` transport as a domain tool table (**D13**),
> and the `internal/db` open/migrate shim is deleted (**D14**). Those conversions are
> **behavior-preserving** except where D13 states otherwise (it adds the chassis
> `reflection` tool scripts never exposed and surfaces the runtime contract on HTTP
> `/health`). The `python3`-exec runner, the script/run data model, the event
> semantics (`internal/consume` fan-out, the producer outbox), and the migrations
> themselves are unchanged. No schema changes: none of these Decisions add a
> migration.

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

- **Language / toolchain:** Go **1.26**, single module `module scripts` rooted at
  `scripts/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). The landing
  page itself touches no SQLite, but the module/build facts are unchanged.
- **Build / typecheck command:** `cd scripts && go build ./...` and
  `cd scripts && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship scripts`).
- **Test command:** `cd scripts && go test ./...`. **"The suite is green"** means:
  `cd scripts && go build ./...`, `cd scripts && go vet ./...`,
  `cd scripts && gofmt -l .` (no output), and `cd scripts && go test ./...`
  all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit` and `eventplane` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`). The landing page adds **no new
  dependency** — it uses only the standard library (`net/http`, `embed`,
  `html/template` or `text/template`) and the appkit chassis. **D10** adds a third
  such replace-sibling, `registry` (`replace registry => ../registry`, a
  standalone zero-dependency module at the repo root), consumed only at the
  composition root. The matching repo-root `go.work use ./registry` entry is
  **external, repo-root-owned wiring** (like the existing `eventplane` entry) and
  is a precondition, not built here.
- **The chassis owns the server.** scripts is `appkit.Main(scriptsSpec())`, where
  `scriptsSpec()` returns **one fully-formed `appkit.Spec` literal** (no
  post-construction mutation, per **D11**'s composition-root normalization):
  `App:"scripts"`, `Mount:"/srv/scripts/"`, `Port: registry.MustPort("scripts")`
  (== `3003`, resolved by name per **D10**; was a `3003` literal), `MCP:true`,
  `WWW:true` (web surface from `share/www` through the chassis, **D12**),
  `Feed:"/feed"` (event-plane **producer** of `scripts.succeeded`/`scripts.failed`),
  the `Consumers` table (five upstream entries — `cron`/`crm`/`ledger`/`dropbox`/
  `prompts` — chassis-owned per **D11**, replacing the legacy `Consumes` +
  `Subscriptions` fields and the hand-rolled consumer `Workers`), `Health` (the
  static runtime-contract reporter, feeding both HTTP `/health` and the MCP `health`
  tool, **D13**), and the `Producer` outbox hook. The fixed verbs
  (`serve`/`version`/`manifest`/`migrate`/`schema`), config-from-env, the loopback
  HTTP server + PRM + identity gate, DB open + migration run, the `/feed` mount, the
  `GET /static/` mount, and the `consumer.Run` loops are appkit's. main.go declares
  scripts's identity (the Spec) and wires its domain surface through the Spec hooks.
  The landing route and `POST /mcp` (assembled via `appkit/mcp`, **D13**) are wired
  through the **`Spec.Handlers`** hook (`registerRoutes`).
- **nginx is the sole trust boundary.** scripts runs no token logic. nginx
  introspects every `/srv/scripts/` request against the dashboard and forwards to
  the loopback service. The landing page's gate is therefore an **nginx** concern
  (the `scripts/etc/nginx.conf` fragment), not a Go concern: the Go handler is
  mounted **ungated in-process**, exactly as `POST /mcp` relies on nginx for its
  bearer gate. scripts binds `127.0.0.1` only.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients are gated by an opaque bearer (`auth_request /_authn`). The landing
  page is the **cookie-gated human** door; the existing `/mcp` is the
  **bearer-gated agent** door, unchanged. The event-plane `/feed` is loopback-only
  and never reachable through nginx — neither door touches it.

## Testing strategy

Testing is part of the architecture, not an afterthought. The cross-cutting
approach every Decision's Verification list assumes:

- **The landing page is tested in-process with `net/http/httptest`** over the
  shipped `scripts/share/www` tree (D12): the tests live in `cmd/scripts`, load the
  repo-real tree via `appkit/web` (a relative path from the package dir), render
  `landing.html` with fixed service/version values, and assert status, body
  substrings (name, version), and `Content-Type`. **No test makes a network call and
  no test needs a running suite** — the page is a pure function of its two string
  inputs and the shipped template.
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is
  proven against an `http.ServeMux` configured the way the composition root
  configures it, asserting that the bare root path is served by the landing
  handler while a non-root path under the mux is **not** captured by `{$}`
  (Go 1.22+ pattern semantics: `{$}` matches only the exact path).
- **Shipped assets are real bytes.** The Carbon `tokens.css` and the woff2 fonts
  ship under `share/www/static` and are served by the chassis static mount (D12);
  tests assert `tokens.css` is served with a CSS content type and that the template
  references the app's **own** `/static/` asset path (not a cross-service URL).
- **The nginx fragment is proven by content assertion.** The session-gate
  fragment is config, not Go, so its behavior is pinned by a test that reads
  `scripts/etc/nginx.conf` from disk and asserts the exact-match `= /srv/scripts/`
  location exists, uses `auth_request /_session-authn` (not `/_authn`), and
  proxies to the loopback upstream root — while the pre-existing bearer-gated
  `/srv/scripts/` prefix, the `= /srv/scripts/feed` 404, and the PRM well-known
  location remain. This is a genuine assertion over the shipped artifact, runnable
  in the same `go test ./...`.
- **Determinism.** The handler takes its name/version as plain string arguments
  (injected at the composition root from `rt.Service()`/`rt.Version()`), so its
  output is fully determined by its inputs — no clock, no network, no DB.

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

**Web surface on disk.** The landing page and its Carbon assets live at
`scripts/share/www/` — `landing.html` + `static/` (`tokens.css` + the woff2 fonts)
— served by the chassis through `Spec.WWW` (**D12**); there is no `internal/web`
package (D12 deleted it, and its landing/asset/nginx tests moved to `cmd/scripts`).
The remaining domain packages are `internal/consume` (event-plane fan-out),
`internal/db` (the embedded migration set only — **D14** deleted its open/migrate
shim), `internal/ids`, `internal/mcp` (the domain tool table over `appkit/mcp` —
**D13**), `internal/runner`, and `internal/script`.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
