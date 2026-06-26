# Phase 3 — Purge the stale "no UI" line; state the landing-page truth

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

ledger now serves a human web landing page (Phases 1–2), so the doctrine docs'
"**no UI**" claim is false. Rewrite it in place to the current truth — no
changelog, no footnote, no "previously."

**What gets changed (docs only — no Go, no nginx):**

- **`ledger/AGENTS.md`** (the source file; `ledger/CLAUDE.md` is a symlink to it —
  edit `AGENTS.md`, the refusal to write through the symlink is expected). Remove
  the "**no UI**" claim wherever it appears:
  - the opening one-liner ("A pure MCP API with **no UI** and **no token
    logic**…");
  - any other place the "no UI" phrasing recurs (e.g. the "What this app is"
    paragraph, if present).
- Replace it with ledger's current truth, in its own voice: ledger serves an **MCP
  surface for agents** (bearer-gated) **and a human web landing page**
  (dashboard-session-cookie-gated) under `/srv/ledger/`; it still runs **no token
  logic** — nginx remains the sole trust boundary for both doors. Keep the "no
  token logic" statement (still true); drop only the "no UI" half.
- Touch nothing else in the doc: the immutable-journal eight-verb MCP domain, the
  header-trust model, the loopback bind, and the deploy/manifest sections are
  unchanged and still true.

**Done when (structural content check — no id-tagged test):**

- `grep -i "no UI" ledger/AGENTS.md` finds nothing (the false claim is gone from
  the file, and hence from `ledger/CLAUDE.md` via the symlink).
- `ledger/AGENTS.md` states the landing-page truth: it mentions that ledger serves
  a human web **landing page** under `/srv/ledger/` alongside its MCP surface.
- The `AGENTS.md`/`CLAUDE.md` symlink is intact (one file; `ledger/CLAUDE.md` still
  resolves to `ledger/AGENTS.md`).
- The suite is green: `cd ledger && go build ./...`, `cd ledger && go vet ./...`,
  `cd ledger && gofmt -l .` (prints nothing), `cd ledger && go test ./...`, and
  `bin/check-migrations ledger` (a docs-only change leaves all of these green).
