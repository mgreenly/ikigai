# dashboard — Design (web pages restructure)

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the dashboard's three-page web surface is built and
*how each behavior is proven*. The product (`project/product/README.md`) owns the
*why*, *for whom*, and the user-facing promises; design states the **exact,
checkable form** of those promises and never re-declares the why. This design
covers two bodies of work in the dashboard: (1) the **web surface** — splitting
the single hybrid apex page into a login page, a landing/home page, and a new
profile page (D1–D6), a diminished name-origin colophon on the login page (D7),
the shared banner chrome (D10), and a new owner-only **telemetry** page that
samples box resource health in memory and graphs the last 24 hours (D11–D16);
and (2) the **identity model** — moving the dashboard's concept of user identity
from email to the OIDC subject pair `(iss, sub)` behind an opaque local handle,
capturing name/picture at login, and emitting them (plus the handle) as
additive identity headers from the introspection endpoints (D17–D19). It is
rewritten in place to stay true (stale decisions are removed, not stacked); the
history of how it got here lives in the plan.

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
- **Schema.** The web-surface work (D1–D16) touches **no** schema: it is a pure
  HTTP-routing + template + view change under `dashboard/internal/server/` and
  `dashboard/ui/`, plus one in-memory package `dashboard/internal/telemetry/`
  whose history lives only in RAM (ring buffers) and is never persisted. The
  **identity model** work (D17–D19) *does* add schema: two new forward-only
  migrations — a new `identities` table (D17) and an `owner_id TEXT` column on
  the four auth-artifact carrier tables (`web_sessions`, `oauth_authcodes`,
  `oauth_chains`, `personal_tokens`) (D18). Both are created with
  `bin/create-migration dashboard <name>` (timestamped, immutable) and applied
  by the appkit runner; committed migrations are never edited.
- **Telemetry collector runs on the appkit `Workers` seam.** `appkit.Spec.Workers`
  is `[]func(ctx context.Context) error`; each worker runs on the serve context and
  a `ctx` cancel (SIGTERM/shutdown) unwinds it. `cmd/dashboard/main.go` follows the
  established capture idiom (`var rt *appkit.Router`; the `Handlers` hook sets it
  and constructs shared collaborators; the `Workers` closure captures it, running
  strictly after Handlers so `rt.Logger()` is live) — the same pattern
  `notify`/`dropbox`/`prompts` use for their consumer loops. The one telemetry
  `Store` instance is constructed once at that composition root and shared between
  the collector worker (writer) and the `server` handlers (reader).
- **Linux metric sources are read through injected roots.** The collector reads
  `MemAvailable`/`MemTotal` from `/proc/meminfo`, free/total from `statfs` of the
  `/opt` filesystem, per-service memory from the cgroup-v2
  `<cgroupRoot>/system.slice/<svc>.service/memory.current`, and per-service disk
  from a directory walk of `/opt/<svc>`. Each path/root is a config field defaulted
  to the production value, so tests point them at fixtures/temp trees. On the box a
  missing per-service cgroup file or `/opt/<svc>` dir is a normal "unavailable"
  reading recorded as **0**; an *unexpected* read error is recorded as **0** and
  logged.
- **Build / typecheck command:** `cd dashboard && go build ./...` and
  `go vet ./...`. The production build adds `CGO_ENABLED=0 GOOS=linux GOARCH=amd64
  GOWORK=off` (driven by `bin/ship`).
- **Test command:** `cd dashboard && go test ./...`. **"The suite is green"**
  means: `cd dashboard && go build ./...`, `go vet ./...`, `gofmt -l .` (no
  output), and `go test ./...` all succeed with zero failures.
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
| **Telemetry** | `GET /telemetry` (+ `GET /telemetry/fragment`) | logged-in owner | session (in-process, redirect if absent) |

`GET /` stays **one** handler that branches on the session (login vs landing) —
its split is by *audience on the same route*, the behavior that exists today.
The profile and telemetry pages are each a **new route** with a **new handler**
and its own template; telemetry adds a second route (`/telemetry/fragment`) that
returns just its charts block for the once-a-minute client poll (D14).

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
- **Telemetry sources are unit-tested against fixtures; the two that hinge on a
  real OS contract get a real-substrate check.** The `/proc/meminfo` parser runs
  against a fixture reader; the cgroup reader and the `du` walk run against temp
  trees at an injected root (including the absent-path → 0 case); service discovery
  runs against a temp manifest root (asserting the dashboard, which lacks
  `MCP=true`, is excluded). Free-disk reads a **real** `statfs` — a claim that only
  the kernel can falsify — so its id drives the syscall against a real temp path,
  not a stub, and asserts a plausible non-zero total. The collector's lifecycle
  (immediate first sample, per-tick sampling, clean return on `ctx` cancel,
  error→0+log) is tested with fake sources and a short injected interval.
- **Identity is tested against a real temp DB; Google is injected.** The
  `internal/identity` store runs against a real temp `modernc.org/sqlite`
  migrated by the appkit runner — the substrate that actually enforces the
  `UNIQUE (iss, sub)` upsert and the schema (D17). The `ids.New()` handle source
  is injected so a test can assert the handle's provenance. The claim decode
  (D18 part A) drives `googleidp`'s existing token-construction test seam
  (`internal/googleidp/googleidp_test.go`) with payloads that include and omit
  `name`/`picture`; no live Google — the id_token is crafted, as the existing
  googleidp tests already do. Stamping (D18 part B) and header emission (D19) are
  HTTP-level `server`-package tests: a request is driven through the callback /
  introspection routes, then the persisted `owner_id` is read back from the temp
  DB (stamping) or the introspection **response headers** are asserted directly
  (emission) — including that existing headers are unchanged, that a Unicode /
  CR-LF attribute emits ASCII+injection-safe and round-trips on decode, and that
  an empty/absent identity never turns an allow into a deny.
- **Chart rendering is pure and unit-tested on geometry, not pixels.** The SVG
  builders are pure functions of a `Store` snapshot; tests assert computed
  coordinates and structure (hero y-axis mapped `0 → total capacity`; stacked band
  height at each x equals the sum of service values; the long tail folds into one
  "Other" band; a legend names every band; each visible band gets a distinct
  palette color) and the `humanBytes` binary-unit formatter, plus that the served
  `app.js` wires the 60s telemetry poll. The categorical band palette was validated
  colorblind-safe with the dataviz validator when authored and is committed as a
  fixed ordered set; the suite asserts the render *uses* it (distinct color per
  band + legend), not that CI re-runs the validator.

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

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated.
Existing `R-XXXX-XXXX` ids are stable handles — never renumbered; a newly added
behavior gets a freshly minted id, and a removed behavior's id is deleted with it.
