# Phase 11 — MCP input schemas are valid JSON Schema (no `required: null`)

*Realizes design Decision 10 (the MCP tool surface `internal/mcp` + identity), covering the new id R-N4KO-2WTZ. Depends on Phase 10 (the conformant eight-verb surface and its `tools/list` declarations).*

Phase 10 brought the eight verbs into name/wiring conformance and asserted the
**set** of tool names (R-MUQ4-K1JS), but it did not assert that each tool's
`inputSchema` is well-formed JSON Schema. The shared object-schema helper in
`internal/mcp` writes `"required": <slice>` unconditionally, so `subjects` — the
one verb with no required field — passes a nil slice and serializes
`"required": null`. That is invalid JSON Schema: a strict MCP client (Claude's)
rejects the **entire** `tools/list` ("tools fetch failed"), so all eight verbs
disappear from the client even though the JSON-RPC response is otherwise correct
and the loopback server is healthy. This phase makes every tool's `inputSchema`
valid and locks it with a test, closing the gap that let a null `required`
through Phase 10's set-only check.

**What gets built (the observable end state):**

- The `internal/mcp` object-schema helper **omits** the `required` key when there
  are no required properties, rather than emitting `"required": null`. A tool
  with required properties still carries `required` as a non-empty array; the
  only change is that an empty/nil required set produces a schema with **no**
  `required` key.
- Concretely, the `subjects` tool's declared `inputSchema` no longer contains
  `required` at all; the other seven verbs are unchanged (`health`/`reflection`
  already have no `required`; the five with required fields keep their arrays).
- `tools/list` is accepted by a strict JSON-Schema-validating client: no tool in
  the surface emits `"required": null`.

**Done when:**

- R-N4KO-2WTZ — a test over the full `tools/list` surface asserts that **every**
  tool's `inputSchema` has `type: "object"` and that its `required` field is
  either absent or a non-empty array of strings — never `null` (and never an
  empty array). It specifically asserts the `subjects` schema omits `required`.
- The existing D10 ids stay green — R-MUQ4-K1JS (the exact eight-verb set) in
  particular still passes, so this change narrows the schema without disturbing
  the verb set or any other Decision.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki`).
