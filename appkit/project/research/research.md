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
agentkit change**.

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
