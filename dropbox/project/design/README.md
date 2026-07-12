# dropbox — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* the dropbox service is built and *how each behavior
is proven*. The product (`project/product/README.md`) owns the *why*, *for whom*,
and the user-facing promises; design states the **exact, checkable form** of
those promises and never re-declares the why. Design *uses* the product's
contractual constants by value (the landing page lives at the mount root only;
the visual system is Carbon; the suite is the authority and Dropbox is a replica;
the shared namespace is convention-organized) but does **not** own them. This is
the single, current statement of the architecture — it is rewritten in place to
stay true (stale decisions are removed, not stacked); the history of how it got
here lives in the plan.

> **Scope.** This design's Decisions cover four threads:
>
> 1. **The web landing page** (D1–D8, built; substrate moved by D11): the page's
>    content, route, session gate, canonical markup, and self-served assets —
>    now rendered from the on-disk `share/www` tree through the chassis.
> 2. **Registry adoption** (D9–D10, built): dropbox resolves its **own**
>    loopback address by name (the listen port, the `/content` base default, and
>    the reflection example origin), with source-scan and deploy-artifact drift
>    guards.
> 3. **The chassis conversion** (D11–D13, built): the web surface through
>    `Spec.WWW`, the MCP surface through `appkit/mcp`, and the leftover
>    `internal/db` shim deleted — all behavior-preserving. appkit's
>    `Spec.WWW` / `appkit/web` / `appkit/mcp` surfaces (appkit design D5–D9) are
>    fixed external contracts consumed through the committed
>    `replace appkit => ../appkit`.
> 4. **The bidirectional, service-facing filesystem** (D14–D20, active): the
>    mirror stops being download-only. dropbox stays the **sole owner** of the
>    folder and exposes a read/write/discovery **API** (option B) so the suite's
>    services can create, read, delete, move, and walk files and directories;
>    every local mutation is **pushed up** to Dropbox asynchronously through a
>    durable queue, with the suite as authority (overwrite) and Dropbox as
>    replica. Streaming byte paths (D14), first-class directories (D15), the
>    write API + loopback routes (D16), the push-up queue/client/uploader (D17),
>    origin-tagged events (D18), MCP write tools (D19), and a `dropbox/docs/`
>    integrator reference (D20).
> 5. **Suite-protocol conformance** (D19 revised + D22, active): the **content
>    plane** (`docs/content-plane-design.md`) — MCP `put` becomes
>    reference-based (`source_url`, fetched server-side, confined to loopback +
>    registry ports), with the capped base64 form kept as the inline
>    convenience (D19); and the **event-routing revision**
>    (`docs/event-routing-design.md`) — the `file.created`/`modified`/`deleted`
>    types become kinds `create`/`modify`/`delete` with the mirror path as
>    subject, the registry becomes `outbox.Family` entries, and the outbox
>    converts by a new timestamped migration (D22). D22 consumes the revised
>    eventplane API (`eventplane/project/design/` D1–D4) — spec'd but not yet
>    built; its phase is operator-sequenced behind eventplane plan phases
>    01–04.
>
> The pre-existing **download** engine and its load-bearing correctness rules
> (crash/replay ordering, per-page cursor advance, poison bound, download
> hash-verify, case-folding, unauthenticated longpoll) remain in force — thread 4
> **extends** the domain (adds the write direction) rather than replacing it, and
> preserves every download invariant (`dropbox/CLAUDE.md` documents them). Thread
> 4 **does** add schema: two additive, timestamped migrations (a `directories`
> table, D15; an `upload_queue` table, D17) created via `bin/create-migration`,
> and thread 5 adds a third (the outbox kind/subject rebuild, D22) — the frozen
> `001`–`003` are untouched. **Wiring the suite's *other* services to
> call this API is out of scope** — each service adopts it in its own `project/`
> later; this design builds only dropbox's side.

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
  conflict, base64 bodies, the sentinel→code error envelope). The table/chassis
  partition (the dropbox-declared domain tools + chassis `health`/`reflection`,
  currently the eight-tool surface pinned by D19) is asserted at the same seam.
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
- **The filesystem write path is tested hermetically over temp DB + temp mirror.**
  The mirror byte primitives (D14), the directory model (D15), the write Service
  methods and loopback routes (D16), the upload queue and uploader logic (D17),
  and origin tagging (D18) are exercised with a temp SQLite DB, a temp mirror
  dir, a **recording event sink** (captures emitted payloads without a real
  outbox), and — for the push side — an **httptest fake Dropbox** (the existing
  `client_test.go` pattern) that captures request shape (`mode:overwrite`,
  `upload_session` chunking). No hermetic test makes a network call or needs a
  running suite; the local filesystem is the real substrate for the atomicity,
  Range, and large-file round-trip claims.
