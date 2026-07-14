# Phase 14 — Structured MCP adoption on the domain tool surface

*Realizes design Decision 16 (structured MCP adoption). Depends on Phase 09
(the `appkit/mcp` tool table) and Phase 12 (`external_ref`/`ErrDuplicateRef`).
Externally gated on appkit phases 12–14 (the re-signed `StructuredResult`/
`ErrorResult`, `Tool.OutputSchema`, and the deleted `JSONResult`), which are
built and green.*

ledger's `internal/mcp` surface conforms to the suite's single verb surface
(`docs/structured-mcp-design.md`, cited by D16). One package, two files —
`internal/mcp/tools.go` and `internal/mcp/mcp.go` — plus their tests. The
emitted JSON of every tool is preserved verbatim; only the transport envelope
changes.

Observable end state:

- The six domain **result** verbs (`record`, `reverse`, `reconcile`, `balance`,
  `register`, `get`) return `appkitmcp.StructuredResult(v)` over the same value
  they marshalled before, propagating (never swallowing) its returned error, so
  each success result carries `structuredContent` plus a mirrored text block.
- Each of those six verbs declares a hand-authored `outputSchema` literal (house
  style: the `objectSchema`/`obj` helper pattern) mirroring the JSON it emits;
  `record`/`reverse`/`get` share one `transactionSchema()` helper, `reconcile`
  wraps it. `amount_cents`/`total`/`running_total` are `integer`; present-when-
  set keys (`reverses_id`, `reversed_by_id`, `external_ref`) are non-required.
- `describe` stays a **prose exception**: it returns `TextResult` over its
  marshalled discovery payload — no `structuredContent`, no `outputSchema`.
- `translateLedgerError` is re-signed to `func(error) map[string]any`, returning
  `appkitmcp.ErrorResult(code, msg)` with the closed-vocabulary mapping in D16's
  table (`ErrUnbalanced`/`ErrBadRoot`/`ErrValidation` → `validation`;
  `ErrNotFound` → `not_found`; `ErrAlreadyReversed`/`ErrDuplicateRef` →
  `conflict`; unmapped → `internal`). `jsonEscape` and every hand-built
  `{"error":{"code":…}}` string envelope are deleted; each call site becomes
  `return translateLedgerError(err), nil`.
- The D11/D14 tests that pinned the pre-adoption error bytes (the `duplicate_ref`
  envelope in the phase-12 `external_ref` tests, any `JSONResult`-shaped success
  assertion) are updated to the D16 wire shape (`structuredContent.code ==
  "conflict"`, structured success). No frozen `project/` doc is edited.

**Done when:**

- Every D16 Verification id is covered by a clearly-named test through the
  assembled `appkit/mcp` handler (`NewHandler` over a migrated in-memory
  `ledger.Service`), driven via `tools/list`/`tools/call`:
  - R-9FRN-SGDT — `tools/list` declares a non-nil `outputSchema` for each of the
    six domain result verbs and none for `describe` (table-driven over
    descriptors).
  - R-9GZK-684I — `record`/`reverse`/`get` success `structuredContent` is the
    full transaction object (`amount_cents` a JSON number) and equals the text
    block's parsed JSON (mirror).
  - R-9I7G-JZV7 — `balance` success `structuredContent` equals
    `{lines:[{account,amount_cents}],total,unit}` (numbers), mirrored in text.
  - R-9JFC-XRLW — `register` success `structuredContent` equals
    `{lines:[{txn_id,date,description,posting_id,account,amount_cents,status,
    running_total}],unit}` (numbers), mirrored in text.
  - R-9KN9-BJCL — `reconcile` success `structuredContent` equals
    `{transactions:[<transaction>…]}`, mirrored in text.
  - R-9LV5-PB3A — `describe` `tools/call` result has the marshalled payload in
    the text block and **no** `structuredContent` key.
  - R-9N32-32TZ — unbalanced `record` → `isError:true`, code `validation`.
  - R-9OAY-GUKO — bad account root → code `validation`, message names the five
    roots / points at `describe`.
  - R-9PIU-UMBD — `get` of an unknown id → code `not_found`.
  - R-9QQR-8E22 — reverse of an already-reversed transaction → code `conflict`.
  - R-9RYN-M5SR — duplicate `external_ref` → code `conflict`, message names the
    existing transaction id.
  - R-9T6J-ZXJG — each input rejection (too few postings, multiple elisions, bad
    date, unknown status, empty `external_ref`, malformed period) → code
    `validation` (table-driven).
  - R-9UEG-DPA5 — every domain tool error's `structuredContent.code` is one of
    the closed set, `isError` is `true`, message non-empty (table-driven scan).
- The suite is green per design Conventions: `cd ledger && go build ./...`,
  `cd ledger && go vet ./...`, `cd ledger && gofmt -l .` (no output), and
  `cd ledger && go test ./...` all succeed.
- Structural greps (scoped to source, `project/`-excluded) return empty:
  - `grep -rn 'JSONResult' ledger/internal ledger/cmd --include='*.go' | grep -v _test.go`
  - `grep -rn 'jsonEscape' ledger/internal ledger/cmd --include='*.go' | grep -v _test.go`
  - `grep -rn '"error":{"code"' ledger/internal ledger/cmd --include='*.go' | grep -v _test.go`
