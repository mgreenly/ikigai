# dashboard — Design (web pages restructure)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the dashboard's three-page web surface is built and
*how each behavior is proven*. The product (`project/product/product.md`) owns the
*why*, *for whom*, and the user-facing promises; design states the **exact,
checkable form** of those promises and never re-declares the why. This design is
scoped to splitting the single hybrid apex page into a login page, a landing/home
page, and a new profile page (D1–D6), plus a diminished name-origin colophon on
the login page (D7) — and is rewritten in place to stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is **no
  separate requirements document**.
- Design's responsibility for ids ends at minting them into this doc. How coverage
  is measured and when the work is "done" are **not** design's concern — downstream
  phases own that.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain:** Go **1.26**, single module `module dashboard` rooted
  at `dashboard/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo). `appkit`
  and `eventplane` are committed in-repo replace-siblings (`replace appkit =>
  ../appkit`, `replace eventplane => ../eventplane`).
- **This change touches no schema.** It is a pure HTTP-routing + template + view
  change under `dashboard/internal/server/` and `dashboard/ui/`. No migration is
  written; `bin/check-migrations dashboard` must still pass (no duplicate/edited
  migrations).
- **Build / typecheck command:** `cd dashboard && go build ./...` and
  `go vet ./...`. The production build adds `CGO_ENABLED=0 GOOS=linux GOARCH=amd64
  GOWORK=off` (driven by `bin/ship`).
- **Test command:** `cd dashboard && go test ./...`. **"The suite is green"**
  means: `cd dashboard && go build ./...`, `go vet ./...`, `gofmt -l .` (no
  output), `go test ./...`, and `bin/check-migrations dashboard` all succeed with
  zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Server package:** the dashboard's whole apex route table is registered in
  `dashboard/internal/server/routes.go` (`(*app).register`), built over `*app`
  (fields in `server.go`). Templates are parsed once at startup via
  `template.ParseFS(ui.Files, …)` in `server.go`; a broken template fails startup,
  not a request. Static assets and the Carbon `tokens.css`/fonts are already
  embedded under `dashboard/ui/static/` and served at `/static/`.

## The apex is the exception — it gates its own session in-process

The suite-wide landing-page change cookie-gates each **path-routed** service's
landing page at nginx via the dashboard-owned `/_session-authn` `auth_request`.
**The dashboard is not gated that way.** It is the apex/`DEFAULT=true` app behind
appkit's Apex bypass: it **owns and issues** the `dashboard_session` cookie and
runs **no** `auth_request` in front of its own routes. So every session decision
for the dashboard's own pages is made **in-process**, exactly as the existing
index does — by reading the `dashboard_session` cookie and looking it up in the
web-session store (`a.sessions.Lookup`, surfaced through `(*app).sessionOwner` /
`(*app).requireSession`). The new profile page reuses that same in-process seam;
it does **not** introduce an nginx fragment, and `/_session-authn` (which other
services consume) is untouched by this change.

## The three pages and the routes that serve them

| Page | Route | Audience | Gate |
|---|---|---|---|
| **Login** | `GET /` with no/invalid session | logged-out human | none (public) |
| **Landing / home** | `GET /` with a valid session | logged-in owner | session (in-process branch) |
| **Profile** | `GET /profile` | logged-in owner | session (in-process, redirect if absent) |

`GET /` stays **one** handler that branches on the session (login vs landing) —
its split is by *audience on the same route*, the behavior that exists today.
The profile page is a **new route** with a **new handler** and its own template.

## Testing strategy

Testing is part of the architecture. The cross-cutting approach every Decision's
Verification list assumes:

- **HTTP-level tests against the standalone server.** The in-package tests drive
  the real route table via `(*app).routes()` (the `server` package's existing test
  harness — see `internal/server/index_test.go`, `grants_test.go`,
  `login_test.go`), issuing requests with `httptest` and asserting status codes,
  `Location` headers, and rendered HTML. New tests are co-located in
  `internal/server/*_test.go`, `package server`, named for the behavior asserted.
- **Session is a real store on a temp DB; identity is injected.** The web-session
  store runs against a real temp `modernc.org/sqlite` migrated by the appkit
  runner, exactly as the existing server tests construct it. A request is "signed
  in" by minting a session and presenting its cookie; "signed out" by omitting it.
  No live network, no real Google IdP.
- **Render assertions, not screenshot diffs.** "The landing has no PAT form" is
  proven by asserting the rendered `GET /` body omits the PAT-create markup;
  "profile renders the grants block" by asserting the `GET /profile` body contains
  it. Redirect targets (`/profile` vs `/`) are proven by asserting the `Location`
  header on the 3xx response.
- **The doc-truth phase is grep-checkable.** The AGENTS.md purge (D6) is verified
  by asserting the stale phrase is gone and the three-page truth is present — a
  text check, not a Go test.

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

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated.
Existing `R-XXXX-XXXX` ids are stable handles — never renumbered; a newly added
behavior gets a freshly minted id, and a removed behavior's id is deleted with it.
