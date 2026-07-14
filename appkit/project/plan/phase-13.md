# Phase 13 — Structured standard tools (`health` + `reflection`)

*Realizes design Decision 9 (revised — the standard tools conform to the
structured contract: its new ids R-WWZ1-HCPK, R-WY6X-V4G9, R-WZEU-8W6Y; the
built ids R-ML2U-CBQM, R-7EK6-8030, R-7FS2-LRTP, R-7GZY-ZJKE, R-7I7V-DBB3 stay
covered by the Phase 09/11 tests, updated in place where result construction
moved). Depends on Phase 12 (`StructuredResult`, `ErrorCode`,
`Tool.OutputSchema`).*

Observable end state, all in `appkit/mcp`:

- The `health` tool returns via `StructuredResult` (envelope +
  `owner_email`/`client_id`), and its `tools/list` descriptor declares an
  `outputSchema` naming `status`, `service`, `version`, `owner_email`,
  `client_id`, and an open-object `details`.
- The `reflection` tool returns both forms (index and kind-detail) via
  `StructuredResult`, and its descriptor declares a `oneOf` `outputSchema`
  covering both shapes; the index/detail semantics over the family registry
  are unchanged.
- The unknown-kind path returns
  `ErrorResult(ErrValidation, <msg naming declared kinds>)` — still built from
  the typed `*outbox.UnknownKindError`.

**Done when:** the suite is green (design Conventions: `go build`, `go vet`,
`gofmt -l .` empty, `go test`, from `appkit/`) — and:

- R-WWZ1-HCPK, R-WY6X-V4G9, R-WZEU-8W6Y are each covered by a clearly-named
  test through the D8 `ServeHTTP` seam with real family-shaped
  `outbox.Registry` values (result-shape assertions compare
  `structuredContent` to the parsed text block; descriptor assertions check
  `outputSchema` presence on both standard tools);
- the pre-existing D9 id tests (R-ML2U-CBQM, R-7EK6-8030, R-7FS2-LRTP,
  R-7GZY-ZJKE, R-7I7V-DBB3) still pass, updated only where result
  construction moved to the structured helpers.
