# eventplane — Design

**Authority: shape and its proof.** This document set owns *how* the routing
revision is built and *how each behavior is proven* — seams, interfaces,
types, naming, and the test strategy. Product owns the why and the promises;
design states their exact, checkable form and never re-declares the why. It
uses product's contractual constants (the envelope fields, the canonical key
form, the glob dialect) by value but does not own them. This is the single
current statement of the design, rewritten in place; history lives in the
plan.

**Scope note — revision over a baseline.** This spec covers only the routing
revision. The as-built library — outbox atomicity, the SSE transport and
control frames, cursors and the epoch token, all four resync reasons,
reconnect backoff, retention, and the handler-return cursor gate
(nil/ErrSkip/stall) — is the baseline described in `eventplane/CLAUDE.md` and
verified in `project/research/research.md`. Decisions reference that baseline;
they never respecify it, and no Verification id below re-proves it.

## Requirement ids

Each Decision ends with a **Verification** list — the concrete behaviors that
decision requires. Every item carries a minted requirement id (`idgen` prefix
`R`, shape `R-` + two four-character groups): a stable,
unique handle for that one behavior. The ids live inline in those lists and
nowhere else — there is no separate requirements document. Design's
responsibility for ids ends at minting them; how coverage is measured and when
the work is "done" are downstream's concern and are not specified here.

## Conventions

- **Language/module:** Go 1.26, module `eventplane` (packages `outbox`,
  `consumer`, and the new `routing`). Sole direct dependency stays
  `modernc.org/sqlite` — the matcher is hand-rolled; **no new `require` may
  appear in `go.mod`**.
- **Build/vet:** `go vet ./...` run from `eventplane/`; code is `gofmt`-clean
  (`gofmt -l .` prints nothing). Local dev runs in workspace mode via the
  repo-root `go.work` (do not set `GOWORK=off`).
- **Test command:** `go test ./...` run from `eventplane/`.
- **"The suite is green" means:** `go test ./...` from `eventplane/` exits 0
  with every package passing, and `go vet ./...` exits 0.
- **Test substrate rule:** any claim that depends on a real substrate is
  proven on that substrate — DDL claims apply the schema to a real SQLite
  database (`modernc.org/sqlite`); wire claims run the real
  `outbox.FeedHandler()` in an `httptest.Server` with a real HTTP client or
  `consumer.Run` on the other end (the existing `consumer_test.go` pattern).
- **Test naming:** each Verification id is covered by a test that cites the id
  in its name or an adjacent comment, so grepping for the id finds the proof.

## Layout

`INDEX.md` is the manifest: every Decision and every Verification id, with id
lookup a grep away. Each `DNN.md` is one self-contained Decision, referenced
in prose and the plan as `D<N>`. This README holds only the spine. Design is
rewritten in place: a changed Decision is rewritten in its `DNN.md` and
`INDEX.md` is regenerated; a new Decision adds a `DNN.md` and an INDEX entry.

Current Decisions:

- **D1** — Envelope and wire cutover: `kind` + `subject` replace `type`.
- **D2** — The `routing` package: canonical key rendering and the hand-rolled
  matcher.
- **D3** — Producer families: registry, reflection, and filter validation.
- **D4** — Consumer surface: routing fields on `consumer.Event`.
