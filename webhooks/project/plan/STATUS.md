# webhooks — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` followed by its zero-padded number, then a status marker — `✅` (done) or
`⬜` (not started) — then `realizes <Decision ids>` (or `realizes —` for a purely
structural phase), then `— <one cohesive objective>`. The build loop finds its
next unit of work with
`grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only that
phase's `project/plan/phase-NN.md`, and on completion flips that one marker
`⬜ → ✅`. A phase body file carries no marker of its own. This document
deliberately carries **no** bare status glyph outside the phase lines, so the
anchored grep matches only phase lines.

Phase 01 ✅ realizes D2 — Module scaffold & data model (migrations + Store)
Phase 02 ✅ realizes D3 — Webhook identity & secret lifecycle (Service/Clock seam, Create/Rotate)
Phase 03 ✅ realizes D5 — Event production (durable-before-ack Record)
Phase 04 ✅ realizes D4 — Public ingress endpoint /in/<name>
Phase 05 ⬜ realizes D6 — MCP tool surface (the four owner tools)
Phase 06 ⬜ realizes D1 — Composition root & chassis boot
Phase 07 ⬜ realizes D7, D8 — nginx fragment, dev-harness wiring & e2e/onboarding
