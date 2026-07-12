# appkit — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* appkit's ralph-governed surfaces are shaped and
*how each behavior is proven*. The product (`project/product/product.md`) owns
the *why* and the user-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. This design is the
**single current** statement, rewritten in place (stale decisions removed, not
stacked); the history of how it got here lives in the plan.

> **Scope.** This design covers three threads:
>
> 1. **The manifest read-path** (D1–D4, built): every manifest reader resolves
>    *through* the per-app `etc/current` deploy symlink; the local dev layout
>    mirrors the box; the stable sibling path is retired.
> 2. **The uniform service chassis surfaces** (D5–D9, built): the on-disk
>    web-asset root (config resolution D5, the `appkit/web` package D6, the
>    chassis integration D7) and the chassis MCP surface (the JSON-RPC
>    transport D8, the standard `health`/`reflection` tools D9).
> 3. **Chassis-owned consumer loops** (D10, active): the declared
>    `Spec.Consumers` table from which the chassis derives the manifest
>    `CONSUMES=`, the reflection subscriptions, and the running
>    `eventplane/consumer` loops — feed-URL/`From` env resolution included.
> 4. **Event-routing conformance** (D11 + the D9 rewrite, active): appkit
>    compiles and plumbs the suite's routing revision
>    (`docs/event-routing-design.md`, specified in
>    `eventplane/project/design/` D1–D4) — `Spec.Events` carries the
>    family-based `outbox.Registry`, the chassis `reflection` tool speaks
>    kinds (D9, rewritten in place), and every other eventplane coupling
>    (feed pass-through, test fixtures) cuts over. **Externally ordered:**
>    eventplane's plan phases 01–04 build first; appkit's conformance phase
>    then precedes every service's own conformance phase (appkit is the hinge
>    between eventplane and the services).
>
> appkit's other pre-existing surfaces — the verb dispatcher, migrations, the
> loopback server's PRM/health/feed routes, the producer/worker seams — are
> settled prior art this design extends and does **not** reopen. Every D5–D10
> change is **additive**: a service that sets none of the new Spec fields and
> imports none of the new packages compiles and behaves exactly as before.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is **no
  separate requirements document**.
- Design's responsibility for ids ends at minting them into this doc. How coverage
  is measured and when the work is "done" are **not** design's concern —
  downstream phases own that.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain:** Go **1.26**, single module `module appkit` rooted at
  `appkit/`. Consumed by every service via a committed
  `replace appkit => ../appkit`; never tagged.
- **Build / typecheck command:** `cd appkit && go build ./...` and `go vet ./...`.
  The isolated-module check (mirroring the production build) adds `GOWORK=off`.
- **Test command:** `cd appkit && go test ./...`. **"The suite is green"** means
  `go build ./...`, `go vet ./...`, `gofmt -l .` (no output), and `go test ./...`
  all succeed with zero failures, from `appkit/`.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Dependencies:** the D5–D9 packages use only the standard library plus the
  existing in-repo `eventplane` sibling (already a committed require/replace in
  `appkit/go.mod`). D10 adds one more **in-repo** replace-sibling, `registry`
  (`require registry v0.0.0` + `replace registry => ../registry`) — the static
  leaf address table. No new third-party dependency. Because a dependency's
  `replace` directives are not transitive, every module requiring appkit must
  mirror the registry replace (exactly as `eventplane` already forced); the
  sweep over the services that don't yet carry it is an operator step in
  `project/plan/plan.md`, not appkit-phase work.
- **Testing substrate:** all D5–D9 behavior is provable in-process —
  `net/http/httptest` for HTTP, `t.TempDir()` for on-disk asset roots, injected
  `getenv` maps for config. A `t.TempDir()` tree is a real filesystem, so the
  web-root load/serve/missing-root claims are exercised against the substrate
  that can falsify them — no mocks stand in for the disk.
- **The on-box layout is a fixed external contract.** `bin/ship` bundles a
  service's `<svc>/share/` directory into `share/<version>/`, and `opsctl`
  swaps the `share/current` symlink atomically on deploy/rollback;
  `IKIGENBA_ROOT` roots the `/opt/<app>/` tree. appkit *consumes* these facts
  (D5 composes paths from them); it does not define or change them.
