# Phase 3 — State the landing-page truth in cron's doctrine

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the doctrine claims it).*

cron now serves a human web landing page (Phases 1–2), so cron's doctrine should
say so. **Read D5 for the cron-specific delta from the crm template:** unlike crm
(which *purges* a stale "no UI" line from `crm/AGENTS.md`), cron has **no
`AGENTS.md`/`CLAUDE.md`** and **no "no UI" claim anywhere** — its doctrine prose
lives in the `cmd/cron/main.go` package-doc header. So this phase is **additive
truth-stating, not a purge**: extend that header to record the landing page.

**What gets changed (a doc comment only — no Go logic, no nginx):**

- **`cron/cmd/cron/main.go`** — the `package main` doc-comment block at the top of
  the file. It currently frames cron as machine-only: "the loopback-only
  scheduled-event-emitter service behind nginx … performs no token logic of its
  own," then lists the surface it wires (crontab CRUD MCP tools, the tick worker,
  the LIVE Publishes provider). Extend it, in cron's own voice, to state that cron
  serves an **MCP surface for agents** (bearer-gated) **and a human web landing
  page** (dashboard-session-cookie-gated) under `/srv/cron/`; it still runs **no
  token logic of its own** — nginx remains the sole trust boundary for both doors.
  Keep the "no token logic" statement (still true); add only the landing-page fact.
- Edit **only** the doc comment. The `Spec`, the hooks, the crontab `Store`, the
  `Publishes` provider, and the tick `Producer`/`Workers` wiring are unchanged.
  This phase adds **no** code and changes **no** behavior (the `GET /{$}` route
  itself landed in Phase 1). Add no changelog, no "previously," no footnote.

**Done when (structural content check — no id-tagged test):**

- `cron/cmd/cron/main.go`'s package-doc header states the landing-page truth: it
  mentions that cron serves a human web **landing page** under `/srv/cron/`
  alongside its MCP surface.
- `grep -rni "no UI" cron/` continues to find nothing (no stale machine-only-only
  claim is introduced).
- The suite is green: `cd cron && go build ./...`, `cd cron && go vet ./...`,
  `cd cron && gofmt -l .` (prints nothing), `cd cron && go test ./...`, and
  `bin/check-migrations cron` (a comment-only change leaves all of these green).
