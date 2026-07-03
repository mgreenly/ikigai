# registry — Plan Status

This is the manifest: one line per phase in build order, and the **only** place a
phase's status marker lives. Each phase line is a Markdown bullet beginning with
`- Phase`, carrying `✅` (done) or `⬜` (not started). The build loop finds its next
work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only
that phase's `project/plan/phase-NN.md`, and on completion flips that one marker.
This file deliberately carries **no bare status glyph**, so the anchored grep
matches only phase lines.

- Phase 01 ✅ realizes — (structural, D1) — create the standalone zero-dependency `registry` module
- Phase 02 ✅ realizes R-B00K-9JYR, R-B18G-NBPG, R-B2GD-13G5, R-B3O9-EV6U — the service table with typed blocks + guardrail tests
- Phase 03 ⬜ realizes R-B642-6EO8, R-B7BY-K6EX, R-B8JU-XY5M, R-B9RR-BPWB — the resolution API (Port / MustPort / BaseURL)
