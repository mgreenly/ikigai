# Phase 9 — MCP surface over `appkit/mcp`

*Realizes design Decision 11 (the seven-domain-tool table). Depends on Phases
07–08 only for a settled `main.go` (mechanically independent otherwise); depends
on the appkit chassis providing `appkit/mcp` with the standard `health`/`reflection`
tools, consumed through the committed replace as a fixed external contract. **Read
D11 for the kept/deleted split and the two accepted untested wire changes.***

Observable end state:

- `ledger/internal/mcp` declares `Instructions`, `Tools(svc *ledger.Service)
  []mcp.Tool` (the seven domain tools — `record`, `reverse`, `reconcile`,
  `balance`, `register`, `get`, `describe` — descriptors, schemas, and handler
  bodies unchanged in wire content), and `NewHandler(svc, rt) (http.Handler,
  error)` assembling the `appkit/mcp` handler over the Router-threaded
  Service/Version/Health/Events/Publishes/Subscriptions; `cmd/ledger/main.go`
  mounts `POST /mcp` with it (`h, err := mcp.NewHandler(svc, rt)`), replacing the
  old six-arg `mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health(),
  rt.Events(), rt.Subscriptions())` call.
- Deleted from `ledger/internal/mcp`: the JSON-RPC transport (`ServeHTTP`,
  `jsonRPCRequest`, `toolCallParams`, `handleToolCall`, `writeJSONRPCResult`/
  `writeJSONRPCError`, `idOrNull`), the local `Identity` type, the local
  result-envelope helpers (`toolResultText`/`toolResultErr`/`toolResultJSON` →
  chassis `appkitmcp.TextResult`/`ErrorResult`/`JSONResult`), and the local
  `health`/`reflection` implementations (`toolHealth`, `toolReflection`,
  `renderSubscriptions`, `reflectionUnknownTypeError`). The domain tool bodies,
  schema helpers, date/period parsing, `transactionJSON`, and `translateLedgerError`/
  `jsonEscape` stay.
- The behavioral tests (`TestRecordGetReverseReconcile_EndToEnd`,
  `TestRecord_ErrorsSurfaceAsToolErrors`, `TestBalance_PeriodBucketAndRange`,
  `TestReflection`, `TestHealth`) drive the assembled `appkit/mcp` handler (built
  over a migrated in-memory `ledger.Service`, via appkit's `server.New`/`Register`
  seam like notify's harness) and keep their assertions unchanged; the tool count
  stays nine.

**Done when:** the suite is green (design Conventions commands, from `ledger/`) and:

- R-52PA-OE6N (D11) is covered by a clearly-named test asserting `tools/list`
  returns **exactly nine** tools — the seven ledger domain verbs declared by ledger
  **plus** chassis `health`/`reflection` — proving the table/chassis partition (a
  table that still declares its own `health`/`reflection` fails `appkitmcp.New`'s
  reserved-name check; a surface missing them fails the count). The existing
  `TestToolsList_HasNine` is rewired to this end and re-tagged.
- the pre-existing domain/reflection/health behavioral tests pass through the
  assembled handler with no assertion changes;
- `grep -rn "writeJSONRPCError\|jsonRPCRequest\|func (h \*Handler) ServeHTTP" ledger --include=*.go | grep -v project/`
  returns no matches (the local transport is gone);
- `grep -n "func (h \*Handler) toolHealth\|func (h \*Handler) toolReflection\|func renderSubscriptions" ledger/internal/mcp/*.go`
  returns no matches (local health/reflection deleted — the chassis owns them).
