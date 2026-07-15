# appkit — Research

External ground truth the structured-results design (D8/D9 revisions, D12)
depends on. Non-contractual: design cites these facts; nothing downstream reads
this file mechanically. Rewritten in place.

## MCP specification, revision 2025-06-18 (structured tool output)

The Model Context Protocol's 2025-06-18 revision is the first that carries
structured tool output. The facts the design uses:

- **`outputSchema`** — a tool descriptor may declare an `outputSchema` (a JSON
  Schema object) alongside `inputSchema` in `tools/list`. It describes the
  shape of the tool's structured result and is optional per tool.
- **`structuredContent`** — a `tools/call` result may carry a
  `structuredContent` field: a plain JSON object, sibling to the `content`
  array. When a tool declares an `outputSchema`, its results are expected to
  carry conforming `structuredContent`; servers **should** also return a
  functionally-equivalent text block in `content` for backward compatibility
  with clients that predate the field. `structuredContent` is an optional
  field on any tool result, so carrying it on `isError` results is legal.
- **Error semantics** — the spec distinguishes *protocol* errors (JSON-RPC
  error objects: `-32700` parse, `-32601` method not found, `-32602` invalid
  params — including an unknown tool name, `-32603` internal error) from
  *tool execution* errors, which are reported **inside** a successful JSON-RPC
  response as a result with `isError: true`. Domain failures belong on the
  `isError` channel; protocol-level codes are for transport/dispatch faults.
- **Version negotiation** — the client proposes a protocol version in
  `initialize`; the server replies with the version it speaks; on HTTP
  transports the client then stamps `MCP-Protocol-Version` on subsequent
  requests. Nothing obliges a minimal server to reject a client's newer
  version; replying with its own supported version is the negotiation.
- Later revisions (e.g. 2025-11-25) add capabilities (tasks, elicitation,
  richer transports) that appkit's deliberately minimal plain-POST transport
  does not implement; `2025-06-18` is the lowest revision that carries
  everything this design needs, which is why it is the pinned answer.

## agentkit client compatibility (verified 2026-07-14)

agentkit (`github.com/ikigenba/agentkit`, the prompts service's agent chassis,
separate repo, checkout at `~/projects/agentkit`, v0.2.1) is the one existing
MCP *client* of appkit-served surfaces. Verified against its source:

- `internal/mcp/mcp.go:100` — `CallResult` decodes only `content` and
  `isError` with plain `encoding/json` (no `DisallowUnknownFields`); an added
  `structuredContent` sibling is ignored, and the full raw result JSON is
  retained in `CallResult.Raw`.
- `mcp.go:255` (`mcpResultText`) — the text an in-run agent sees is the join
  of the `content` text blocks; a mirrored-text result renders for agents
  exactly as today's `JSONResult` output does.
- `internal/mcp/mcp.go:85` — `Tool.UnmarshalJSON` extracts only
  `name`/`description`/`inputSchema`; an added `outputSchema` key in
  `tools/list` is ignored.
- `mcp.go:18,194` — agentkit proposes protocol `2025-11-25` and simply adopts
  the version the server returns; it does not reject a server that answers
  with an older revision.

Conclusion: appkit's protocol bump and result-shape additions require **no
agentkit change**. agentkit is a **lenient** client, though — it ignores
`outputSchema` entirely — so verifying against it proves nothing about clients
that *do* validate the advertised schemas. The strict client below is the one
that constrains the schema shapes.

## Strict MCP client schema validation (Claude Code / Anthropic tools API, verified 2026-07-15)

Claude Code is the suite's real end-user MCP *client*, and unlike agentkit it
**strictly validates every advertised `inputSchema`/`outputSchema`** before it
will use a server's tools. The rules the design must satisfy, established from
the Claude Code issue tracker and confirmed against the live box:

- **Draft 2020-12 + a top-level-object constraint.** Every advertised schema is
  checked as JSON Schema draft 2020-12 **and** against the Anthropic tools-API
  rule that the schema's **top level must be a plain object** — `"type":
  "object"` at the root, and **no `oneOf` / `anyOf` / `allOf` at the top
  level**. The API rejects a top-level composition with
  `input_schema does not support oneOf, allOf, or anyOf at the top level`;
  Claude Code's client-side validator likewise skips a tool/server whose schema
  has composition at the root (e.g. "invalid schema (oneOf at top level)").
- **One bad schema fails the whole server.** Validation is all-or-nothing per
  server: a single non-conforming tool descriptor fails the entire `tools/list`,
  and the client reports **`tools fetch failed`**. Because `initialize` carries
  no schemas, it still succeeds, so the server shows **`connected` yet
  `tools fetch failed`** — connected but tool-less. In the worst case a
  malformed schema 400s the whole session.
- **Curl is not a test.** A raw `curl` of `tools/list` returns 200 with a
  well-formed body — curl does no schema validation. Only a strict client
  (Claude Code, MCP Inspector's strict mode, any Zod/JSON-Schema-validating
  consumer) exercises this contract. The live proof is a strict client actually
  fetching the tools, not a 200 from curl.
- **Nested composition may be tolerated**, but appkit deliberately advertises
  **no** `oneOf`/`anyOf`/`allOf` anywhere in its schemas, so chassis correctness
  never rides on that uncertainty.

Evidence: claude-code issues #10606 (strict schema validation introduced
v2.0.21+, top-level `oneOf` server skipped), #10031 / #28620 (client requires a
top-level `"type": "object"`; a top-level `anyOf`/`oneOf` without it is
rejected). Observed 2026-07-15 on `int.ikigenba.com`: **all twelve services**
showed `△ connected · tools fetch failed` in Claude Code while `curl` of the
same `/srv/<svc>/mcp` `tools/list` returned 200 — because the one appkit schema
with a top-level `oneOf` (`reflection`'s `outputSchema`, D9) is present on every
service via the shared chassis. The design blind spot: only agentkit — which
ignores `outputSchema` — had been modeled as a client, so no strict validator
ever saw the emitted schemas until they reached Claude Code in production.

## Error-code vocabulary in live use (suite survey, 2026-07-14)

The string codes services already emit in tool-error text/JSON today:
`validation`, `not_found`, `conflict`, `too_large` (dropbox MCP, dropbox
filesystem API error vocabulary), `source_unavailable` (dropbox `put
source_url`, prompts sandbox Fetch/File* taxonomy). prompts' MCP layer mostly
returns bare `err.Error()` strings (uncoded — the gap the typed vocabulary
closes). No other code word is in suite-wide use; `internal` is added as the
residue code for faults that are no caller's fault.

## The suite contract this design implements

`docs/structured-mcp-design.md` (repo root) is the suite-level design this
appkit work is the root dependency of: structured MCP results as the single
verb surface for agents and machines, caller-asserted identity on loopback,
and the loopback guard narrowed to `X-Forwarded-Proto`. The per-service
adoption and the scripts runtime land through those units' own `project/`
loops and cite that document; this research file exists so appkit's design
does not re-derive the external MCP-spec facts above.
