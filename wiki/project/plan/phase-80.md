# Phase 80 — MCP surface over `appkit/mcp`

*Realizes design Decision 53. Depends on Phase 78 (one inline composition root)
and on the appkit chassis providing `appkit/mcp` with the standard tools (appkit
D8–D9), consumed through the committed replace as a fixed external contract.
Independent of Phase 79. The existing domain-tool behavioral ids (D10/D16/D27/…)
are re-proven through the new seam.*

Observable end state:

- `wiki/internal/mcp` declares `Instructions` (the pinned wiki instructions
  string from D53), `Tools(...) []mcp.Tool` returning wiki's **13 domain tools**
  (`ingest`, `status`, `abort`, `rerun`, `jobs`, `jobs_count`, `merge`,
  `merges`, `ask`, `subjects`, `claims`, `page`, `llm_calls`) gated by the
  injected services (the `Option`/`WithXxxService` surface is retained), and
  `NewHandler(rt, opts...) (http.Handler, error)` assembling `appkit/mcp.New`
  over `{Service, Version, Instructions, Tools, Health, Events, Publishes,
  Subscriptions}` from the Router accessors. `cmd/wiki/main.go` mounts
  `POST /mcp` with it.
- Each domain tool's `Name`/`Description`/`InputSchema` is byte-for-byte the
  current value (the `*Tool()` descriptor funcs and `objectSchema`/`listSchema`
  reused verbatim). Each handler body is preserved (arg decode, required-field
  checks, identity gating now off the `server.Identity` parameter, `sql.ErrNoRows`
  → `notFound`, and the reflect-based `publicXxxResult` shaping) but returns
  `(map[string]any, error)`: domain outcomes return `(result, nil)` (an `isError`
  or JSON result, HTTP 200), never a Go error, so the wire stays a 200 result and
  not a JSON-RPC `-32602`.
- Deleted from `internal/mcp`: the hand-rolled transport (`ServeHTTP`,
  `handleToolCall`, `dispatchTool`, `tools()`, the `request` type, the JSON-RPC
  result/error writers), and the local `health`/`reflection` tools
  (`handleHealthCall`, `handleReflectionCall`, `healthTool`, `reflectionTool`) —
  the chassis owns those.
- The domain-tool behavioral tests swap their handler constructor to
  `NewHandler` and keep their assertions.

The transport metadata normalizes to the chassis standard (`initialize.serverInfo.name`
= `rt.Service()` = `"wiki"`; `initialize.instructions` = `Instructions`; the
chassis `health`/`reflection` descriptors and bodies) — the deliberate D8/D9
de-forking. The **tool surface** (names, descriptions, schemas, results,
error envelopes for the 13 domain tools) is unchanged.

**Done when:** the suite is green (`cd wiki && go build ./...`, `go vet ./...`,
`gofmt -l .` empty, `go test ./...`) and:

- **R-JKMR-5MV1** is covered by a clearly-named test asserting `tools/list`
  through the assembled handler returns **exactly fifteen** tools — the 13 wiki
  domain tools **plus** chassis `health` and `reflection`;
- the pre-existing domain-tool behavioral tests pass through the assembled
  handler with no assertion changes;
- `grep -rn "writeJSONRPCError\|jsonRPCRequest\|func (h \*Handler) ServeHTTP\|handleToolCall" wiki/internal/mcp/*.go`
  returns no matches;
- `grep -n "healthTool\|reflectionTool\|handleHealthCall\|handleReflectionCall" wiki/internal/mcp/*.go`
  returns no matches.
