# prompts agentkit migration — Plan Status

This is the manifest: one line per phase in build order, the **only** place a phase's status marker lives. Each phase line begins with the literal word `Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only that phase's `project/plan/phase-NN.md`, and on completion flips that one marker. This file deliberately carries **no bare status glyph**, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D2, D3 — Config struct and four-provider validation
Phase 02 ✅ realizes D8 — DB migration: backfill provider and model aliases
Phase 03 ✅ realizes D9 — MCP schema expansion
Phase 04 ✅ realizes D5 — Built-in tools package
Phase 05 ✅ realizes D6 — Suite discovery rewrite
Phase 06 ✅ realizes D1, D4, D7 — Module swap, provider factory, and runner rewrite
Phase 07 ✅ realizes D6 (completion) — Vendor MCP client; cut suite.go off local agentkit
Phase 08 ✅ realizes D3 (completion) — Validation via published provider registries
Phase 09 ✅ realizes D7 (completion) — Finish runner off local agentkit (delete buildRequest, copy FramingPrompt)
Phase 10 ✅ realizes D1 (completion) — Pin agentkit v0.1.1, purge local agentkit dependency, add import guard
Phase 11 ✅ realizes D10 — the landing page: a new `internal/web` package (embedded Carbon `tokens.css` + woff2 fonts + `landing.tmpl`) and `LandingHandler(Service, Version)` wired ungated at `GET /{$}` in `cmd/prompts/main.go`'s `registerRoutes`, with a sibling embedded `/static/` asset route — renders name+version, 200 `text/html`, exact-root only, no token required
