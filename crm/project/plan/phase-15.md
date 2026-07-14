# Phase 15 — Structured MCP adoption: `structuredContent`, `outputSchema`, typed error codes

*Realizes design Decision 19 (structured MCP adoption). Depends on Phase 09 (the
`appkit/mcp` tool-table seam) and Phase 13's `tools_test.go` harness. Covers
`R-5Y60-E30A`, `R-5ZDW-RUQZ`, `R-60LT-5MHO`, `R-61TP-JE8D`, `R-631L-X5Z2`,
`R-65HE-OPGG`.*

> **⛔ EXTERNAL ORDERING — operator-sequenced.** This phase consumes the **new
> appkit MCP contract** (`StructuredResult(v) (map[string]any, error)`,
> `Tool.OutputSchema`, `ErrorResult(code ErrorCode, msg string)` over the closed
> vocabulary; `JSONResult` **deleted**) built by appkit's own plan. crm does not
> compile against the current appkit until this phase lands; appkit's revision
> must be green in the tree before this phase runs.

All work is confined to `crm/internal/mcp/` (`tools.go` + `tools_test.go`).

Observable end state:

- The five domain verb handlers in `internal/mcp/tools.go` (`toolSearch`,
  `toolGet`, `toolSave`, `toolDelete`, `toolLog`) return their success value
  through `appkitmcp.StructuredResult(v)` instead of the deleted
  `appkitmcp.JSONResult(v)`; the helper's `(map[string]any, error)` marshal error
  is propagated up each handler's existing `(map[string]any, error)` return, not
  swallowed. The emitted values are byte-for-byte the same JSON: search
  `{items, next_cursor}`, get the `Card`, save/log a `Summary`, delete `{ok:true}`.
- Each domain `mcp.Tool` declares an `OutputSchema` literal (D19): `search` →
  `obj({items: array<summarySchema>, next_cursor}, "items")`; `get` → open object
  `{type:object, additionalProperties:true}`; `save`/`log` → `summarySchema()`;
  `delete` → `obj({ok:{type:boolean}}, "ok")`. A small private `summarySchema()`
  helper renders the shared `Summary` shape (`id`/`type`/`label`/`updated_at`
  required, open `fields`). `guide` declares **no** `OutputSchema`.
- `errorEnvelope`/`toolErr` are replaced by a mapping onto the appkit `ErrorCode`
  closed vocabulary, returning `appkitmcp.ErrorResult(code, msg)`: `*DuplicateError`
  → `ErrConflict` (message retains its `existing_id=` text); `*ValidationError`
  and `errors.Is(ErrValidation)` → `ErrValidation`; `errors.Is(ErrNotFound)` →
  `ErrNotFound`; `errors.Is(ErrConflict)` → `ErrConflict`; default → `ErrInternal`
  with `"internal error"`. The nested `{"error":{…}}` JSON-in-a-string envelope is
  removed.
- `tools_test.go` conforms its assertions to the new encoding (structured success
  results, per-tool output schemas, closed-vocabulary error codes) at the same
  composition-root seam it already drives.

**Done when:** the suite is green (design Conventions commands, run from `crm/`:
`go build ./...`, `go vet ./...`, `gofmt -l .` empty, `go test ./...`) and:

- R-5Y60-E30A — a `tools/list` test asserts a non-nil `type:object` `outputSchema`
  on each of `search`/`get`/`save`/`delete`/`log` and its absence on `guide`
  (table-driven over the six crm tools).
- R-5ZDW-RUQZ — a `tools/call` test asserts each domain verb's success result has
  `structuredContent` deep-equal to the JSON parsed from its own `content[0].text`
  and matching the verb's shape (`delete` → `{ok:true}`; `save`/`log` → a
  `Summary`; `search` → `{items,next_cursor}`), driven over a real migrated DB.
- R-60LT-5MHO — an error-path test asserts every domain-verb error result sets
  `isError:true`, carries a `structuredContent.code` in the closed vocabulary,
  and has no top-level `error` key and no `duplicate` code.
- R-61TP-JE8D — a test inserts a colliding live row, then a `save` create with
  `force` false returns `code == "conflict"` and a message containing
  `existing_id=`.
- R-631L-X5Z2 — a test asserts `get` and `delete` of an unresolvable id return
  `code == "not_found"`.
- R-65HE-OPGG — a test asserts a decode/validation-rejected `save` (client-set
  deal `status`, or a create missing a required field) returns
  `code == "validation"`.
- `grep -rn 'JSONResult' internal cmd --include='*.go' | grep -v _test.go` returns
  empty (the deleted symbol is gone from non-test source).
