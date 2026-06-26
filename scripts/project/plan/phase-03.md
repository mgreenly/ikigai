# Phase 3 — State the landing-page surface in scripts's doctrine

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

scripts now serves a human web landing page (Phases 1–2), so its doctrine's
served-surface picture — currently machine-only (PRM, `/health`, bearer-gated
`/mcp`, loopback `/feed`) — is out of date. Bring it current in place — no
changelog, no footnote, no "previously."

> **No "no UI" line to purge.** scripts has **no root `AGENTS.md`/`CLAUDE.md`** and
> carries **no literal "no UI" claim** today (unlike crm/ledger/notify/dropbox/wiki),
> so this phase is purely **additive**: state that scripts now also serves a human
> landing page. If a root `AGENTS.md`/`CLAUDE.md` exists at build time, edit
> `AGENTS.md` (the `CLAUDE.md` symlink refusal is expected) and purge any "no UI" /
> "machine-only" overreach there too.

**What gets changed (docs only — no Go, no nginx):**

- **`scripts/project/notes/README.md`** (the "What scripts is" framing) and, where
  it enumerates served surfaces, **`scripts/project/notes/ARCHITECTURE.md`**: add
  scripts's current truth, in its own voice — scripts is a loopback-only service
  that serves an **MCP surface for agents** (bearer-gated) **and a human web
  landing page** (dashboard-session-cookie-gated) under `/srv/scripts/`, alongside
  its event-plane producer/consumer feeds; it still runs **no token logic** —
  nginx remains the sole trust boundary for both doors.
- Purge only phrasing the change actually **falsifies** (e.g. a served-surface
  list that reads as "machine-only / no human page"). Keep true framing intact:
  scripts runs **non-interactive background work** and you still *delegate* a
  script over MCP rather than chat with a run — the static name+version landing
  page does not change that, so that line stays.
- Touch nothing else: the deterministic `python3`-exec runner, the script/run data
  model, the event-plane producer (`scripts.succeeded`/`scripts.failed`) and
  multi-upstream consumer, the header-trust model, the loopback bind, and the
  deploy/manifest facts are unchanged and still true.

**Done when (structural content check — no id-tagged test):**

- scripts's doctrine (`scripts/project/notes/README.md`, and `ARCHITECTURE.md`
  where it lists surfaces) states the landing-page truth: it mentions that scripts
  serves a human web **landing page** under `/srv/scripts/` alongside its MCP
  surface.
- `grep -i "no UI" scripts/project/notes/*.md` finds nothing, and no served-surface
  enumeration in those docs still reads as machine-only / no human page.
- The suite is green: `cd scripts && go build ./...`, `cd scripts && go vet ./...`,
  `cd scripts && gofmt -l .` (prints nothing), `cd scripts && go test ./...`, and
  `bin/check-migrations scripts` (a docs-only change leaves all of these green).
