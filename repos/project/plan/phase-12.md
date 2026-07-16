# Phase 12 — Route intake through the Enqueuer seam (one session-construction point)

*Realizes design Decision 5 (session engine), slice R-2U0F-NNXH, and the D3
prose alignment. Depends on Phase 03 and Phase 05.*

Webhook intake stops hand-building session rows and constructs every session
through the runner's `Enqueue`. `SessionRequest` moves from `internal/runner`
into `internal/repos` and `internal/runner` aliases it (as it already aliases
`IssueContent`). A new `Enqueuer` interface in `internal/repos` is the seam
intake depends on; `Intake` gains an `enqueuer Enqueuer` field and calls it
instead of `store.InsertSession`, deleting its local id generation, its
`MaxAttempt` call, and its `ikibot/issue-%d-%d` branch string. The composition
root (`cmd/repos/spec.go`) wires the runner as the `Enqueuer` implementation.
The observable end state: a session created by the webhook path is
indistinguishable from one created by the MCP path — it has a non-empty
`LogPath` (`state/sessions/<id>/output.jsonl`), the correct
`ikibot/issue-<N>[.<attempt>]` branch, a `queued` comment when queued behind an
active session, and it is admitted by `Dispatch` on the doorbell rather than
sitting `queued` until the next sweep tick. No schema change, no new migration.

**Done when:**

- R-2U0F-NNXH is covered by a clearly-named test: driving `Intake.Handle` with
  a recorded `issues`/`labeled`/`execute` delivery (through the `Enqueuer` seam
  wired to the runner, over real migrated SQLite, a real git fixture remote, a
  scripted fake agentkit provider, and a recording github peer) yields a
  `queued` row whose `LogPath` is `state/sessions/<id>/output.jsonl` (non-empty)
  and which `Dispatch` admits on the doorbell without the sweep ticker firing,
  after which the fake engine drives it to completion and the transcript is
  opened at that `LogPath`. A variant that reproduces the pre-fix hand-rolled
  insert (empty `LogPath`, no ring) fails the assertion, so the routing is what
  the test turns on. The existing D3 intake tests (R-ERC8-R05U et al.) and D5
  Enqueue/dispatch tests remain green.
- The suite is green per design Conventions (`go build ./...`, `go vet ./...`,
  `go test ./...` exit 0, `gofmt -l .` prints nothing, all from `repos/`).
