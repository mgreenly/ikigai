# Phase 2 — Repo lifecycle & git custody

*Realizes design Decision 4 (repo lifecycle & git custody). Depends on
Phase 1.*

`internal/repos/git.go` wraps the real `git` binary as the service's only git
invoker: `Clone`, `Freshen`, `WorktreeAdd`, `WorktreeRemove`, `Push`,
`BranchExists`, rooted at `state/repos/`. A `TokenSource` fetches short-lived
installation tokens from the github service's loopback `GET /token`
(`registry.BaseURL("github")`), cached until near expiry; authenticated
operations inject the token per invocation via `-c http.extraHeader=…`, never
into any file the agent can reach. `Service.EnsureRepo` (idempotent lazy
provisioning from delivery facts) and `Service.CloneRepo` (explicit onboard,
org from `REPOS_GITHUB_ORG`, conflict on duplicates) sit on top. Tests run
the real git binary against local bare fixture remotes in `t.TempDir()` with
a recording `httptest` token stub.

**Done when:** R-EXFQ-NUVB, R-EYNN-1MM0, R-EZVJ-FECP, R-F13F-T63E, and
R-F3J8-KPKS are each covered by a clearly-named test, and the suite is green
per design Conventions.
