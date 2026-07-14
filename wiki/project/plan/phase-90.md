# Phase 90 — Structured MCP adoption in `internal/mcp`

*Realizes design Decision 61 (structured MCP: `StructuredResult`, typed error
codes, per-tool output schemas). Depends on Phase 80 (the `appkit/mcp` tool-table
surface) and Phase 83 (the `guide` prose tool).*

Bring `wiki/internal/mcp` into conformance with the suite structured-MCP contract
(`../../docs/structured-mcp-design.md`), making the service compile and go green
against the current appkit (which deleted `JSONResult` and re-signed
`ErrorResult(code, msg)`). The observable end state, all within
`wiki/internal/mcp` (the transport and `health`/`reflection` stay chassis-owned):

- Every domain handler's success path returns `appkitmcp.StructuredResult(v)`
  instead of the deleted `JSONResult(v)` — the same `public*Result`/`askToolResult`
  payload `v`, now carried as `structuredContent` **and** a mirrored text block.
- The `toolError(text)` helper is replaced by code-carrying helpers over
  `appkitmcp.ErrorResult(code, msg)`; every failure site is classified per D61's
  closed mapping — `validation` (bad/missing/malformed args, bad time/cursor,
  same-subject merge, missing identity), `not_found` (unknown `job_id` on
  `status`/`abort`/`rerun`; unknown subject path on `merge`/`claims`/`page` — the
  former `{found:false,kind,id}` sentinel becomes a typed `not_found` error), and
  `internal` (opaque domain errors and the "not configured" guards).
- Each of the 13 domain `*Tool()` descriptors declares an `outputSchema` literal
  mirroring its emitted payload (D61's table); `domainTool` plumbs
  `desc["outputSchema"]` into `Tool.OutputSchema`. `guide` remains
  `TextResult`-only with no `outputSchema` (the sole prose exception).
- The `internal/mcp` behavioral tests (incl. Phase 83's D57 neutrality test, now
  narrowed by D57's in-place rewrite to names/inputs/validation/result-fields) are
  updated to the structured envelopes.

## Done when

Every id below is covered by a clearly-named test driving the assembled
`appkit/mcp` handler (`NewHandler` over stubbed domain services + stubbed
identity), and the suite is green per design Conventions (`go build ./...`,
`go vet ./...`, `gofmt -l .` empty, `go test ./...` all pass):

- **R-EMCA-BCU2** — a domain success result (`ingest`) carries `structuredContent`
  equal to the emitted value **and** a `content[0].text` mirror of the same JSON.
- **R-ENK6-P4KR** — all 13 domain tools surface a non-nil `outputSchema` in
  `tools/list`; `guide` surfaces none (table-driven).
- **R-EPZZ-GO25** — `tools/call guide` returns a plain text result (non-empty,
  `content[0].type=="text"`, no `structuredContent`).
- **R-ER7V-UFSU** — each domain tool's `outputSchema.properties` covers the keys of
  a representative success `structuredContent` (and its `required` fields are all
  emitted), table-driven across the 13 tools incl. nested item/`citations` shapes.
- **R-ESFS-87JJ** — input-validation triggers (malformed args, missing required
  field, non-RFC3339 `since`/`until`, invalid `cursor`, same-subject `merge`) each
  return `isError` with `structuredContent.code == "validation"` (table-driven).
- **R-ETNO-LZA8** — unknown `job_id` (`status`/`abort`/`rerun`) or unknown subject
  path (`page`/`claims`/`merge`) returns `isError` with `code == "not_found"`, not
  a `{found:false}` success (table-driven).
- **R-EW3H-DIRM** — an injected opaque domain failure returns `isError` with
  `code == "internal"`.
- **R-EXBD-RAIB** — every error envelope carries `isError == true`, the human
  message in `content[0].text`, and a `structuredContent.code` from the closed
  vocabulary.

And the structural cutover holds:
`grep -rn 'JSONResult' internal cmd --include='*.go' | grep -v _test.go` returns
empty (no `JSONResult` reference in non-test service source).
</content>
