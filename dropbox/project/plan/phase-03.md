# Phase 3 — Purge the stale "no UI" line; state the landing-page truth

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

dropbox now serves a human web landing page (Phases 1–2), so the doctrine doc's
"**no UI**" claim is false. Rewrite it in place to the current truth — no
changelog, no footnote, no "previously."

**What gets changed (docs only — no Go, no nginx):**

- **`dropbox/CLAUDE.md`** — a single regular file (dropbox has **no `AGENTS.md`
  symlink**, unlike crm; edit `dropbox/CLAUDE.md` directly). Remove the "**no
  UI**" claim from the opening one-liner ("A loopback-only **daemon + event-plane
  producer** (not an API wrapper) with **no UI** and **no token logic of its
  own**…"). The "no UI" string appears only there in the file.
- Replace it with dropbox's current truth, in its own voice: dropbox is a
  loopback-only mirror-sync daemon + event-plane producer that serves an **MCP
  surface for agents** (bearer-gated) **and a human web landing page**
  (dashboard-session-cookie-gated) under `/srv/dropbox/`; it still runs **no token
  logic of its own** — nginx remains the sole trust boundary for both doors. Keep
  the "no token logic of its own" statement (still true); drop only the "no UI"
  half.
- Touch nothing else in the doc: the mirror-sync daemon model, the four-tool MCP
  surface, the event-plane producer edges, the loopback `/content`/`/list` byte
  routes, the header-trust model, the loopback bind, the no-backup decision, and
  the deploy/manifest sections are unchanged and still true.

**Done when (structural content check — no id-tagged test):**

- `grep -i "no UI" dropbox/CLAUDE.md` finds nothing (the false claim is gone from
  the file).
- `dropbox/CLAUDE.md` states the landing-page truth: it mentions that dropbox
  serves a human web **landing page** under `/srv/dropbox/` alongside its MCP
  surface.
- The suite is green: `cd dropbox && go build ./...`, `cd dropbox && go vet ./...`,
  `cd dropbox && gofmt -l .` (prints nothing), `cd dropbox && go test ./...`, and
  `bin/check-migrations dropbox` (a docs-only change leaves all of these green).
