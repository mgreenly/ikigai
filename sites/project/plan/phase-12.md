# Phase 12 — MCP surface over `appkit/mcp`

*Realizes design Decision 13 (the fourteen-domain-tool table over the chassis
transport). Depends on Phase 11 only for a settled `cmd/sites/main.go`
(mechanically independent otherwise); depends on the appkit chassis providing
`appkit/mcp` with the standard tools (appkit plan Phases 08–09), consumed through
the committed replace as a fixed external contract.*

Observable end state:

- `sites/internal/mcp` declares `Instructions`, `Tools(store, layout, baseURL,
  mirror) []mcp.Tool` (the fourteen domain tools — `describe`, `create`, `list`,
  `delete`, `mkdir`, `publish`, `unpublish`, `sync`, `file_write`, `file_read`,
  `file_edit`, `file_glob`, `file_grep`, `file_list` — each with descriptor,
  schema, and handler wire-unchanged), and `NewHandler(store, layout, baseURL,
  mirror, rt) (http.Handler, error)` assembling the `appkit/mcp` handler;
  `cmd/sites/main.go` mounts `POST /mcp` with it behind `rt.RequireIdentity`.
- The local JSON-RPC transport (`ServeHTTP`, `jsonRPCRequest`,
  `writeJSONRPCResult`/`writeJSONRPCError`, `idOrNull`, `handleToolCall`,
  `dispatchTool`, `toolDescriptors`), the local `Identity` type, the local
  `toolHealth` implementation, the `SetMirrorClient` field-injection seam, and
  the private result-envelope helpers (`toolResultText`/`toolResultErr`) are
  deleted from `sites/internal/mcp`; result envelopes come from
  `mcp.TextResult`/`mcp.JSONResult`/`mcp.ErrorResult`.
- The mirror client is a constructor parameter to `Tools`/`NewHandler` (not a
  post-construction field); `sync` behavior is unchanged.
- The existing domain-tool behavioral tests (`tools_test.go`, `files_test.go`,
  `sync_test.go`) drive the assembled `appkit/mcp` handler and keep their
  assertions (lifecycle happy paths, confinement → `path_escapes_working_dir`,
  the cleaner file-tool result shapes, sync reconcile); they swap only the
  handler constructor (and the fake-mirror wiring moves from `SetMirrorClient`
  into the constructor call).

Dependency interfaces (from appkit D8/D9, consumed as fixed contracts):

```go
// appkit/mcp
type Tool struct {
    Name        string
    Description string
    InputSchema map[string]any
    Handler     func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error)
}
type Options struct {
    Service, Version, Instructions string
    Tools         []Tool
    Health        func(ctx context.Context) (map[string]any, error)
    Events        outbox.Registry
    Publishes     func() outbox.Registry
    Subscriptions func() []consumer.Subscription
}
func New(opts Options) (*Handler, error)          // rejects a declared health/reflection
func TextResult(text string) map[string]any
func JSONResult(v any) (map[string]any, error)
func ErrorResult(msg string) map[string]any
// appkit Router accessors to thread: rt.Service(), rt.Version(), rt.Health(),
// rt.Events(), rt.Publishes(), rt.Subscriptions()
```

**Done when:** the suite is green (design Conventions commands, from `sites/`)
and:

- R-0UUY-N97T (D13) is covered by a clearly-named test asserting the exactly-16
  partition (14 declared domain tools + chassis `health`/`reflection`), and
  R-P21E-0285 (D13) by a test that `tools/call reflection` (no args) returns
  `{"publishes":[], "subscribes":[]}` without error;
- the pre-existing domain-tool behavioral ids re-prove through the assembled
  handler with no assertion changes: R-0FMU-J775, R-0GUQ-WYXU (no agentkit —
  source/`go.mod` scans, unaffected), R-0I2N-AQOJ (cleaner file-tool result
  shapes), R-0JAJ-OIF8 (`path_escapes_working_dir` envelope);
- R-0KIG-2A5X (D11, superseded by D13) is updated in place to assert the 16-tool
  set including the chassis `reflection`, with `health`/`reflection` treated as
  chassis-supplied — the id is retained, only its expected set grows;
- `grep -rn "writeJSONRPCError\|jsonRPCRequest\|SetMirrorClient\|func (h \*Handler) ServeHTTP" sites --include=*.go`
  returns no matches, and `grep -rn "\"health\"\|toolHealth" sites/internal/mcp --include=*.go | grep -v _test`
  shows no locally-declared `health` tool.
