# github — Plan Status

This is the manifest: one line per phase in build order, and the **only** place a
phase's status marker lives. Each phase line is a Markdown bullet beginning with
`- Phase`, carrying `✅` (done) or `⬜` (not started). The build loop finds its next
work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only
that phase's `project/plan/phase-NN.md`, and on completion flips that one marker.
This file deliberately carries **no bare status glyph**, so the anchored grep
matches only phase lines.

- Phase 01 ✅ realizes — (structural, D1) — the stateless connector module skeleton on the appkit chassis
- Phase 02 ⬜ realizes R-DLMX-CNDL, R-DO2Q-46UZ, R-DPAM-HYLO, R-DQII-VQCD, R-DRQF-9I32 — the GitHub App installation-token source (offline)
- Phase 03 ⬜ realizes R-DVE4-ETB5, R-DWM0-SL1U, R-DXTX-6CSJ, R-DZ1T-K4J8, R-E09P-XW9X, R-E1HM-BO0M, R-E2PI-PFRB, R-E3XF-37I0, R-E55B-GZ8P, R-E6D7-UQZE, R-E7L4-8IQ3, R-EA0X-027H, R-EB8T-DTY6, R-ECGP-RLOV, R-D0IM-VQ7H — the typed GitHub REST v3 client
- Phase 04 ⬜ realizes R-EEWI-J569, R-EHCB-AONN, R-EIK7-OGEC, R-EJS4-2851, R-EL00-FZVQ, R-EM7W-TRMF, R-ENFT-7JD4 — the MCP tool surface (all verbs + health + reflection + provenance)
- Phase 05 ⬜ realizes R-EPVL-Z2UI, R-ER3I-CUL7, R-ETJB-4E2L — the loopback GET /pr twin for scripts
- Phase 06 ⬜ realizes R-EVZ3-VXJZ, R-EX70-9PAO, R-EYEW-NH1D — the landing page and nginx fragment
