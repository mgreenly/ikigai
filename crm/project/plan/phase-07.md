# Phase 07 — Self-describing MCP discovery surface

*Realizes design Decision 9 (self-routing `instructions`), 10 (lean tool
descriptions), and 11 (the `guide` tool + embedded document). Depends on the
pre-existing `internal/mcp` transport (`mcp.go` `initialize`/`tools/list`/
`tools/call`, `tools.go` `toolDescriptors`/`dispatchTool`) and its
`tools_test.go` harness — no earlier plan phase.*

All work is in the existing `crm/internal/mcp/` package; no new package, no
migration, no behavior change to any domain verb.

Observable end state:

- **`initialize` instructions (D9).** The `initialize` response's `instructions`
  string is the D9 text: it names crm's entities with the user-facing synonyms
  (`companies`, `people`, `pipeline`/`opportunities`, …), states the
  search → get → save → log flow, and points at `guide` before a first `save`.
- **Lean `save` (D10).** The `saveDescription` const no longer carries the
  per-type field catalog (`Fields by type`, `given_name`, `amount_cents` gone),
  while still stating the upsert / `force` dedup / declarative-set-replacement
  contract and directing interactions to `log`. The `save` `inputSchema` and all
  domain behavior are unchanged; the other tool descriptions are untouched.
- **The `guide` tool (D11).** A new `internal/mcp/guide.md` is `//go:embed`ed and
  returned by a new flat, input-free `guide` tool: it is listed in
  `tools/list` (bringing the surface to eight tools) and, when called, returns the
  usage document — the entity model, the per-type field catalog relocated from
  `save`, and Basics + Advanced worked examples. `toolGuide()` reads no DB and
  never errors. `tools_test.go`'s exact-count assertion is updated from seven to
  eight.

Every existing `tools/call` verb still returns exactly what it did before this
phase (the reflection/verb/error-envelope tests remain green unchanged).

**Done when:** the suite is green — `cd crm && go build ./...`,
`cd crm && go vet ./...`, `cd crm && gofmt -l .` (no output),
`cd crm && go test ./...`, and `bin/check-migrations crm` all succeed with zero
failures — and every id below is covered by a clearly-named test in
`internal/mcp` that genuinely asserts the behavior its D9–D11 Verification line
describes, each driven through the real `ServeHTTP` JSON-RPC seam:

- R-PDZ7-HTAN, R-PF73-VL1C (D9 — `initialize` instructions)
- R-PGF0-9CS1, R-PIUT-0W9F (D10 — lean `save` description)
- R-PK2P-EO04, R-PLAL-SFQT, R-PMII-67HI (D11 — the `guide` tool + document)

Specifically reachable checks: an `initialize` assertion finds `companies`,
`people`, `pipeline`/`opportunities`, and `guide` in `instructions`; a
`tools/list` assertion finds no `Fields by type`/`given_name`/`amount_cents` in
`save.description` but does find its retained-semantics markers, and finds a
`guide` tool with the total count equal to eight; a `tools/call name:"guide"`
with empty arguments returns a non-error text result containing `given_name`,
`amount_cents`, `stage`, and at least one example `save`/`log` call.