- **Cross-module collaborators (outside `appkit/`).** The repo-root shell
  scripts `bin/registry` and `bin/start` are not Go and not under `appkit/`;
  where a Decision names one (D2–D4 historically, D7's dev wiring) it is a
  boundary-crossing collaborator of this chassis work, verified by its shell
  test or a live `bin/start` smoke, **not** by the appkit Go suite. Phases that
  touch them are called out explicitly in the plan.
- **Additivity guard (D5–D10):** none of the new Spec fields, Router accessors,
  or packages may change the behavior of a Spec that doesn't use them. The
  pre-existing appkit test suite passing unchanged is the standing proof.
  **D11 is the deliberate exception:** the routing revision is a suite-wide
  hard cutover (no compatibility period), so D11 revises fixtures and the
  reflection wire surface rather than preserving them — additivity does not
  apply to it.
- **This design touches no schema and no `opsctl` code.** appkit is a library:
  it owns no service database and no outbox table, so the routing revision's
  outbox DDL change (D11) reaches services through their own migrations, never
  through appkit.

## Testing strategy (D5–D11)

- **`appkit/config`** is pure over its injected `getenv`; www-root resolution is
  table-tested exactly like the existing DB-path composition.
- **`appkit/web`** is tested against real temporary directories: tests write
  template and asset files into `t.TempDir()`, load them, and drive the returned
  handlers with `httptest`. Failure paths (missing root, missing template) are
  real filesystem states.
- **Server integration (D7)** is tested at the `server.New` seam the existing
  server tests use: build `server.Options` with and without a loaded site, and
  assert route presence/absence and served bytes through the real mux.
- **`appkit/mcp`** is tested through the real `ServeHTTP` JSON-RPC seam — the
  same harness style every service's `tools_test.go` uses — with a test tool
  table whose handlers record their inputs. The standard tools are driven
  through the same seam with real `outbox.Registry` / `consumer.Subscription`
  values.
- **Consumer loops (D10)** split the same way `appkit/config` and the server
  do: env resolution is pure and table-tested over injected `getenv` maps; the
  loop itself is proven end-to-end against a **real** SSE feed served by
  `httptest` over a **real** `t.TempDir()` SQLite database, because cursor
  independence and delivery ordering are exactly the claims a stub cannot
  falsify. The manifest/reflection derivations reuse the existing manifest-emit
  and mcp-seam harnesses.
- **Routing conformance (D11 + revised D9)** is proven on the real eventplane:
  the gating and wire claims run `feed.Start` over a real `t.TempDir()` SQLite
  database (`modernc.org/sqlite`) and drive the returned `Producer.Handler`
  through `httptest`; the reflection claims go through the D8 `ServeHTTP` seam
  with real family-shaped `outbox.Registry` values. Converted consumer-test
  fixtures frame `kind`/`subject` envelopes with canonical-key `event:` lines.

## Layout

The design is **split for addressability** so the build loop reads only the one
Decision a phase realizes:

- `project/design/INDEX.md` — the manifest: each Decision → its `DNN.md`, plus a
  sorted `R-id → Decision/file` reverse map (the grep target for id lookup).
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded
  `D01.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/design.md` — this spine: static cross-cutting facts only.

**New packages (D5–D9).** This work adds two packages to the module:
`appkit/web` (template loading + rendering + static serving over an on-disk
root) and `appkit/mcp` (the JSON-RPC MCP transport + standard tools). It extends
two existing seams: `appkit/config` (www-root resolution) and the root
`appkit`/`appkit/server` pair (the `Spec.WWW` field, site loading at serve, the
auto-mounted static route, the `Router.WWW()` accessor).

**Consumer seam (D10).** No new package: the `Consumer` type and `Consumers`
Spec field live in the root `appkit` package beside `Workers`; the
feed-URL/`From` env resolution extends `appkit/config`; the manifest and
reflection derivations extend the existing emit/tool paths.

Design is rewritten in place, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a new
Decision adds a `DNN.md` and an INDEX entry.
