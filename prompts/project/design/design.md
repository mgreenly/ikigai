# prompts agentkit migration — Design

**Authority: shape and its proof.** This document owns *how* the migration is built and *how each behavior is verified*. The product doc owns the *why* and the user-facing promises; this doc uses the product's contractual constants (provider names, config keys) by value but does not restate the intent behind them. Design states the exact, checkable form of those promises — mechanism, interfaces, types, naming, test strategy. This is the single current statement of the architecture: when a decision changes, its `DNN.md` is rewritten in place; history lives in the plan.

## Requirement ids

Each Decision ends with a **Verification** list. Every item in that list carries a minted id of the form `R-XXXX-XXXX` — a stable, unique handle for that one behavior. The ids live inline in the Verification lists and nowhere else; there is no separate requirements document.

Design's responsibility ends at minting. How coverage is measured, what counts as covered, and when the work is done are downstream concerns — not specified here.

## Conventions

- **Language / toolchain**: Go 1.26, module path `prompts`.
- **Build**: `go build ./...` run from the `prompts/` directory. Passes when all packages compile without error.
- **Test**: `go test ./...` run from the `prompts/` directory. "The suite is green" means every test passes and no race detector violations appear (`-race` is implicit in CI).
- **Formatting**: `gofmt -l .` emits no output.
- **Published agentkit**: `github.com/ikigenba/agentkit v0.1.0` — the external dependency that replaces the local `./agentkit` module for `prompts` only.
- **Local chassis modules**: `appkit` and `eventplane` remain as committed `replace` directives in `prompts/go.mod`; they are out of scope for this migration.

## Web surface (the landing page)

prompts is no longer MCP-only: it serves one **human HTML web page** — the landing page at the bare mount root `GET /{$}` (service name + version, Carbon-styled) — **beside** the unchanged MCP/`/health`/PRM/`/feed` surfaces (D10). The two surfaces have two audiences gated two ways: **agents** reach `/mcp` with an opaque bearer (`auth_request /_authn`, unchanged); **humans** reach the landing page with the dashboard login-session cookie (`auth_request /_session-authn`, the same coarse gate `sites` uses for its private tier). Both routes are mounted **ungated in-process** (in `registerRoutes`, beside the existing `POST /mcp`) — nginx remains the sole trust boundary — so the landing handler reads no token and no identity header. prompts embeds its **own** copy of the Carbon assets (`tokens.css` + woff2 fonts) under a new `internal/web` package via `//go:embed`, mirroring the dashboard's `ui/` precedent, so the binary stays one static file and the page can diverge later without a shared-library change. The landing page is proven with `net/http/httptest` against the `GET /{$}` handler as the composition root mounts it — no DB, no LLM, no runner, no identity header — covering the 200 / `text/html` response, the rendered name+version, the embedded `tokens.css`/fonts asset route, exact-root matching (no shadow of `/mcp`/`/health`/`/feed`/PRM), and the ungated-in-process route. The nginx session-gate itself is config, not Go — proven by Phase 12's named fragment check, not an `R-id` test. Details and the nginx fragment live in D10.

## Layout

`project/design/INDEX.md` is the manifest: each Decision maps to its `DNN.md` file, and every `R-XXXX-XXXX` id maps back to its Decision and file.

`project/design/DNN.md` — one self-contained file per Decision (zero-padded), referenced in prose and the plan as `D<N>`.

This spine holds only the cross-cutting facts above. Rewritten in place when decisions change; history lives in the plan.
