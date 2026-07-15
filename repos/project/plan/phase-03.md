# Phase 3 — The session engine

*Realizes design Decision 5 (worktree-per-session runner, confined toolset,
queue/cap/TTL, model config). Depends on Phase 2.*

Copy and adapt prompts' engine pattern into `internal/runner` +
`internal/tools`: the `Runner` with `Enqueue`/`Cancel`/`Dispatch`/`Recover`,
FIFO admission under the global cap (`REPOS_MAX_SESSIONS`, default 2) and the
one-active-session-per-repo gate, per-session `context.WithTimeout`
(`REPOS_SESSION_TTL`, default 30m), terminal classification (user cancel >
TTL > engine error > success), and boot recovery. Session setup builds the
worktree at `state/sessions/<id>/worktree` on `ikibot/issue-<N>[.k]` (or
`ikibot/session-<id>`) off the freshened default tip, pins `instructions.md`
before the conversation starts, and writes the agentkit stream-json
transcript to `output.jsonl`. The toolset is exactly Bash/Read/Write/Edit/
Glob/Grep confined to the worktree — no Fetch, no share tools, no MCP.
Boot-time model validation checks `REPOS_PROVIDER`/`REPOS_MODEL` (default
`anthropic`/`claude-opus-4-8`) against agentkit's pricing table and the
provider API key. Tests drive a scripted fake provider over real git
fixtures; the GitHub-facing protocol steps are Phase 4's (stub the protocol
seam here).

**Done when:** R-F4R4-YHBH, R-F5Z1-C926, R-F76X-Q0SV, R-F8EU-3SJK,
R-F9MQ-HKA9, R-FAUM-VC0Y, and R-FC2J-93RN are each covered by a
clearly-named test, and the suite is green per design Conventions.
