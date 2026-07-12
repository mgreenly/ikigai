# Phase 14 — Attachment content endpoint: `Client.AttachmentGet` + loopback `GET /attachment` + nginx 404 fragment

*Realizes design Decision 16 (attachment content endpoint). Depends on Phase 12
(the converted service shape this builds on). Covers `R-WVZH-M0IY`,
`R-WX7D-ZS9N`, `R-WYFA-DK0C`, `R-WZN6-RBR1`, `R-X0V3-53HQ`, `R-X22Z-IV8F`.*

Observable end state:

- `internal/gmail/client.go` gains `AttachmentGet(ctx, messageID, attachmentID)
  ([]byte, error)`: `GET /messages/{id}/attachments/{aid}` (path segments
  URL-escaped) with the bearer through the existing `rpcCall` machinery, the
  response's URL-safe base64 `data` decoded (padded `base64.URLEncoding`, falling
  back to `RawURLEncoding`); empty ids → `ErrValidation` with no HTTP call; a
  Google 404 → `ErrNotFound`. Tested on the client's existing offline stub seam.
- `internal/gmail/attachment.go` holds the `AttachmentSource` interface
  (`MessageGet` + `AttachmentGet`) and `AttachmentHandler(src) http.Handler`,
  per D16's flow: identity-header guard first (`X-Owner-Email` or
  `X-Forwarded-Proto` present → 404), blank/missing `message_id`/`attachment_id`
  → 404, `MessageGet(id, "full")` + recursive MIME-tree walk matching
  `Body.AttachmentID` (miss or `ErrNotFound` → 404), other upstream errors →
  502, hit → 200 with the decoded bytes, `Content-Type` from the matched part's
  mime type (`application/octet-stream` fallback) and exact `Content-Length`.
- `cmd/gmail/main.go` `Handlers` mounts `rt.Handle("GET /attachment",
  gm.AttachmentHandler(client))` — verbatim, **not** behind `RequireIdentity`.
- `gmail/etc/nginx.conf` gains the exact-match
  `location = /srv/gmail/attachment { return 404; }`; every pre-existing
  location is untouched. The `cmd/gmail` nginx content-assertion tests grow the
  matching assertion.

**Done when:** the suite is green (design Conventions commands, from `gmail/`)
and:

- R-WVZH-M0IY, R-WX7D-ZS9N, R-WYFA-DK0C, R-WZN6-RBR1, R-X0V3-53HQ, and
  R-X22Z-IV8F are each covered by a clearly-named test asserting the behavior
  its D16 Verification line states (the guard test must show the same request
  passing without identity headers and 404ing with each of the two headers;
  the 404-mapping tests must show absence → 404 while an upstream fault → 502);
- `grep -n "GET /attachment" gmail/cmd/gmail/main.go` returns exactly one
  match, and that line does not contain `RequireIdentity`;
- `grep -c "location = /srv/gmail/attachment { return 404; }" gmail/etc/nginx.conf`
  prints `1`.
