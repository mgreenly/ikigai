# Phase 12 — MCP surface over `appkit/mcp`

*Realizes design Decision 12 (the four-tool table). Depends on Phases 10–11 only
for a settled `main.go` (mechanically independent otherwise); depends on the appkit
chassis providing `appkit/mcp` with the standard `health`/`reflection` tools (a
fixed external contract via the committed replace). Covers `R-0JBF-690N`;
re-proves D6's `R-5Z8J-Y0YP`, `R-60GG-BSPE`, `R-61OC-PKG3`, `R-62W9-3C6S`,
`R-6445-H3XH`, `R-65C1-UVO6` through the assembled handler. **Read D12 and the
rewritten D6 for the exact shape.***

Observable end state:

- `webhooks/internal/mcp` declares `Instructions` (the exact pre-conversion
  initialize instructions bytes), `Tools(svc *webhooks.Service, baseURL string)
  []appkitmcp.Tool` (the four domain tools: `create`, `list`, `delete`, `rotate`
  — descriptors, schemas, owner scoping, URL minting, and error envelope unchanged
  in wire content), and `NewHandler(svc, rt) (http.Handler, error)` assembling
  `appkitmcp.New` over `Instructions` + `Tools(svc, baseURL)` + the Router-threaded
  `Service`/`Version`/`Health`/`Events`. `baseURL` is derived inside `NewHandler`
  as `strings.TrimSuffix(rt.ResourceID(), "mcp")`.
- The local JSON-RPC transport (`jsonRPCRequest`, `ServeHTTP`, the
  `initialize`/`notifications/initialized`/`tools/list`/`tools/call` dispatch,
  `writeJSONRPCResult`/`writeJSONRPCError`/`idOrNull`), the local `Identity` type,
  the local `toolHealth`/`toolReflection`/`reflectionUnknownTypeError`
  implementations, and the local result-envelope helpers
  (`toolResultText`/`toolResultErr`/`toolResultJSON`) are deleted; the domain
  verbs use the chassis `appkitmcp.JSONResult`/`TextResult`/`ErrorResult` and read
  identity from the `server.Identity` the chassis supplies to each `Tool.Handler`.
- `cmd/webhooks/main.go`'s `Handlers` mounts `POST /mcp` via
  `handler, err := mcp.NewHandler(svc, rt)` (error-checked) wrapped in
  `rt.RequireIdentity`.
- `tools_test.go` drives the assembled handler (built via a real `appkit/server`
  Router whose `Register` hook calls `mcp.NewHandler(svc, rt)`, with
  `ResourceID: "https://int.ikigenba.com/srv/webhooks/mcp"` so the derived
  `baseURL` is the mount root) and keeps every behavioral assertion (owner
  scoping, show-once secret, `trigger_url` shape, duplicate/validation/not_found
  envelopes, the non-owner-mutates-nothing end-to-end ingress check). Its
  `newTestHandler` builds `svc` over a real temp-file SQLite (its own conn +
  outbox, as today) and assembles the handler through a real `appkit/server`
  Router; the DB standup keeps using the local `db.Open`/`db.Migrate` wrappers for
  now (they still exist — the swap to `appkit/db` directly is phase 13's shim
  deletion). The tool count is six.

**Done when:** the suite is green — `cd webhooks && go build ./...`,
`cd webhooks && go vet ./...`, `cd webhooks && gofmt -l .` (no output), and
`cd webhooks && go test ./...` all succeed with zero failures — and:

- R-0JBF-690N — a clearly-named test asserts `tools/list` through the assembled
  handler returns **exactly six** tools: `create`, `list`, `delete`, `rotate`
  declared by webhooks plus chassis `health` and `reflection`.
- The pre-existing `create`/`list`/`delete`/`rotate` behavioral tests
  (R-5Z8J-Y0YP, R-60GG-BSPE, R-61OC-PKG3, R-62W9-3C6S, R-6445-H3XH, R-65C1-UVO6)
  pass through the assembled handler with no assertion changes.
- `grep -rn "writeJSONRPCError\|jsonRPCRequest\|func (h \*Handler) ServeHTTP" webhooks/internal/mcp --include=*.go`
  returns no matches, and
  `grep -rn "func (h \*Handler) toolHealth\|func (h \*Handler) toolReflection" webhooks/internal/mcp --include=*.go`
  returns no matches (the local transport and standard-tool implementations are
  gone).
