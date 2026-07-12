# Phase 15 — Attachment references in `read`/`thread`: `attachment_id` + `content_url`

*Realizes design Decision 17 (attachment references), which also rewrites D10's
`Tools`/`NewHandler` signatures in place. Depends on Phase 14 (the endpoint the
references name). Covers `R-X3AV-WMZ4`, `R-X4IS-AEPT`, `R-X5QO-O6GI`.*

Observable end state:

- `internal/mcp` signatures become `Tools(client Client, contentBase string)`
  and `NewHandler(client Client, contentBase string, rt *appkit.Router)`; an
  empty `contentBase` panics at the `NewHandler` seam like a nil client.
- `renderMessage`/`collectAttachments` stamp, on every attachment part with a
  non-empty `Body.AttachmentID`, the entry fields `attachment_id` (raw) and
  `content_url` = `contentBase + "/attachment?" + url.Values{"message_id",
  "attachment_id"}.Encode()`; `filename`/`size`/`mime_type` are unchanged; a
  filename part with an empty `Body.AttachmentID` carries no `attachment_id`
  and no `content_url` key.
- `cmd/gmail/main.go` composes `contentBase` per D17 (`GMAIL_PORT` env or
  `registry.MustPort("gmail")`, `GMAIL_IP` default `127.0.0.1`) and passes it to
  `mcp.NewHandler` — no `127.0.0.1:3xxx` literal anywhere in Go source,
  including test expectations (the Phase 08 loopback guard stays green).
- The `read` tool description drops "Attachment blob download is not
  supported." and both `read` and `thread` descriptions state that attachment
  entries carry `attachment_id` + a loopback `content_url` for service-side
  byte fetch. All other descriptors/schemas unchanged; the surface stays
  exactly twelve tools (the existing R-9NYN-SVIR test stays green).

**Done when:** the suite is green (design Conventions commands, from `gmail/`)
and:

- R-X3AV-WMZ4, R-X4IS-AEPT, and R-X5QO-O6GI are each covered by a clearly-named
  test through the assembled handler asserting the behavior its D17
  Verification line states (the read test's expected URL is built from
  `registry.MustPort("gmail")` and its attachment id contains a character
  requiring percent-encoding, with the query round-trip asserted);
- `grep -n "not supported" gmail/internal/mcp/tools.go` returns no matches;
- the pre-existing `read`/`thread` behavioral tests and the twelve-tool
  partition test pass with the new constructor signature.
