# Phase 20 — MCP surface over `appkit/mcp`

*Realizes design Decision 17 (the domain tool table). Depends on Phases 18–19
only for a settled `main.go`/`promptsSpec()` (mechanically independent
otherwise); depends on the appkit chassis providing `appkit/mcp` with the
standard `health`/`reflection` tools (appkit plan Phases 08–09), consumed through
the committed replace as a fixed external contract.*

prompts' `internal/mcp` is a hand-rolled JSON-RPC transport with its own `health`
tool and no `reflection`. This phase reshapes it into a declaration —
`Instructions` + `Tools(svc)` (the sixteen domain tools) + `NewHandler(svc, rt)`
over `appkit/mcp.New` — deleting the transport and the local `health`. The
domain tools' wire surface (names, descriptions, schemas, error envelopes,
instructions) is unchanged; the surface **gains** the chassis `reflection` tool
and adopts the chassis `serverInfo`/`health` copy (deliberate, per D17).

## Steps

In **`prompts/internal/mcp/`**:

- **Add `const Instructions`** = the exact string the old transport returned in
  the `initialize` `instructions` field ("Prompts runs sandboxed Claude agent
  sessions on your behalf. If you haven't used prompts before, call describe
  first …"), byte-for-byte.
- **Add `func Tools(svc *prompt.Service) []appkitmcp.Tool`** declaring the
  sixteen domain tools — `describe`, `create`, `import`, `list`, `get`,
  `update`, `delete`, `set_trigger`, `clear_trigger`, `run`, `run_list`,
  `run_get`, `run_output`, `run_cancel`, `run_fs_list`, `run_fs_read` — each with
  its **current** name, description, and `inputSchema`, and a `Handler` closure
  wrapping the existing per-tool body. `health` is **not** declared (chassis
  reserves it). Reuse the current `desc`/`obj`/`typ`/`configSchema`/
  `triggersSchema` helpers (now producing `appkitmcp.Tool` values / schema maps)
  and `describeText`.
- **Preserve the error mapping exactly.** Each tool `Handler` must:
  - return `(nil, err)` **only** for a `parseArgs` failure (unparseable
    arguments) — the chassis maps a returned error to JSON-RPC `-32602`, matching
    today's `paramError` path;
  - for a **domain** error (not-found, validation, sandbox path escape) return
    `(appkitmcp.ErrorResult(err.Error()), nil)` so it renders as an
    `isError:true` content envelope, matching today's `toolResultErr` path.
  Success results use `appkitmcp.JSONResult(...)` / `appkitmcp.TextResult(...)`.
- **Add `func NewHandler(svc *prompt.Service, rt *appkit.Router) (http.Handler, error)`**:
  ```go
  return appkitmcp.New(appkitmcp.Options{
      Service:       rt.Service(),
      Version:       rt.Version(),
      Instructions:  Instructions,
      Tools:         Tools(svc),
      Health:        rt.Health(),
      Events:        rt.Events(),
      Publishes:     rt.Publishes(),
      Subscriptions: rt.Subscriptions(),
  })
  ```
- **Delete** the JSON-RPC plumbing (`jsonRPCRequest`, `toolCallParams`,
  `ServeHTTP`, `handleToolCall`, `writeJSONRPCResult`/`writeJSONRPCError`,
  `idOrNull`, the local `Identity` type), the `toolDescriptors()` list and the
  `dispatchTool` switch scaffolding (folded into per-tool `Handler`s), the local
  `toolHealth` and its `health` descriptor/dispatch entry, and the local
  `toolResultText`/`toolResultErr`/`toolResultJSON` helpers.

In **`cmd/prompts/main.go`** (`registerRoutes`): replace
`rt.Handle("POST /mcp", rt.RequireIdentity(mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())))`
with:
```go
handler, err := mcp.NewHandler(svc, rt)
if err != nil {
    return err
}
rt.Handle("POST /mcp", rt.RequireIdentity(handler))
```

In **`internal/mcp/mcp_test.go` / `tools_test.go`**: swap the handler
construction to go through `appkit/server.New` (a `Register` hook calling
`NewHandler(svc, rt)`, with `server.Options` threading `Events: prompt.Events`
and `Subscriptions: func() []consumer.Subscription { return consume.Subscriptions(sources) }`
so `reflection` has data), and drive the assembled handler's `ServeHTTP`. Keep
every behavioral assertion (create/get/list/run round-trip, trigger set/clear,
import, `TestErrorMapping`'s `isError` cases, describe, the health envelope).
Update the tool-count test from 17 to 18 (adding `reflection`) — this is
`R-DKQP-QZ3Q`. Add a reflection test — `R-DLYM-4QUF`. `sources` is defined in
`package main`; the mcp-package test declares its own local six-source slice (or
the phase exposes `consume`'s list) to build the subscription expectation.

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-DKQP-QZ3Q** — a clearly-named test asserts `tools/list` through the
  assembled handler returns **exactly eighteen** tools: the sixteen prompts
  domain tools declared by prompts **plus** chassis `health` and `reflection`
  (asserting the full name set, sorted) — a table still declaring its own
  `health` fails `New`'s reserved-name check; a missing chassis tool fails the
  count.
- **R-DLYM-4QUF** — a clearly-named test asserts `reflection` (no arguments)
  through the assembled handler reports `publishes` equal to prompts' two static
  outcome types (`run.succeeded`, `run.failed`) and `subscribes` equal to the six
  upstream in-edges in order (`cron`, `crm`, `ledger`, `dropbox`, `scripts`,
  `prompts`, each filter `"*"`), with no `handler`/`Handler` key leaked.
- the pre-existing `create`/`get`/`list`/`run`/trigger/`import`/error-mapping/
  `describe`/`health` behavioral tests pass through the assembled handler with no
  assertion changes;
- `grep -rn "writeJSONRPCError\|writeJSONRPCResult\|jsonRPCRequest\|func (h \*Handler) ServeHTTP" prompts/internal/mcp --include=*.go`
  returns no matches, and `grep -rn "func Tools\|const Instructions\|func NewHandler" prompts/internal/mcp --include=*.go`
  matches.
