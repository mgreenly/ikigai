# opsctl — Plan Status

This is the manifest: one line per phase in build order, and the **only** place a
phase's status marker lives. Each phase line is a Markdown bullet beginning with
`- Phase`, carrying `✅` (done) or `⬜` (not started). The build loop finds its
next work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`, reads
only that phase's `project/plan/phase-NN.md`, and on completion flips that one
marker. This file deliberately carries **no bare status glyph**, so the anchored
grep matches only phase lines.

- Phase 01 ✅ realizes R-WP3M-PO1V, R-WQBJ-3FSK — Restore recreates `cache/` owned by the service user
- Phase 02 ✅ realizes R-65MT-7QEK — Stage unpacks on the OPSCTL_ROOT filesystem (no cross-device rename)
- Phase 03 ✅ realizes R-6AIE-QTDC, R-6BQB-4L41, R-6CY7-ICUQ — opsctl loads the box env file at startup
- Phase 04 ⬜ realizes R-MSOP-5MDA, R-MTWL-JE3Z, R-MV4H-X5UO, R-MXKA-OPC2 — deploy renders + installs the apex block for the DEFAULT app (R-MYS7-2H2R live-box, operator-verified out-of-loop)
