# Phase 3 — Purge the stale "no UI" line; state the landing-page truth

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

notify now serves a human web landing page (Phases 1–2), so the doctrine docs'
"**no UI**" claim is false. Rewrite it in place to the current truth — no
changelog, no footnote, no "previously."

**What gets changed (docs only — no Go, no nginx):**

- **`notify/AGENTS.md`** (the source file; `notify/CLAUDE.md` is a symlink to it —
  edit `AGENTS.md`, the refusal to write through the symlink is expected). Remove
  the "**no UI**" claim wherever it appears:
  - the opening one-liner ("A pure MCP API with **no UI** and **no token
    logic**…");
  - any related "no UI" phrasing in the "two planes" / north-south paragraph.
- Replace it with notify's current truth, in its own voice: notify serves an **MCP
  surface for agents** (`send`/`health`/`reflection`, bearer-gated) **and a human
  web landing page** (dashboard-session-cookie-gated) under `/srv/notify/`, plus
  its east/west event-plane consumer loops; it still runs **no token logic** —
  nginx remains the sole trust boundary. Keep the "no token logic" statement
  (still true); drop only the "no UI" half.
- Touch nothing else in the doc: the event-plane consumer model, the
  `send`/`health`/`reflection` surface, the "no `/feed`" (consumer, not producer)
  statement, the header-trust model, the loopback bind, the secrets handling, and
  the deploy/manifest sections are unchanged and still true.

**Done when (structural content check — no id-tagged test):**

- `grep -i "no UI" notify/AGENTS.md` finds nothing (the false claim is gone from
  the file, and hence from `notify/CLAUDE.md` via the symlink).
- `notify/AGENTS.md` states the landing-page truth: it mentions that notify serves
  a human web **landing page** under `/srv/notify/` alongside its MCP surface.
- The `AGENTS.md`/`CLAUDE.md` symlink is intact (one file; `notify/CLAUDE.md` still
  resolves to `notify/AGENTS.md`).
- The suite is green: `cd notify && go build ./...`, `cd notify && go vet ./...`,
  `cd notify && gofmt -l .` (prints nothing), `cd notify && go test ./...`, and
  `bin/check-migrations notify` (a docs-only change leaves all of these green).
