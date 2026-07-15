# Phase 6 — Outcome events & state retention

*Realizes design Decision 8 (session-outcome families) and Decision 9 (state
layout & retention). Depends on Phase 3.*

`internal/repos/events.go`: the two-family registry
(`session.succeeded`/`session.failed`, subject `/<repo name>`, reference-only
payload) and the `FinishSession` pairing that appends the outcome event on
the terminal write's transaction, `Ring()` after commit — cancelled sessions
emit `session.failed` with error `cancelled`. `internal/repos/reaper.go` and
the runner hooks: immediate worktree prune on success, crime-scene retention
on failure, supersession pruning when a later attempt on the same issue
succeeds, the `REPOS_WORKTREE_TTL_DAYS` age sweep (worktrees only,
transcripts never), and repo-deletion cleanup that keeps session rows and
transcript files readable. Tests: real outbox over migrated SQLite, the real
`FeedHandler` over `httptest`, real worktrees on fixtures, an injected clock
for the sweep.

**Done when:** R-FT54-LW5D, R-FUD0-ZNW2, R-FVKX-DFMR, R-FWST-R7DG,
R-FY0Q-4Z45, and R-G0GI-WILJ are each covered by a clearly-named test, and
the suite is green per design Conventions.
