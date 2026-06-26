# Phase 13 — Docs purge: correct the stale "MCP-only" surface line, state the landing-page truth

*Realizes design Decision 10 (the stale-doc consequence). **Structural / docs
phase — no R-ids.** Edits only the package doc comment of
`prompts/cmd/prompts/main.go` (a comment, not behavior); touches no Go behavior,
no nginx, no migration. Naturally sequenced after Phases 11–12 so the doc flips
true only once the page and its gate exist.*

prompts has **no** service-level `AGENTS.md`/`CLAUDE.md` — its authoritative
surface-posture doctrine is the **package doc comment of
`cmd/prompts/main.go`**. That comment describes what `main.go` wires through the
Handlers hook and ends the enumeration with "…the boot-time crash-recovery sweep,
and **the bare MCP surface**." With Phases 11–12 landed, that enumeration is
**stale**: `registerRoutes` now also mounts a session-gated human **landing
page** (and its `/static/` asset route). Per the suite doctrine — docs state
current truth; purge what the change falsified, rewrite in place, no archaeology,
no footnotes — correct that clause.

- **Purge the MCP-only framing.** Rewrite the surface enumeration so it states the
  current truth: `main.go` wires the prompt store, sandbox tree, async runner,
  the crash-recovery sweep, the bare MCP surface, **and** the session-gated human
  landing page (service name + version, Carbon-styled) served ungated in-process
  at the mount root.
- **Keep "performs no token logic of its own" — it is still true.** The landing
  page reads no token and runs no token logic; nginx remains the sole trust
  boundary (the cookie gate is `/_session-authn`, owned by the dashboard, asserted
  in nginx — not in prompts).
- Do **not** rewrite the rest of the comment (the chassis description, the
  event-plane producer/consumer notes, the secret-handling note); this is a
  single-clause correction, not a rewrite. The edit is comment-only — no Go
  behavior changes, so the suite stays trivially green.

**Done when:** the Go suite stays green (this phase changes no Go behavior) **and**
the named structural check passes:

- `cmd/prompts/main.go`'s package doc comment no longer presents the wired domain
  surface as MCP-only ("the bare MCP surface" as the terminal item), and names the
  landing page (a human web page served beside MCP, session-gated).
- The "performs no token logic of its own" / "nginx is the trust boundary"
  statement remains intact (it is still true).
- `cd prompts && go build ./... && go vet ./... && gofmt -l . && go test ./...`
  and `bin/check-migrations prompts` remain green.

(Structural phase: "Ids to cover" is **(none — structural phase)**; the build
loop verifies the green suite plus the docs check above, not an `R-id` test.)