- **The real Dropbox write contract is proven by a `-tags live` smoke.** Some
  claims hinge on the **real external contract** — Dropbox accepting an
  `overwrite`, returning a `rev` that a later `list_folder` actually reports
  (echo suppression), reassembling an `upload_session`, applying
  `create_folder`/`delete`/`move`. A mock cannot falsify these, so they carry
  **distinct LIVE ids** (D17: R-KEIO-B98F, R-KFQK-P0Z4, R-KGYH-2SPT) whose test
  runs against the **real app folder** with the suite refresh token
  (`DROPBOX_*` from `.envrc`), asserting the observable outcome via a follow-up
  `list_folder`. The live smoke is **not** part of the hermetic green suite —
  it is a distinct, reachable check (`go test -tags live ./...`) the D17 build
  phases require be run once; the phases that own those ids state it in their
  "Done when".
- **The `source_url` fetch is proven against a real local HTTP server.** The
  reference-based MCP `put` (D19) is tested with a real `httptest` server on
  loopback serving known bytes — the fetch, the 404/409/refused failure
  mapping, and the no-mutation-on-failure claims all run against that real
  substrate. The **confinement check takes its allowed-port set as data**
  (injected in tests, derived from `registry.Services` at the composition
  root), and the registry derivation is proven separately against the real
  `registry.Services` table — so the confinement tests are discriminating
  without needing to bind registered ports.
- **Routing conformance is proven on real substrates** (the gmail D18
  pattern): kind/subject rows are read back by SQL from a real temp SQLite
  database migrated through the full embedded set; the canonical-key frame
  (`event: dropbox:create/notes/meeting.md`) is asserted through the real
  eventplane `FeedHandler` over `httptest`; the family registry through the
  assembled MCP reflection surface. D22's phase additionally depends on the
  revised eventplane API being built (external ordering, operator-sequenced).

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
`internal/db` (embedded migrations + the load and outbox byte-equality
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

**Package shape additions (thread 4, D14–D20).** `internal/dropbox` gains the
streaming byte primitives (`WriteFrom`/`Open` in `mirror.go`, D14), the directory
store/service surface (`store.go`/`service.go` + `004_directories.sql`, D15), the
mutating `Service` methods (`Write`/`Mkdir`/`Delete`/`Move`, D16), the Dropbox
**write** client methods (`Upload`/`CreateFolder`/`DeletePath`/`Move` in
`client.go`, D17), a new `uploader.go` worker draining `upload_queue`
(`005_upload_queue.sql`, D17), and the `origin` field on the event payload
(`events.go`, D18). `cmd/dropbox/main.go` mounts the new loopback routes
(`PUT`/`DELETE /content`, `POST /mkdir`, `POST /move`, `GET /stat`) beside the
existing byte routes and starts the uploader through the `Spec.Workers` hook.
`internal/mcp` grows four write tools (`put`/`mkdir`/`delete`/`move`, D19), with
`put` reference-based first (`source_url` fetched server-side through an
allowed-port seam threaded from the composition root; capped base64 kept as the
inline convenience). The frozen `001`–`003` migrations are untouched; the
timestamped `directories`/`upload_queue` migrations are additive. A new shipped
`dropbox/docs/` tree carries the filesystem-API integrator reference (D20).

**Package shape additions (thread 5, D22).** `internal/dropbox/events.go`
replaces the `file.*` type constants with kind constants
(`KindCreate`/`KindModify`/`KindDelete`), emits `outbox.Event{Kind, Subject:
<display path>, Payload}` (the payload drops its `event` discriminator field;
the other seven fields are unchanged), and reshapes `dropbox.Events` into three
`outbox.Family` entries. One further timestamped migration
(`bin/create-migration dropbox outbox_routing`) rebuilds the outbox table per
the revised `outbox.SchemaSQL`; the byte-equality drift guard in
`internal/db/migrations_outbox_test.go` re-points at that newest migration
while the frozen `003_outbox.sql` stays untouched.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are
stable handles — never renumbered; a newly added behavior gets a freshly minted
id, and a removed behavior's id is deleted with it.
