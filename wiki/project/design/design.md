# wiki — Design

**Authority: shape and its proof.** This document and the `project/design/` directory it heads own *how* the wiki is built and *how each behavior is proven*. The product (`project/product/product.md`) owns the *why*, *for whom*, and the user-facing promises; design states the **exact, checkable form** of those promises and never re-declares the why. Design *uses* the product's contractual constants by value (page cap 12,000 chars; subject types `entity|event|concept`; `ask` strictly read-only) but does **not** own them. This is the single, current statement of the architecture — it is rewritten in place to stay true (stale decisions are removed, not stacked); the history of how it got here lives in the plan.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is **no separate requirements document**.
- Design's responsibility for ids ends at minting them into this doc. How coverage is measured, what counts as a covered id, and when the work is "done" are **not** design's concern — downstream phases own that.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain:** Go **1.26**, single module `module wiki` rooted at `wiki/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo).
- **Build / typecheck command:** `cd wiki && go build -trimpath -ldflags "-X main.version=$(cat VERSION)" -o build/wiki.bin ./cmd/wiki`. A bare typecheck is `go build ./...` and `go vet ./...`. The production build adds `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by `bin/ship`).
- **Test command:** `cd wiki && go test ./...`. **"The suite is green"** means: `go build ./...`, `go vet ./...`, `gofmt -l .` (no output), `go test ./...`, and `bin/check-migrations wiki` all succeed with zero failures.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Module wiring:** `appkit` and `eventplane` are committed in-repo replace-siblings (`replace appkit => ../appkit`, `replace eventplane => ../eventplane`); `github.com/ikigenba/agentkit` is a **published, proxy-fetched** dependency with **no committed replace** (see D1).
- **Migrations:** ordered SQL under `wiki/internal/db/migrations/`, embedded via `//go:embed` as `db.FS`, applied forward-only by the appkit runner. Never hand-author a version; always `bin/new-migration wiki <name>`. `001_schema_migrations.sql` is frozen/verbatim. Never edit a committed migration; `bin/check-migrations` enforces this in CI.
- **Time / IO:** the service takes its clock and any external effect (LLM provider, DB) as injected dependencies at the composition root (`cmd/wiki/main.go` via appkit's `Handlers`/`Workers` hooks), so domain code is testable without wall-clock or network.

## Testing strategy

Testing is part of the architecture, not an afterthought; the seams above exist so behavior can be exercised in isolation. The cross-cutting approach every Decision's Verification list assumes:

- **The LLM is always mocked in tests.** The `llm.Client` / `agentkit.Provider` is reached only through the D5 seam, so extract (D6), compile (D7), and ask (D9) are unit-tested against a **capturing/scripted mock provider** — it returns canned (optionally fenced/over-cap/invalid) responses and records the Conversation it was handed. **No test makes a live LLM call;** the suite is green offline with no `ANTHROPIC_API_KEY`.
- **The DB is a real temp SQLite.** Schema, constraints, normalization, the job lifecycle, and subject/page resolution (D3/D4/D9) are tested against a real `modernc.org/sqlite` database opened on a temp path and migrated by the appkit runner — DB-enforced invariants (UNIQUE, CHECK) and the `DROP TABLE pages_fts` migration (D8) are only meaningful against the real engine. These tests carry no network dependency. **Concurrency** is likewise proven against the real engine: the split write/read handles (D17) are tested with concurrent goroutines on one temp WAL database — a reader not blocking on an open writer, two writers serializing, read-your-writes across the handles — since these are properties of the engine + pool config, not of mocks.
- **Determinism via injection.** Clock and any IO are injected (above), so time-dependent behavior (received-at anchoring, job timestamps) is exercised with a fixed clock.
- **Seam-level unit tests + thin integration.** Each Decision is proven primarily at its own seam (mocked neighbors); a small number of integration tests wire the worker + real DB + mock provider end-to-end to prove the ingest→page→ask spine (the D4/D9 compounding and honest-empty ids). Pure functions (`Normalize` (D3, the single normalizer), `Path` (D11), `Mentions`/`RenderFooter` (D12), `ExtractJSON`, truncate-at-boundary) are table-tested directly; path resolution (`GetByPath`, and the alias-aware `Resolver.ResolveByPath` that the `page`/`claims` read entry adopts — D29) and the read-time link projection (`PageWithLinks`, now alias-aware — D12) run against a real temp SQLite.
- **MCP surface** (D10) is tested by driving the JSON-RPC handler with `tools/list` and `tools/call` requests over an in-process server with a stubbed identity, asserting the tool list, result/not-found shapes, `type/slug` path resolution and the rendered page link footer (D11/D12), and identity gating.
- **The landing page** (D39) is tested with `net/http/httptest` against the `GET /{$}` handler as the composition root mounts it — no DB, no LLM, no identity header: the page is a pure function of `Service()`+`Version()`. Assertions cover the 200 / `text/html` response, the rendered name+version, the embedded Carbon `tokens.css`/fonts asset route, exact-root matching (no shadow of `/mcp`/`/health`/`/feed`/PRM), and that the route is ungated in-process (served with no token). The nginx session-gate itself is config, not Go — proven by Phase 64's named fragment check, not an `R-id` test.

## Web surface (the landing page)

wiki is no longer MCP-only: it serves one **human HTML web page** — the landing page at the bare mount root `GET /{$}` (service name + version, Carbon-styled) — **beside** the unchanged MCP/`/health`/PRM JSON surfaces (D39). The two surfaces have two audiences gated two ways: **agents** reach `/mcp` with an opaque bearer (`auth_request /_authn`, unchanged); **humans** reach the landing page with the dashboard login-session cookie (`auth_request /_session-authn`, the same coarse gate `sites` uses for its private tier). Both routes are mounted **ungated in-process** — nginx remains the sole trust boundary — so the landing handler reads no token and no identity header. wiki embeds its **own** copy of the Carbon assets (`tokens.css` + woff2 fonts) under a new `internal/web` package via `//go:embed`, mirroring the dashboard's `ui/` precedent, so the binary stays one static file and each app's page can diverge later without a shared-library change. Details and the nginx fragment live in D39.

## Layout

The design is split for addressability so a build phase reads only the one Decision it realizes:

- `project/design/design.md` — this spine: static cross-cutting facts only, no per-Decision detail.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded: `D01.md`, `D02.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a sorted `R-id → Decision/file` reverse map. It is the grep target for resolving an id.

Design is **rewritten in place**, not append-only (history lives in the plan): a changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a new Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are stable handles — never renumbered; a newly added behavior gets a freshly minted id, and a removed behavior's id is deleted with it.
