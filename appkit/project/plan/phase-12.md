# Phase 12 — Structured results in the `appkit/mcp` transport

*Realizes design Decision 8 (revised — the structured-result contract: its new
ids R-WPNN-6Q9E, R-WQVJ-KI03, R-WTBC-C1HH, R-WUJ8-PT86, R-WVR5-3KYV; the
built ids R-MCJJ-NXJR, R-MDRG-1PAG, R-MEZC-FH15, R-MG78-T8RU, R-MHF5-70IJ,
R-MIN1-KS98, R-MJUX-YJZX stay covered by Phase 08's tests, updated in place
where signatures moved). Depends on Phase 08 (the transport this revises).
⛔ Externally ordered: this phase deletes `JSONResult` and re-signs
`ErrorResult`, so the *services* do not compile until their own adoption
phases convert them — a deliberate coordinated cutover
(`docs/structured-mcp-design.md`); appkit's own module suite is this phase's
green bar and builds independently of the services.*

Observable end state, all in `appkit/mcp`:

- `protocolVersion` is `"2025-06-18"`; `initialize` answers it.
- `Tool` gains `OutputSchema map[string]any`; `tools/list` descriptors carry
  `outputSchema` verbatim when non-nil and omit the key when nil.
- `StructuredResult(v any) (map[string]any, error)` exists and returns
  `{content: [one text block = compact JSON marshal of v],
  structuredContent: v}`; `JSONResult` is deleted (the internal
  `jsonResultFrom` seam converts to the structured form for the standard
  tools).
- `type ErrorCode string` with the six constants (`validation`, `not_found`,
  `conflict`, `too_large`, `source_unavailable`, `internal`);
  `ErrorResult(code ErrorCode, msg string)` returns
  `{content: [text msg], isError: true,
  structuredContent: {code, message}}`.
- `dispatchTool`/`handleToolCall` map a Go error from a declared tool's
  Handler to JSON-RPC `-32603`; an undeclared tool name and malformed
  `tools/call` params stay `-32602`; `-32700`/`-32601` unchanged.
- `TextResult` unchanged. No runtime validation of results against
  `OutputSchema`.

**Done when:** the suite is green — `cd appkit && go build ./...`,
`go vet ./...`, `gofmt -l .` (no output), and `go test ./...` all succeed with
zero failures — and:

- R-WPNN-6Q9E, R-WQVJ-KI03, R-WTBC-C1HH, R-WUJ8-PT86, R-WVR5-3KYV are each
  covered by a clearly-named test through the real `ServeHTTP` seam (the
  result-shape ids compare `structuredContent` against the parsed text block,
  never a string fixture);
- the pre-existing D8 id tests (R-MCJJ-NXJR … R-MJUX-YJZX) still pass, updated
  only where the `ErrorResult` signature or the protocol constant moved;
- `grep -rn "JSONResult" --include="*.go" .` run from `appkit/` prints
  nothing (the `project/` docs are not `.go` files, so the check is not
  self-referential).
