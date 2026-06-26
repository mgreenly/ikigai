# Phase 3 — Purge the stale "no UI" line; state the landing-page truth

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

gmail now serves a human web landing page (Phases 1–2), so the doctrine's
"**no UI**" / machine-surfaces-only framing is false. Rewrite it in place to the
current truth — no changelog, no footnote, no "previously."

**What gets changed (docs only — no Go, no nginx):**

- **`gmail/AGENTS.md`** (the source file; `gmail/CLAUDE.md` is a symlink to it —
  edit `AGENTS.md`, the refusal to write through the symlink is expected). gmail's
  service doctrine is where it states its identity (the file every sibling service
  carries). Remove the "**no UI**" / "serves only machine surfaces" / "no human web
  page" claim wherever it appears in that doctrine.
- Replace it with gmail's current truth, in its own voice: gmail is a loopback-only
  Gmail connector that serves an **MCP surface for agents** (bearer-gated) **and a
  human web landing page** (dashboard-session-cookie-gated) under `/srv/gmail/`,
  with its History-API poll daemon and `mail.*` event producer unchanged; it still
  runs **no token logic** — nginx remains the sole trust boundary for both doors.
  Keep the "no token logic" statement (still true); drop only the "no UI" half.
- Touch nothing else in the doc: the normal-mailbox MCP tool surface, the poll
  daemon, the `mail.*` event producer, the header-trust model, the loopback bind,
  and the deploy/manifest sections are unchanged and still true.
- The `cmd/gmail/main.go` package comment is Go (owned by the connector design)
  and is **not** edited by this phase — only the doctrine doc.

**Done when (structural content check — no id-tagged test):**

- `grep -i "no UI" gmail/AGENTS.md` finds nothing (the false claim is gone from the
  file, and hence from `gmail/CLAUDE.md` via the symlink).
- `gmail/AGENTS.md` states the landing-page truth: it mentions that gmail serves a
  human web **landing page** under `/srv/gmail/` alongside its MCP surface.
- The `AGENTS.md`/`CLAUDE.md` symlink is intact (one file; `gmail/CLAUDE.md` still
  resolves to `gmail/AGENTS.md`).
- The suite is green: `cd gmail && go build ./...`, `cd gmail && go vet ./...`,
  `cd gmail && gofmt -l .` (prints nothing), `cd gmail && go test ./...`, and
  `bin/check-migrations gmail` (a docs-only change leaves all of these green).
