# Phase 15 — Structured MCP adoption for the `send` tool

*Realizes design Decision 17 (structured MCP adoption). Depends on Phase 11
(the `internal/mcp` tool-table seam over `appkit/mcp`).*

Reshape notify's three MCP result sites in `internal/mcp/tools.go` to the
revised `appkit/mcp` surface, formalizing the JSON `send` already emits without
changing any domain behavior:

- `toolSend`'s success return becomes
  `appkitmcp.StructuredResult(map[string]any{"ok": true})`, propagating the
  helper's new `(map, error)` error return honestly (never swallowed).
- Both failure paths adopt `appkitmcp.ErrorResult(code, msg)` with typed codes:
  the three input-validation failures → `appkitmcp.ErrValidation`; an ntfy
  non-2xx or unreachable server → `appkitmcp.ErrSourceUnavailable` (the retired
  local `upstream` code is gone). The local `validationErr`/`upstreamErr`
  helpers and the hand-marshalled `{"error":{…}}` envelope are deleted; the
  fixed non-leaking `source_unavailable` message is preserved.
- The `send` Tool gains an `OutputSchema` literal (object; required boolean
  `ok`) authored through the existing `obj`/`typ` helpers, surfaced by
  `tools/list`.

No guard swap (notify holds no hand-copied loopback guard site — its only
`X-Owner-Email` mention is a comment in `cmd/notify/main.go`). No product
change, no migration, no schema change; only `internal/mcp/tools.go` and its
tests (`tools_test.go`, `send_test.go`) are touched.

**Done when:**

- R-A918-YY6H — a named test drives a successful `send` (mock ntfy accepts) and
  asserts the result carries `structuredContent == {"ok": true}` plus a text
  block that is the same JSON object (a text-only result with no
  `structuredContent` key fails).
- R-AA95-CPX6 — a named test asserts `tools/list` advertises `send` with an
  `outputSchema` that is an object schema whose `properties.ok.type` is
  `"boolean"` and whose `required` contains `"ok"` (a missing `outputSchema`
  key fails).
- R-ACOY-49EK — a named table test drives each invalid `send` (missing/empty
  `message`, out-of-enum `priority`, non-absolute `click`) and asserts the
  error result's `structuredContent.code == "validation"` and that **zero**
  POSTs reached the mock ntfy server.
- R-ADWU-I159 — named tests drive an ntfy non-2xx and an unreachable ntfy
  server and assert the error result's `structuredContent.code ==
  "source_unavailable"` (the string `upstream` must not appear as the code) and
  that the human message contains neither the topic nor the token.
- The notify suite is green per design Conventions:
  `cd notify && go build ./...`, `go vet ./...`, `gofmt -l .` (empty), and
  `go test ./...` all succeed.
- No pre-adoption result token remains in non-test source:
  `cd notify && grep -rn 'JSONResult' internal cmd --include='*.go' | grep -v _test.go`
  returns empty.
