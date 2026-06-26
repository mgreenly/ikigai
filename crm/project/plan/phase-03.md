# Phase 3 — Purge the stale "no UI" line; state the landing-page truth

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

crm now serves a human web landing page (Phases 1–2), so the doctrine docs'
"**no UI**" claim is false. Rewrite it in place to the current truth — no
changelog, no footnote, no "previously."

**What gets changed (docs only — no Go, no nginx):**

- **`crm/AGENTS.md`** (the source file; `crm/CLAUDE.md` is a symlink to it — edit
  `AGENTS.md`, the refusal to write through the symlink is expected). Remove the
  "**no UI**" claim wherever it appears:
  - the opening one-liner ("A pure REST + MCP API with **no UI** and **no token
    logic**…");
  - the "What this app is" paragraph ("A loopback-only domain service … **no UI**
    and no token logic …").
- Replace it with crm's current truth, in its own voice: crm serves an **MCP
  surface for agents** (bearer-gated) **and a human web landing page**
  (dashboard-session-cookie-gated) under `/srv/crm/`; it still runs **no token
  logic** — nginx remains the sole trust boundary for both doors. Keep the "no
  token logic" statement (still true); drop only the "no UI" half.
- Touch nothing else in the doc: the five-entity/six-verb MCP domain, the
  header-trust model, the loopback bind, and the deploy/manifest sections are
  unchanged and still true.

**Done when (structural content check — no id-tagged test):**

- `grep -i "no UI" crm/AGENTS.md` finds nothing (the false claim is gone from the
  file, and hence from `crm/CLAUDE.md` via the symlink).
- `crm/AGENTS.md` states the landing-page truth: it mentions that crm serves a
  human web **landing page** under `/srv/crm/` alongside its MCP surface.
- The `AGENTS.md`/`CLAUDE.md` symlink is intact (one file; `crm/CLAUDE.md` still
  resolves to `crm/AGENTS.md`).
- The suite is green: `cd crm && go build ./...`, `cd crm && go vet ./...`,
  `cd crm && gofmt -l .` (prints nothing), `cd crm && go test ./...`, and
  `bin/check-migrations crm` (a docs-only change leaves all of these green).
