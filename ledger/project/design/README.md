# ledger — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the ledger landing page is built and *how each
behavior is proven*. The product (`project/product/README.md`) owns the *why*,
*for whom*, and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants by value (the page lives at the mount root only;
v1 content is service name + version; the gate is `/_session-authn` and coarse;
the visual system is Carbon) but does **not** own them. This is the single,
current statement of the landing-page architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design's Decisions cover two threads:
>
> 1. **The web landing page** (D1–D8, built; substrate moved by D10): the page's
>    content, route, session gate, canonical markup, and self-served assets — now
>    rendered from the on-disk `share/www` tree through the chassis.
> 2. **The chassis conversion** (D9–D12): ledger resolves its **own** loopback
>    port by name through `registry` with source-scan and deploy-artifact drift
>    guards (D9); the web surface serves from `share/www` through `Spec.WWW` (D10);
>    the MCP surface serves the seven domain tools over `appkit/mcp`, the transport
>    and the `health`/`reflection` tools chassis-owned (D11); and the leftover
>    `internal/db` shims are deleted with the doctrine doc trued up (D12) — all
>    behavior-preserving. appkit's `Spec.WWW`/`appkit/web`/`appkit/mcp` surfaces
>    (appkit design D5–D10) are fixed external contracts consumed through the
>    committed `replace appkit => ../appkit`.
>
> The existing ledger bookkeeping **domain** (the immutable journal, the domain
> tool bodies, the outbox producer, the migration *contents*) is owned elsewhere
> (`ledger/CLAUDE.md`) and is untouched. **No schema changes: no Decision here adds
> a migration.**

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
  `cd ledger && gofmt -l .` (no output), and `cd ledger && go test ./...` all
  succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed in-repo
  replace-siblings (`replace appkit => ../appkit`,
  `replace eventplane => ../eventplane`, `replace registry => ../registry`; D9).
  The repo-root `go.work` and the sibling modules themselves are external
  preconditions owned outside `ledger/`.
- **The chassis owns the server.** ledger is `appkit.Main(appkit.Spec{…})`:
  `App:"ledger"`, `Mount:"/srv/ledger/"`, `Port:registry.MustPort("ledger")`
  (== `3101`; D9), `MCP:true`, `WWW:true` (D10), and `Feed:"/feed"` (event-plane
  producer). The fixed verbs (`serve`/`version`/`manifest`/`migrate`/`schema`),
  config-from-env, the loopback HTTP server + PRM + identity gate, the www-site
  load + static mount, the MCP transport with the standard `health`/`reflection`
  tools, and the `/feed` mount are appkit's. main.go declares ledger's identity
  (the Spec) and wires its surface through the Spec hooks; the landing route is
  wired through the existing **`Spec.Handlers`** hook, beside the `POST /mcp`
  mount.
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

- **The landing page is tested over the shipped tree.** Tests in `cmd/ledger`
  load the repo-real `ledger/share/www` via `appkit/web` (a relative path from the
  package dir) and drive the landing handler and the chassis static mount with
  `net/http/httptest`, asserting status, body substrings (name, version, asset
  links), and content types. The files under test are the exact files that ship.
  **No test makes a network call and no test needs a running suite** (D10).
- **The route mux is tested as wired.** The `GET /{$}` exact-root pattern is proven
  against an `http.ServeMux` configured the way the composition root configures it,
  asserting the bare root is served while a non-root path is **not** captured by
  `{$}` (Go 1.22+ semantics: `{$}` matches only the exact path).
- **The MCP surface is tested through the assembled chassis handler.** The
  `internal/mcp` / `cmd/ledger` tests build `NewHandler(svc, rt)` over a migrated
  in-memory `ledger.Service` and drive `tools/list`/`tools/call` through the real
  `appkit/mcp` `ServeHTTP` seam, keeping the pre-conversion behavioral assertions
  (the seven domain verbs' success/error envelopes, account/period normalization,
  the reflection index, the health envelope + identity) unchanged (D11).
- **The nginx fragment is proven by content assertion.** Tests read
  `ledger/etc/nginx.conf` from disk and assert the exact-match `= /srv/ledger/`
  session-gated location, the session-gated `/srv/ledger/static/` location, and
  **registry-derived** proxy targets (`registry.BaseURL("ledger")`, D9) — while the
  pre-existing bearer-gated `/srv/ledger/` prefix, its `@ledger_authn_500` re-emit,
  and the PRM well-known location remain. A genuine assertion over the shipped
  artifact, runnable in the same `go test ./...`.
- **Determinism.** The landing handler takes name/version as plain strings and the
  MCP handler runs over an in-memory DB; no clock, no network in the web/MCP tests.

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

**Package shape after D9–D12.** ledger carries `internal/ledger` (the immutable
double-entry domain), `internal/ids` (ULID generation), `internal/db` (the embedded
migration set `FS` plus the load + outbox-DDL byte-equality guard tests only — DB
open and the migration runner are `appkit/db`, D12), and `internal/mcp` (the seven
domain tools' declaration — `Instructions` + `Tools(svc)` + `NewHandler` — over the
`appkit/mcp` transport, D11). There is **no** `internal/web` (deleted by D10 — the
page template and assets live in `share/www/`, the mechanism in `appkit/web`) and
**no** `internal/server`/`internal/logging` (routing, the PRM document, the identity
gate, `/feed`, security headers, graceful shutdown, and structured slog are all
appkit's). The composition root (`cmd/ledger/main.go`) is the Spec plus the landing
handler; the outbox is injected into the domain Service through the `Producer` hook.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
