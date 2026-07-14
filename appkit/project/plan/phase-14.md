# Phase 14 — The loopback-only route class

*Realizes design Decision 12 (R-X0MQ-MNXN, R-X1UN-0FOC, R-X32J-E7F1,
R-X4AF-RZ5Q). Depends on no new phase — it extends the settled
`appkit/server` route table (prior art from before this design) and is
independent of Phases 12–13.
⛔ Externally ordered: eventplane's redundant inline feed guard
(`outbox/feed.go`) is deleted in eventplane's own change, and each service
replaces its hand-copied guard with `HandleLoopback` in its adoption phase —
neither happens here (scope boundary).*

Observable end state, all in `appkit/server`:

- `LoopbackOnly(next http.Handler) http.Handler` exists: a request carrying
  `X-Forwarded-Proto` (any non-empty value) is answered with a bare `404`
  without invoking `next`; all other requests pass through — identity
  headers are not consulted.
- `Router.HandleLoopback(pattern string, h http.Handler)` registers `h`
  wrapped in `LoopbackOnly` on the real route table.
- The `opts.Feed` mount in `server.New` is wrapped in `LoopbackOnly`.

**Done when:** the suite is green (design Conventions: `go build`, `go vet`,
`gofmt -l .` empty, `go test`, from `appkit/`) — and R-X0MQ-MNXN, R-X1UN-0FOC,
R-X32J-E7F1, R-X4AF-RZ5Q are each covered by a clearly-named `httptest` test
through the real handler/mux (recording inner handlers proving
invoked/not-invoked, the `HandleLoopback` and `/feed` claims driven through a
real `server.New` server).
