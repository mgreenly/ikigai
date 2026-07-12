# Phase 22 — Reference-based MCP `put` (content-plane `source_url` form)

*Realizes design Decision 19 (slice: the reference-based `put` — R-Q52B-JQLP,
R-Q6A7-XICE, R-Q8Q0-P1TS, R-Q9XX-2TKH; the phase-19 ids R-KRXK-IQE2 /
R-KT5G-WI4R / R-KUDD-A9VG stay owned by phase 19 and must remain green).
Depends on phase 19 (the existing write tools) and phase 18 (the `Service`
write seam).*

Observable end state:

- `internal/mcp`'s `put` accepts **exactly one** of `source_url` /
  `content_base64` (both or neither → `validation`). The `source_url` form
  fetches the stream **server-side** and feeds it to the same `Service.Write`
  seam (mirror write + index/event tx + `upload_queue` enqueue, origin = the
  caller's client id); it carries **no size cap**. The base64 form and its
  25 MiB cap are unchanged.
- Confinement is validated **before any network I/O**: scheme `http`, host
  literally `127.0.0.1` or `::1` (hostnames rejected), and a port for which the
  injected `sourcePortAllowed func(int) bool` is true; any violation is
  `validation` with zero fetch calls.
- The seam threads through the tool table: `Tools(svc, sourcePortAllowed)` /
  `NewHandler(svc, sourcePortAllowed, rt)`; `cmd/dropbox/main.go` passes the
  registry-derived set (every `registry.Services` port, nothing else).
- The pinned failure mapping: source 404 → `not_found`, source 409 →
  `conflict`, connection refused / timeout / other non-2xx / torn body →
  `source_unavailable`; every failure leaves mirror, index, and `upload_queue`
  unchanged. `put`'s description/schema state the two forms and the
  confinement rule.
- `list`/`get`, the other write tools, and the loopback filesystem HTTP API
  are unchanged; the advertised surface stays eight tools.

**Done when:** the suite is green (design Conventions commands, from
`dropbox/`) and:

- R-Q52B-JQLP is covered by a test driving `put(path, source_url)` against a
  **real local `httptest` server** (its port injected via the allowed-port
  seam): the server records exactly one `GET`, a following `get` returns the
  identical bytes, an upload is enqueued, the emitted event's `origin` is the
  caller's client id, and the result is `{path, size, content_hash, rev}`.
- R-Q6A7-XICE is covered by a test asserting the confinement discriminates: a
  non-loopback host, a hostname, and a loopback-but-unallowed port each return
  `validation` with **zero** fetch calls, while a loopback+allowed port
  proceeds; both-or-neither of `source_url`/`content_base64` → `validation`.
- R-Q8Q0-P1TS is covered by a test asserting source 404 → `not_found`,
  source 409 → `conflict`, connection-refused → `source_unavailable`, and that
  after each failure the mirror has no file, the index no row, and
  `upload_queue` no row for the path.
- R-Q9XX-2TKH is covered by a test asserting the composition root's
  registry-derived allowed set returns true for every `registry.Services` port
  and false for 8080 and 39999.
