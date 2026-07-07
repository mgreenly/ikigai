# Phase 11 — MCP surface over `appkit/mcp`

*Realizes design Decision 13. Touches `scripts/internal/mcp/` (reshape) and
`cmd/scripts/main.go` (the `mcp.NewHandler(svc, rt)` call site + the `Spec.Health`
reporter), all inside `scripts/`. Depends on Phases 09–10 for a settled `main.go`
(mechanically independent otherwise); depends on the appkit chassis providing
`appkit/mcp` with the standard tools (appkit plan Phases 08–09), consumed through the
committed replace as a fixed external contract.*

Observable end state:

- `scripts/internal/mcp` declares `Instructions` (byte-identical to the current
  `initialize` instructions string), `Tools(svc *script.Service) []mcp.Tool` (the
  **sixteen** domain tools — `describe`, `create`, `import`, `list`, `get`,
  `update`, `delete`, `set_trigger`, `clear_trigger`, `run`, `run_list`, `run_get`,
  `run_output`, `run_cancel`, `run_fs_list`, `run_fs_read` — each with its
  descriptor, `inputSchema`, and existing dispatch body unchanged in wire content),
  and `NewHandler(svc *script.Service, rt *appkit.Router) (http.Handler, error)`
  assembling `appkitmcp.New(Options{Service, Version, Instructions, Tools, Health,
  Events, Publishes, Subscriptions})` from the Router accessors. `cmd/scripts`
  mounts `POST /mcp` with `rt.RequireIdentity(handler)`.
- The local JSON-RPC transport (`ServeHTTP`, `handleToolCall`, `jsonRPCRequest`,
  `toolCallParams`, `writeJSONRPCResult`/`writeJSONRPCError`/`idOrNull`), the local
  `Identity` type, the local `toolHealth`/`runtimeContract`, and the local
  result-envelope helpers (`toolResultText`/`toolResultErr`/`toolResultJSON`) and the
  `toolDescriptors()`/`desc`/`obj`/`typ` map form are **deleted** from
  `scripts/internal/mcp` (folded into the `[]mcp.Tool` literal and the chassis
  `mcp.TextResult`/`JSONResult`/`ErrorResult`). The `paramError` → `-32602` behavior
  is preserved by the handler returning that error (the chassis renders it).
- The static runtime contract (`python_version`/`bash_version`/`network`/`packages`)
  moves to `cmd/scripts`'s `Spec.Health` reporter, feeding **both** the HTTP
  `/health` route and the chassis MCP `health` tool.
- **Wire surface: 18 tools** — the 16 domain tools (unchanged bytes) plus chassis
  `health` and `reflection`. `reflection` is **newly present** (chassis-owned;
  scripts had none); `health`'s descriptor text becomes the chassis generic
  wording, while its result still carries the runtime contract (via `Spec.Health`)
  plus `owner_email`/`client_id`.

## Reshape `internal/mcp`

Keep `describe.go` (`describeText`/`toolDescribe`) as-is (describe is a domain
tool). Convert `toolDescriptors()` + `dispatchTool` into `Tools(svc) []mcp.Tool`:
each existing `desc(...)` becomes a `mcp.Tool{Name, Description, InputSchema, Handler}`
with the **same** name/description/schema, and each `dispatchTool` case body becomes
that tool's `Handler func(ctx, args json.RawMessage, id server.Identity)
(map[string]any, error)` — owner is `id.OwnerEmail`, argument decoding via the
existing `parseArgs`/`configInput` helpers, results via `mcp.JSONResult`/`TextResult`
and errors returned (chassis maps to the JSON-RPC error / `isError`). **Do not**
declare `health` or `reflection` (reserved — `New` rejects them).

## `cmd/scripts/main.go` — health reporter + handler assembly

- Add the `Spec.Health` reporter returning the runtime contract:
  ```go
  Health: func(ctx context.Context) (map[string]any, error) {
      return map[string]any{
          "python_version": ">=3.11", "bash_version": ">=5.0",
          "network": true, "packages": "stdlib",
      }, nil
  },
  ```
- In `registerRoutes`, replace the old constructor call with the assembled handler:
  ```go
  handler, err := mcp.NewHandler(svc, rt)
  if err != nil { return err }
  rt.Handle("POST /mcp", rt.RequireIdentity(handler))
  ```

## Test edits (assertions unchanged; constructor swapped)

Rebuild `internal/mcp/tools_test.go`'s `newTestHandler`/`newTestHandlerWithFetcher`
to drive the **assembled** appkit/mcp handler, the crm/notify way: `appkitdb.Open` +
`appkitdb.LoadMigrations(db.FS,…)` + `appkitdb.Migrate` for the DB (this anticipates
D14's shim deletion — do not use `db.Open`/`db.Migrate`), build the domain
`script.Service`, then either call `mcp.NewHandler(svc, rt)` over a Router captured
from `server.New` (with `Health` set to the runtime-contract reporter and
`Events: script.Events`) or call `appkitmcp.New(Options{…})` directly with those
values. The `call`/`do`/`resultText`/`isError` helpers still drive `ServeHTTP` on the
returned handler. Keep every behavioral assertion: `TestCreateGetList`,
`TestRunSpawns`, `TestSetTriggerValidAndInvalid`, `TestRunOutputAndFs`,
`TestImportDispatch`, `TestErrorMapping`, `TestDescribeNonEmpty`, and
`TestHealthDetailsContract` (health details still carry the runtime contract +
identity through the assembled handler).

**`TestToolsListReturns17` is REPLACED** by the R-91IM-JYPA 18-tool test below.

## Add the D13 verification coverage (id-tagged)

- **R-91IM-JYPA** — `tools/list` through the assembled handler returns **exactly 18**
  tools: the 16 domain tools (assert each name present) **plus** `health` and
  `reflection` (chassis). Assert `describe` is present and `reflection` is present
  (it was absent pre-conversion). A table still declaring its own `health`/`reflection`
  fails `New`'s reserved-name check; a missing standard tool fails the count. Tag
  `// R-91IM-JYPA`. (Replaces `TestToolsListReturns17`.)
- **R-92QI-XQFZ** — the runtime contract is served from the single `Spec.Health`
  reporter through **both** surfaces: (a) the MCP `health` tool result's `details`
  carries `python_version`/`bash_version`/`network`/`packages` (plus `owner_email`/
  `client_id` on the envelope), and (b) the HTTP `GET /health` route's `details`
  carries the same four keys. Drive (b) via the appkit server `/health` route (or the
  shared `appkit.Envelope`/reporter the chassis wires from `Spec.Health`). Tag
  `// R-92QI-XQFZ`.

## Done when

The suite is green (design *Conventions* commands, from `scripts/`, plus
`bin/check-migrations scripts`) with zero failures, **and**:

- **R-91IM-JYPA** (exactly-18 partition) and **R-92QI-XQFZ** (health reporter feeds
  both surfaces) are covered by clearly-named, id-tagged tests.
- The pre-existing `internal/mcp` behavioral tests pass through the assembled handler
  with **no assertion changes**.
- `grep -rn "writeJSONRPCError\|writeJSONRPCResult\|jsonRPCRequest\|func (h \*Handler) ServeHTTP\|runtimeContract" scripts/internal/mcp --include=*.go`
  returns no matches.
- `grep -rn "TestToolsListReturns17" scripts --include=*.go` returns no matches.
