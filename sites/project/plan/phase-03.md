# Phase 3 — State the standardized landing card in sites's self-description

*Realizes design Decision 5 (docs state current truth). Structural / docs-only —
covers no `R-XXXX-XXXX` ids. Depends on Phases 1–2 (the landing page must exist
before the docs claim it).*

sites now serves a standardized human web landing card at the bare mount root
(Phases 1–2). Unlike the other services in this suite-wide change, **sites has no
`AGENTS.md`/`CLAUDE.md` and no "no UI" claim to purge** — it already documents
itself as a static-website host. Its self-description is the `cmd/sites/main.go`
**package doc comment**, which currently enumerates the PRM doc, the bearer-gated
MCP endpoint, and the public/private static tiers, but not the new dynamic landing
card. This phase **adds** that one truthful sentence — no changelog, no footnote,
no "previously."

**What gets changed (doc/comment only — no behavior):**

- **`sites/cmd/sites/main.go`** — in the **package doc comment** (the `// …` block
  above `package main`), add the dynamic landing card to the enumerated surfaces:
  alongside the PRM doc, the bearer-gated MCP endpoint, and the public/private
  static tiers, sites now also serves a Carbon-styled **human web landing page** at
  the bare mount root `/srv/sites/`, rendered **in-process** and gated by the
  dashboard browser **session** (the same `/_session-authn` gate the private static
  tier already uses).
- This edit touches **only the comment prose**, not any executable code: the
  `appkit.Spec`, the `Handlers` hook body (already grown in Phase 1), and every
  still-true statement (loopback-only, header-trust, "no token logic", "not an
  event-plane producer", the MCP surface) stay exactly as they are.
- There is **no `AGENTS.md`/`CLAUDE.md`** in sites and **no "no UI" string** to
  remove — so this phase removes nothing; it only adds the landing-card sentence.

**Done when (structural content check — no id-tagged test):**

- `cmd/sites/main.go`'s package doc comment mentions that sites serves a human web
  **landing page** at the bare mount root `/srv/sites/`, gated by the dashboard
  **session**, alongside its existing surfaces.
- No "no UI" claim is introduced anywhere, and no changelog/footnote is added.
- The edit is comment-only, so the suite stays green: `cd sites && go build ./...`,
  `cd sites && go vet ./...`, `cd sites && gofmt -l .` (prints nothing),
  `cd sites && go test ./...`, and `bin/check-migrations sites` all remain green.
