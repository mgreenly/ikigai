# Phase 12 — Structured MCP adoption: migrate off the hand-rolled transport

*Realizes design Decision 8 (Structured MCP adoption). Depends on Phase 4 (the
MCP tool surface — the 14 domain verbs, provenance logging, `GitHubClient`
interface) and Phase 5 (the `GET /pr` loopback twin, whose guard this phase
swaps). D4 and D5 are rewritten in place to cite D8; this phase builds the
migration.*

## What gets built

The hand-rolled MCP JSON-RPC layer is replaced by the shared `appkit/mcp`
transport, and the `/pr` self-guard is replaced by the chassis loopback guard.

- **`internal/mcp/mcp.go`** — delete the bespoke transport: the `Handler`
  struct + `ServeHTTP` method switch (`initialize`/`tools/list`/`tools/call`),
  `jsonRPCRequest`/`toolCallParams`, `writeJSONRPCResult`/`writeJSONRPCError`/
  `idOrNull`, and `toolResultText`/`toolResultErr`. Keep only the `GitHubClient`
  interface (D3's 14 methods, unchanged). Add `NewHandler(client, rt) →
  appkitmcp.New(appkitmcp.Options{…})` (Service/Version/Instructions/Tools/Health/
  Events/Subscriptions from the `*appkit.Router` seam), structurally like
  `crm/internal/mcp`.
- **`internal/mcp/tools.go`** — `Tools(client, logger)` returns
  `[]appkitmcp.Tool`, one per domain verb, each with its existing input schema,
  a hand-authored `OutputSchema` per D8's table — the eleven single-object verbs
  mirror their emitted JSON verbatim; the three list verbs wrap under `items`
  (`type:object`, `required ["items"]`); `file_get`/`file_put` metadata-only, no
  `content` — a handler that dispatches to the `GitHubClient` method and returns
  `appkitmcp.StructuredResult(v)` on success (list verbs wrapping as
  `{"items": …}`) or `appkitmcp.ErrorResult(code, msg)` on failure, and the
  write-verb `slog` provenance line preserved. A private
  `codeFor(err)` realizes the client-side failure→code mapping; local
  argument-validation failures return `ErrorResult(appkitmcp.ErrValidation, …)`
  before any client call. The `health`/`reflection` tools are **removed** from
  the table (the transport provides them).
- **`internal/gh/pr_route.go`** — delete the inline
  `hasIdentityHeader(X-Owner-Email)||hasIdentityHeader(X-Forwarded-Proto)` guard
  and the `hasIdentityHeader` helper; `PRHandler` keeps only its input parsing
  and `PRGet` call.
- **`internal/githubapp/spec.go`** — mount `POST /mcp` via the new `NewHandler`
  behind `rt.RequireIdentity`, and mount `GET /pr` via
  `rt.HandleLoopback("GET /pr", client.PRHandler())` (chassis guard).
- Tests (`internal/mcp/tools_test.go`, `internal/gh/pr_route_test.go`) are
  rewritten to the appkit-served handler and the chassis-guarded route.

Observable end state: a machine caller gets `structuredContent` + a typed error
`code` from every domain verb; `tools/list` advertises each domain verb with an
`outputSchema`; `initialize` reports `2025-06-18`; `health`/`reflection` are
transport-provided; and `GET /pr` is 404'd for front-door requests but served for
a loopback caller asserting `X-Owner-Email`.

## Done when

All hold on identical repo state, from `github/`:

- `GOWORK=off go build ./...` and `GOWORK=off go test ./...` exit 0; `gofmt -l .`
  empty; `go vet ./...` clean. (The service compiling against the new
  `appkit/mcp` — `StructuredResult`'s error return, `ErrorResult`'s typed code —
  is itself a gate.)
- The hand-rolled transport is gone:
  `grep -rnE 'toolResultJSON|toolResultErr|toolResultText|writeJSONRPCError|jsonRPCRequest|hasIdentityHeader|JSONResult|2025-03-26' internal cmd --include='*.go' | grep -v _test.go`
  returns empty.
- Clearly-named offline tests cover and pass, each id named in a test:
  - `R-FI1O-9E44` — `initialize` returns `protocolVersion` exactly `2025-06-18`.
  - `R-FJ9K-N5UT` — `repo_get` success carries `structuredContent` equal to the
    `gh.Repo` value plus a mirroring `text` block of the same JSON.
  - `R-FKHH-0XLI` — table-driven over `tools/list`: all 14 domain tools declare a
    non-nil `outputSchema` (no prose exceptions); `health`/`reflection` carry
    schemas too.
  - `R-FLPD-EPC7` — `repos_list`/`pr_list`/`issue_list` declare top type `object`
    with `required ["items"]` and emit `structuredContent` `{"items": [...]}`
    (never a bare top-level array); `file_get`'s `outputSchema` properties are
    exactly `{path, sha, encoding}` (no `content`) and its result omits the body.
  - `R-FMX9-SH2W` — client `gh.ErrNotFound` → `isError` with
    `structuredContent.code == "not_found"`.
  - `R-FO56-68TL` — client `gh.ErrInvalid` (422) → `structuredContent.code ==
    "validation"`.
  - `R-FPD2-K0KA` — client `gh.ErrAppAuth` **and** a generic transport error each
    → `structuredContent.code == "source_unavailable"`.
  - `R-FQKY-XSAZ` — a missing/malformed required argument → `isError` with
    `structuredContent.code == "validation"` and the `GitHubClient` is not called.
  - `R-FT0R-PBSD` — `GET /pr` via `rt.HandleLoopback`: a request with
    `X-Forwarded-Proto: https` → bare 404, `PRHandler` not run; the same
    `?repo=&number=` request without `X-Forwarded-Proto` but with `X-Owner-Email`
    set → 200 with the PR JSON.
