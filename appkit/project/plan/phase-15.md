# Phase 15 — strict-client conformance: `New` schema guard + `reflection` open-object outputSchema

*Realizes design Decision 8 (advertised-schema conformance guard) and 9
(`reflection` open-object `outputSchema`). Depends on Phase 12 (structured
results / `OutputSchema` surfaced in `tools/list`) and Phase 13 (structured
standard tools).*

All work is in the `appkit/mcp` package. Two coupled changes so no
appkit-advertised schema can carry a top-level `oneOf`/`anyOf`/`allOf` — the
shape strict MCP clients (Claude Code) reject, which took down every service's
tool list in production.

- **`reflection`'s advertised `outputSchema` becomes an open object.**
  `reflectionOutputSchema()` returns `{"type": "object",
  "additionalProperties": true}` instead of the top-level `{"oneOf": [index,
  detail]}`. The handler's two runtime forms and their mirrored
  `structuredContent` are unchanged; only the advertised schema changes. The
  existing `mcp_test.go` assertion that reflection's `outputSchema` is a
  two-form `oneOf` is rewritten to assert the open-object shape (top-level
  `type` `"object"`, `additionalProperties` true, and the absence of any
  `oneOf`/`anyOf`/`allOf` key).
- **`mcp.New` gains a construction-time conformance guard.** A helper
  (e.g. `conformsToStrictClient(schema map[string]any) error`) encodes the
  rule: an advertised schema's top level must be `"type": "object"` and must
  carry no top-level `oneOf`/`anyOf`/`allOf`. `New` runs it over every tool's
  `InputSchema` and, when non-nil, `OutputSchema` — the auto-registered
  `health`/`reflection` descriptors included — and returns a non-nil error
  naming the offending tool and key on a violation, alongside the existing
  duplicate/reserved-name rejections. Nil `OutputSchema` (prose tools) is
  skipped. The guard is a shape check on the advertised schema, not runtime
  validation of results.

The external-contract check — a real strict client fetching the deployed tool
list — is **not** built here: it is an operator step (`plan/README.md`
§ Operator steps), verified after the suite-wide redeploy, deliberately absent
from `STATUS.md` (and from this phase's id coverage) so the loop stays
convergent.

**Done when:** the `appkit` suite is green per design *Conventions* (from
`appkit/`: `go build ./...`, `go vet ./...`, `gofmt -l .` empty, `go test
./...` all pass) with both ids covered by clearly-named, genuinely-asserting
tests:

- R-EIYD-4M57 — a `New` test proves construction fails for a tool whose
  `InputSchema` or non-nil `OutputSchema` has a top-level `type` other than
  `"object"` (including absent) or a top-level `oneOf`/`anyOf`/`allOf`, and
  succeeds for a conforming table; a case reinstating `reflection`'s top-level
  `oneOf` (or an equivalent standard-tool violation) is shown to fail `New`.
- R-EK69-IDVW — a `tools/list` test proves the `reflection` descriptor's
  `outputSchema` has top-level `"type": "object"` with `additionalProperties`
  true and **no** `oneOf`/`anyOf`/`allOf` at any level.
