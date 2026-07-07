# Phase 11 — MCP surface over `appkit/mcp`

*Realizes design Decision 10 (the ten-tool table). Depends on phases 9–10 only for
a settled `main.go` (mechanically independent otherwise); depends on the appkit
chassis providing `appkit/mcp` with the standard tools (appkit plan Phases 08–09),
consumed through the committed replace as a fixed external contract. Covers
`R-9NYN-SVIR`.*

Observable end state:

- `gmail/internal/mcp` declares `Instructions`, `Tools(client Client) []mcp.Tool`
  (the ten domain tools: `list`, `read`, `thread`, `labels`, `send`, `draft`,
  `label`, `unlabel`, `trash`, `delete` — each descriptor, schema, and handler
  body unchanged in wire content), and `NewHandler(client Client, rt
  *appkit.Router) (http.Handler, error)` assembling the `appkit/mcp` handler over
  `Instructions` + `Tools(client)` + the Router-threaded
  `Service`/`Version`/`Health`/`Events`/`Publishes`/`Subscriptions`; the gmail
  `Spec.Handlers` mounts `POST /mcp` with it (`mcp.NewHandler(client, rt)`).
- The local JSON-RPC transport (`jsonRPCRequest`, the `ServeHTTP` dispatch,
  `writeJSONRPCError`/`writeJSONRPCResult`, `idOrNull`, the local `Identity`
  type), the local `toolHealth`/`toolReflection`/`renderSubscriptions`/
  `reflectionUnknownTypeError` implementations, the `toolDescriptors()` builder,
  and the local result-envelope helpers (`toolResultText`/`toolResultErr`/
  `toolResultJSON`) are deleted from `gmail/internal/mcp` — replaced by the chassis
  `mcp.TextResult`/`mcp.ErrorResult`/`mcp.JSONResult`. The `Events` registry and
  the `mail.*` payload structs remain (still `Spec.Events`).
- The existing mailbox behavioral tests in `tools_test.go` drive the assembled
  `appkit/mcp` handler and keep their assertions (list paging, read/thread render,
  send/draft RFC-2822 composition, label/unlabel, trash/delete fake-client-only,
  client-error → tool-error mapping, health envelope carrying
  `owner_email`/`client_id`, reflection index/detail/unknown-type); only the
  handler constructor is swapped. The listed surface is **exactly twelve** tools.

**Done when:** the suite is green (design Conventions commands, from `gmail/`) and:

- R-9NYN-SVIR (D10) is covered by a clearly-named test asserting the
  exactly-twelve partition (the ten gmail domain tools declared + chassis
  `health`/`reflection`);
- the pre-existing `list`/`read`/`thread`/`labels`/`send`/`draft`/`label`/
  `unlabel`/`trash`/`delete`, `health`, and `reflection` behavioral tests pass
  through the assembled handler with no assertion changes;
- `grep -rn "writeJSONRPCError\|jsonRPCRequest\|toolDescriptors\|func (h \*Handler) toolHealth\|func (h \*Handler) toolReflection" gmail/internal/mcp --include=*.go`
  returns no matches.
