# Phase 13 — Unmask the true failure reason in complete()

*Realizes design Decision 6 (issue protocol / check gate), slice R-2V8C-1FO6.
Depends on Phase 03 and Phase 04.*

`runner.complete()` stops overwriting an errored run's reason with the commit
verdict. It is reordered so the incoming failure is checked first: when the run
entered `complete()` already `failed` (an engine/setup error such as a
`FetchIssue` failure, or a TTL expiry, carried in `message`), that message is
surfaced verbatim as the session's reason — the branch is pushed only when the
worktree has commits (crime scene), the check gate never runs, and no PR is
created. The `"no commits produced"` verdict is reached only on the clean
success path (`status == Succeeded` on entry) when the agent completed without
error yet left the worktree empty. The observable end state: a session whose
issue fetch (or engine) errored reports that error on the GitHub issue comment
and in its `error` column, instead of the masking `"no commits produced"`. No
schema change, no new migration; `Protocol.Failure` and R-FI61-5YH4's
clean-completion behavior are unchanged.

**Done when:**

- R-2V8C-1FO6 is covered by a clearly-named test: with a scripted engine (or
  `FetchIssue`) that returns an error and produces no commits, over the
  recording `httptest` github stub, the session ends `failed` with the failure
  comment carrying that error verbatim, zero `pr_create` calls reach the peer,
  and no check log is written. A variant asserting the pre-fix behavior
  (reason `"no commits produced"`) fails, so the reordering is what the test
  turns on. The existing R-FI61-5YH4 test (clean completion, no commits →
  `"no commits produced"`) remains green.
- The suite is green per design Conventions (`go build ./...`, `go vet ./...`,
  `go test ./...` exit 0, `gofmt -l .` prints nothing, all from `repos/`).
