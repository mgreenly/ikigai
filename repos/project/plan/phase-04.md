# Phase 4 — The issue protocol

*Realizes design Decision 6 (github peer client, label lifecycle,
`.ikibot/check` gate, PR creation). Depends on Phase 3.*

`internal/repos/ghpeer.go` — the thin loopback JSON-RPC client over the
github service's `/mcp` (verbs `issue_get`, `issue_comments`,
`issue_comment`, `label_add`, `label_remove`, `pr_create`), asserting
`X-Owner-Email`/`X-Client-Id` per call. `internal/repos/protocol.go` — the
runner-side lifecycle: admission ack (`execute`→`executing` + session-id
comment; `queued` comment at enqueue when gated), the `.ikibot/check` gate
executed by the runner with output to `check.log`, success (push, `pr_create`
with `Fixes #N` + session id + check summary, PR-link comment, label
cleanup), failure (branch pushed when commits exist, `failed` + reason/check
output), the no-commits failure, retry attempts landing on `.2` branches,
and label-free manual sessions. Tests run against a recording `httptest`
stub of the github `/mcp` wire shape plus real git fixtures and the Phase 3
engine with a scripted provider.

**Done when:** R-FDAF-MVIC, R-FEIC-0N91, R-FFQ8-EEZQ, R-FGY4-S6QF,
R-FI61-5YH4, R-FKLT-XHYI, and R-FLTQ-B9P7 are each covered by a
clearly-named test, and the suite is green per design Conventions.
