# Phase 08 — MCP surface over `appkit/mcp`

*Realizes design Decision 10 (the crontab tool table). Depends on Phase 06 for a
settled inline `main.go` (mechanically independent of Phase 07 otherwise); depends
on the appkit chassis providing `appkit/mcp` with the standard tools (a fixed
external contract via the committed replace).*

Observable end state:

- `cron/internal/mcp` declares `Instructions`, `Tools(store *crontab.Store)
  []mcp.Tool` (the five domain tools: `create`, `list`, `get`, `update`, `delete`
  — descriptors, schemas, and handlers unchanged in wire content), and
  `NewHandler(store, rt) (http.Handler, error)` assembling the `appkit/mcp` handler
  over `Instructions` + `Tools(store)` + the Router-threaded
  `Service`/`Version`/`Health`/`Events`/`Publishes`/`Subscriptions`;
  `cmd/cron/main.go` mounts `POST /mcp` with it.
- The local JSON-RPC transport (`jsonRPCRequest`, the `ServeHTTP` dispatch, the
  JSON-RPC error/result writers, `handleToolCall`), the local `Identity` type, the
  local `toolHealth`/`toolReflection` (and `renderSubscriptions`/
  `reflectionUnknownTypeError`), and the local result-envelope helpers are deleted
  from `cron/internal/mcp`; the descriptor helpers, expr validation
  (`validateExpr`/`parseError`), the typed `errorEnvelope`/`toolErr` mapping, and
  `renderEntry` are kept (now producing `mcp.Tool` values and using the chassis
  result helpers).
- The live `cron.<name>` reflection is preserved by threading
  `mcp.Options.Publishes: rt.Publishes()` (the `cronSpec()` `event.Publishes(store)`
  provider); `Events` stays empty.
- `internal/mcp/tools_test.go`'s harness builds the assembled `appkit/mcp` handler
  over a `server.Router` (opening the crontab store on a temp DB **via
  `appkit/db` directly**, not the soon-deleted `internal/db` wrappers) and keeps
  its behavioral assertions (bad-expr rejected naming `hour`, create→list→get
  round-trip, reflection `publishes` shows the live `cron.<name>`); the tool count
  is seven.

**Done when:** the suite is green — `cd cron && go build ./...`,
`cd cron && go vet ./...`, `cd cron && gofmt -l .` (no output),
`cd cron && go test ./...`, and `bin/check-migrations cron` all succeed with zero
failures — and:

- R-LS2J-73T5 (D10) is covered by a clearly-named test asserting the
  exactly-seven partition (`create`/`list`/`get`/`update`/`delete` declared +
  chassis `health`/`reflection`);
- the pre-existing crontab behavioral tests pass through the assembled handler
  with no assertion changes;
- `grep -rn "writeJSONRPCError\|jsonRPCRequest" cron/internal/mcp --include=*.go`
  returns no matches.
