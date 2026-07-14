# Phase 5 — Delete the inline feed guard; chassis owns loopback-only

*Realizes design Decision 5 (feed guard ownership moves to the chassis).
Depends on Phase 2.*

Remove the defence-in-depth header check from `outbox.FeedHandler()`: the
block near the top of the returned handler that 404s any request carrying
`X-Owner-Email` or `X-Forwarded-Proto`, and the doc-comment paragraph on
`FeedHandler` that describes it. The handler streams for any request that
reaches it; the loopback-only property of `/feed` is owned by the appkit
chassis mount (`LoopbackOnly`), outside this module. Delete the old
`TestFeed_RejectsIdentityHeaders` test with the guard and replace it with the
D5 verification: the bare handler, mounted directly in an `httptest.Server`,
serves the SSE stream normally to a request carrying those headers. Nothing
else in the feed path changes — frames, control events, cursors, resync, and
keepalive behavior are untouched.

**Done when:**

- R-Z8Y5-5R0C — a request to the bare `FeedHandler` (real `httptest.Server` +
  real HTTP client) carrying `X-Owner-Email` and/or `X-Forwarded-Proto` gets
  200 `text/event-stream` and receives an appended event's frame, covered by a
  test citing the id.
- Run from `eventplane/`,
  `grep -rn --exclude='*_test.go' -e 'X-Owner-Email' -e 'X-Forwarded-Proto' outbox/`
  prints nothing (the new test itself sends these headers, so test files are
  excluded; non-test source must not mention them).
- The suite is green: `go test ./...` and `go vet ./...` from `eventplane/`
  exit 0, and `gofmt -l .` prints nothing.
