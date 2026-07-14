# Phase 26 — Structured MCP adoption

*Realizes design Decision 23 (structured MCP adoption). Depends on Phase 19
(the eight-tool MCP surface), Phase 22 (reference-based `put`), and Phase 18/25
(the loopback filesystem routes).*

Conform dropbox's MCP and loopback surfaces to the suite structured-MCP contract
(`docs/structured-mcp-design.md`), against the new appkit (`JSONResult` deleted;
`StructuredResult` re-signed; `ErrorResult` typed; shared loopback guard). No
result content changes; the JSON dropbox already emits is formalized.

**What gets built (observable end state):**

- `internal/mcp/tools.go`: every domain success result (`list`, `get`, `put`,
  `mkdir`, `delete`, `move`) returns through `appkitmcp.StructuredResult(v)` — the
  same value map as today — with the returned marshal error propagated as the
  handler's `error`, not swallowed. Each tool gains an `OutputSchema` literal (a
  small private helper paralleling the existing `obj`/`descTyp`) mirroring the
  keys/types it emits (D23 §3). Errors go through `toolErr` → the typed
  `appkitmcp.ErrorResult(code, msg)`; the size-cap sites use `ErrTooLarge` and the
  `source_url` unavailability site uses `ErrSourceUnavailable`. The hand-built
  `toolErrorJSON` helper and the `{error:{code,message}}` body are deleted.
- `cmd/dropbox/main.go`: the seven loopback filesystem routes (`GET`/`PUT`/
  `DELETE /content`, `POST /mkdir`, `POST /move`, `GET /list`, `GET /stat`) mount
  via `rt.HandleLoopback` instead of `rt.Handle`.
- `internal/dropbox/{content.go,list.go,write.go}`: the inline
  `X-Owner-Email`-or-`X-Forwarded-Proto` predicates and the `loopbackRejected`
  helper are removed; handler bodies keep their existing logic minus the deleted
  first-line rejection.
- Existing D12/D16/D19/D22 tests that asserted the retired `JSONResult`/
  `{error:{code,message}}` shape or the old two-header predicate are updated to
  read the new envelope/guard (they re-prove their own ids on the new substrate).

**Done when** every id below is covered by a named test and the suite is green
per the design Conventions (`cd dropbox && go build ./... && go vet ./... &&
gofmt -l . && go test ./...`, all clean), and both structural greps return empty:

- `grep -rn 'JSONResult' internal cmd --include='*.go' | grep -v _test.go` — empty
  (no `JSONResult` reference remains in service source).
- `grep -rn 'loopbackRejected' internal cmd --include='*.go' | grep -v _test.go` —
  empty (the copied guard helper is gone).

Verification ids:

- R-7PKS-A5KE — each of the six domain tools returns a result carrying both
  `structuredContent` and a mirrored text block equal to that object's JSON
  (`StructuredResult` dual rendering), never text-only.
- R-7QSO-NXB3 — `tools/list` advertises a non-nil `outputSchema` for all six
  domain tools, each an `object` whose `required` keys match the emitted keys
  (`list`→`files`; `get`→six keys; `put`→four; `mkdir`→`path`; `delete`→
  `removed`; `move`→`from,to`); no prose-exception tool omits one.
- R-7S0L-1P1S — a domain-tool error result has the chassis shape (`isError`,
  text = message, `structuredContent` = flat `{code, message}` with a vocabulary
  `code`), not the retired nested `{error:{code,message}}`.
- R-7T8H-FGSH — `toolErr` maps the domain sentinels to typed codes
  (`ErrNotFound`→`not_found`, `ErrRevMismatch`→`conflict`,
  `ErrValidation`/`ErrPathEscape`→`validation`, other→`internal`).
- R-7UGD-T8J6 — the loopback filesystem routes are guarded by the shared chassis
  guard keyed on `X-Forwarded-Proto` only: `X-Forwarded-Proto: https` → bare 404,
  handler does not run (no side effect); the same request without that header but
  with `X-Owner-Email` set is served (the retired predicate would have 404'd it).
