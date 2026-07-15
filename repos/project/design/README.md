# repos — Design

**Authority: shape and its proof.** This directory owns *how* the repos
service is built and *how each behavior is proven* — seams, interfaces, types,
naming, and the test plan. Product (`project/product/README.md`) owns the why
and the promises; design states the exact, checkable form of those promises
and never re-declares the why. Design uses the product's contractual constants
(bot identity `@ikibot`, label set `execute`/`executing`/`failed`/`discuss`,
port 3007, mount `/srv/repos/`, starting version `v0.1.0`) by value but does
not own them. This is the single current statement of the architecture,
rewritten in place; history lives in the plan.

## Requirement ids

Each Decision ends with a **Verification** list — the concrete behaviors that
Decision requires. Every item carries a minted `R-XXXX-XXXX` id: a stable,
unique handle for that one behavior. The ids live inline in these lists and
nowhere else (no separate requirements document). Design's responsibility for
ids ends at minting them — how coverage is measured and when the work is
"done" are downstream's concern and are not specified here.

## Conventions

- **Language / module:** Go (`go 1.26`); module path `repos`, a standalone
  module at `repos/`, on the `appkit` chassis over SQLite
  (`modernc.org/sqlite`, pure-Go, no cgo). In-repo libraries via committed
  `replace` directives (`appkit => ../appkit`, `eventplane => ../eventplane`)
  plus `require registry` (same pattern); the agent engine via the pinned
  tagged module `github.com/ikigenba/agentkit` (v0.2.0 line, matching
  prompts).
- **Build / typecheck:** `cd repos && go build ./...` and `go vet ./...`.
  Production binary via `bin/ship repos` (`CGO_ENABLED=0 GOOS=linux
  GOARCH=amd64 GOWORK=off`).
- **Test:** `cd repos && go test ./...`. **"The suite is green" means:**
  `go build ./...`, `go vet ./...`, and `go test ./...` all exit 0 with no
  failures, and `gofmt -l .` prints nothing — all from `repos/`.
- **Test substrates:** real temp-file SQLite through the embedded migration
  set; the **real `git` binary** against local fixture remotes
  (`git init --bare` in `t.TempDir()`, `file://` URLs) — never a mocked git;
  suite peers (github, webhooks) as `httptest` stubs that record requests; a
  deterministic injected clock; no live network I/O in unit tests. The live
  end-to-end proof runs against `bin/start`.
- **DB / migrations:** ordered, immutable SQL in `internal/db/migrations/`,
  embedded, applied forward-only by the appkit runner. New migrations only via
  `bin/create-migration repos <name>`; numbers never hand-picked, committed
  migrations never edited.
- **Config:** env only, prefix `REPOS_`, read at the composition root, never
  below it. The set: `REPOS_PROVIDER` (default `anthropic`), `REPOS_MODEL`
  (default `claude-opus-4-8`), `REPOS_SESSION_TTL` (default `30m`),
  `REPOS_MAX_SESSIONS` (default `2`), `REPOS_GITHUB_HOOK` (default `github`),
  `REPOS_BOT_LOGIN` (default `ikibot[bot]`), `REPOS_GITHUB_ORG` (default
  `ikigenba`), `REPOS_WORKTREE_TTL_DAYS` (default `14`), plus the provider API
  keys (`ANTHROPIC_API_KEY` et al.) that agentkit providers need.
- **Peers by name, addresses from the registry:** the service names its peers
  in code (`webhooks` as event source, `github` as the GitHub actor) and asks
  `registry` where they live (`registry.MustPort("repos")`,
  `registry.BaseURL("github")`). No `127.0.0.1:30xx` literal in source.
- **Time / IO:** time enters through a `Clock` seam; the DB handle is the
  appkit single-writer `*sql.DB` (`rt.DB()`), shared with the producer outbox.
- **Trust posture:** identity arrives as nginx-injected (or
  loopback-asserted) `X-Owner-Email`/`X-Client-Id`; the service accepts them
  blindly (suite convention). The session agent's isolation is
  worktree-cwd + path-confined file tools, the same single-owner-box posture
  as prompts/scripts — not a security sandbox.

## Layout

`INDEX.md` is the manifest; `DNN.md` is one self-contained file per Decision
(zero-padded; referenced in prose and the plan as `D<N>`); this README holds
only the spine. Design is rewritten in place: a changed Decision is rewritten
in its `DNN.md` and `INDEX.md` is regenerated; a new Decision adds a `DNN.md`
and an INDEX entry.
