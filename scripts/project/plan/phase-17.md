# Phase 17 — Structured MCP adoption: structured results, output schemas, typed error codes

*Realizes design Decision 19 (structured MCP adoption). Depends on Phase 11
(D13 — the `internal/mcp` declaration surface these results flow through).*

Edit `scripts/internal/mcp/tools.go` in place to conform the sixteen domain
tools to the settled structured-MCP result contract
(`docs/structured-mcp-design.md`), making scripts compile and go green against
the new appkit (`JSONResult` deleted, `StructuredResult`/`ErrorResult`
re-signed). No result is reshaped: the same JSON travels, now as
`structuredContent` under a declared `outputSchema`, with a machine-branchable
error code.

Observable end state:

- The thirteen structured tools (`create`, `import`, `list`, `get`, `update`,
  `delete`, `set_trigger`, `clear_trigger`, `run`, `run_list`, `run_get`,
  `run_cancel`, `run_fs_list`) return `mcp.StructuredResult` — `structuredContent`
  plus a mirrored text block — and each declares a hand-authored `OutputSchema`
  in its `Tool` descriptor, mirroring the emitted JSON **verbatim** (PascalCase
  `Script`/`ScriptDetail`/`Trigger` keys; snake_case `Run`/`FileEntry` keys — see
  D19's casing table). The `StructuredResult` error return is propagated, never
  swallowed.
- The three prose tools (`describe`, `run_output`, `run_fs_read`) keep
  `mcp.TextResult` and declare no `OutputSchema`.
- `toolResultErr` becomes `structuredError(err)`, mapping domain sentinels to
  closed-vocabulary codes: `script.ErrNotFound` → `not_found`,
  `script.ErrValidation` → `validation`, else → `internal`.
- Wire surface stays 18 tools (16 domain + chassis `health`/`reflection`); D13's
  partition and every existing behavioral assertion still hold.

**Done when** the following Verification ids are each covered by a clearly-named
test in `internal/mcp/tools_test.go` (driving the `NewHandler`-assembled
`appkit/mcp` handler over a real `script.Service` on an `appkitdb` temp DB via
`httptest`), and the suite is green per design Conventions
(`cd scripts && go build ./...`, `go vet ./...`, `gofmt -l .` empty, and
`go test ./...` all succeed):

- **R-C0G0-V0QL** — every structured tool's success result carries a
  `structuredContent` object deep-equal to the JSON parsed from its mirrored text
  block (table-driven over the thirteen tools).
- **R-C1NX-8SHA** — `describe`, `run_output`, `run_fs_read` return a text block
  and carry no `structuredContent`.
- **R-C2VT-MK7Z** — `tools/list` shows an `outputSchema` on each of the thirteen
  structured tools and none on the three prose tools (13-present / 3-absent
  partition).
- **R-C43Q-0BYO** — schema fidelity to the wire: `update`'s `outputSchema`
  declares the PascalCase `Script` keys and `run_get`'s declares the snake_case
  `Run` keys, each covering the top-level keys the tool actually emits in
  `structuredContent`.
- **R-C5BM-E3PD** — `create` with empty `name` → `isError` + `code == "validation"`.
- **R-C6JI-RVG2** — `get`/`run_get` with an unknown id → `isError` +
  `code == "not_found"` (not `validation`, not `internal`).
- **R-C7RF-5N6R** — `import` with an injected `Fetcher` returning a plain
  non-sentinel error → `isError` + `code == "internal"` (the default arm).
- **R-CA77-X6O5** — structural: `grep -rn 'JSONResult' internal cmd
  --include='*.go' | grep -v _test.go` returns empty.
