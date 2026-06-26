# Phase 3 — Move OAuth grant management onto the profile page

*Realizes design Decision 4 (OAuth grant management moves to the profile page).
Depends on Phase 01 (the profile page and route exist); independent of Phase 02 but
sequenced after it.*

Relocate the live-grants surface — the SSE-backed grants block and per-grant
revoke — from the landing/home page to the profile page, and point the revoke
redirect at `/profile`. The grant capability and its endpoints are unchanged; only
the page that embeds the block and the post-revoke redirect move.

**What gets built (the observable end state):**

- `handleProfile` now populates `Grants` for the signed-in owner via
  `a.oauthTokens.ListChainsByOwner` (non-fatal on a transient list error), and
  `profile.html` renders the `#grants-block` container —
  `data-stream="/grants/stream"`, `data-fragment="/grants/fragment"` — with the
  `grants_block` partial for first paint.
- `handleIndex` **stops** populating `Grants`; the landing branch of the index
  template **removes** the grants section.
- `POST /grants/{public_id}/revoke` redirects to `/profile`
  (`http.StatusSeeOther`) on success instead of `/` (its same-origin +
  `requireSession` guards, not-found-on-mismatch behavior, and grant-change
  `Publish` are unchanged).
- `GET /grants/stream` and `GET /grants/fragment` are **unchanged** — same gate,
  same payloads; only the page hosting the `#grants-block` container moved.

**Done when:**

- R-DB09-GRNT — a test asserts `GET /profile` with a valid session renders the
  live-grants block (the `#grants-block` container with its `data-stream` /
  `data-fragment` hooks and the `grants_block` partial).
- R-DB10-GRVK — a test asserts `POST /grants/{public_id}/revoke` redirects to
  `/profile` (not `/`) on success.
- R-DB11-GRNX — a test asserts `GET /` with a valid session (the landing) contains
  no grants block.
- Tests are co-located in `dashboard/internal/server/*_test.go`, `package server`,
  named for the behavior asserted.
- The suite is green: `cd dashboard && go build ./...`, `go vet ./...`,
  `gofmt -l .` (no output), `go test ./...`, `bin/check-migrations dashboard`.
